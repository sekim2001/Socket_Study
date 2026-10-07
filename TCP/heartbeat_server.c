#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <netinet/tcp.h> // TCP_KEEPIDLE 등을 위한 헤더

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_EVENTS 64
#define MAX_FDS 1024
#define HEARTBEAT_TIMEOUT 15 // 15초 이상 핑 미수신 시 강제 정리
#define EPOLL_TIMEOUT_MS 1000 // 1초(1000ms)마다 깨어나 타임아웃 검사

// 클라이언트 연결 세션 정보 구조체
typedef struct {
    int active;                 // 1: 활성 연결, 0: 비활성
    time_t last_active_time;    // 마지막으로 데이터를 수신한 시각
    char ip[INET_ADDRSTRLEN];   // 클라이언트 IP
    int port;                   // 클라이언트 Port
} client_session_t;

static client_session_t sessions[MAX_FDS];

// 소켓 정리 및 epoll 감시 해제 공통 함수
void disconnect_client(int epoll_fd, int fd, const char *reason)
{
    if (fd < 0 || fd >= MAX_FDS || !sessions[fd].active) return;

    printf("[Disconnected] Client (FD: %d, %s:%d) 해제 사유: %s\n",
           fd, sessions[fd].ip, sessions[fd].port, reason);

    // 1. epoll 감시 목록에서 제거
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    // 2. 소켓 디스크립터 닫기
    close(fd);
    // 3. 세션 정보 초기화
    sessions[fd].active = 0;
}

// [참고용] TCP 소켓 레벨 SO_KEEPALIVE 설정 함수
void enable_so_keepalive(int sock_fd)
{
    int optval = 1;
    socklen_t optlen = sizeof(optval);

    // 1. SO_KEEPALIVE 활성화
    setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen);

    // 2. Linux 세부 옵션: 최초 probe 전송 전 유휴 시간 (초)
    int idle = 5;
    setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));

    // 3. probe 재전송 주기 (초)
    int interval = 3;
    setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));

    // 4. 응답 없을 시 최대 probe 시도 횟수
    int max_count = 3;
    setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPCNT, &max_count, sizeof(max_count));
}

int main()
{
    // ==============================================================
    // 요구사항 1: SIGPIPE 무시 처리
    // 상대가 소켓을 닫았을 때 send/write 호출로 프로세스가 강제 종료되는 것을 방지
    // ==============================================================
    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // epoll 인스턴스 생성
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 서버 리슨 소켓을 epoll 감시 목록에 등록
    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        perror("epoll_ctl: server_fd");
        close(server_fd);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    // 세션 테이블 초기화
    memset(sessions, 0, sizeof(sessions));

    printf("=================================================================\n");
    printf("Heartbeat & SIGPIPE 방어 epoll 서버 시작 (포트: %d)\n", PORT);
    printf("=================================================================\n");

    while (1)
    {
        // 1초(1000ms) 단위로 대기하여 정기적으로 Heartbeat 타임아웃 체크를 수행
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, EPOLL_TIMEOUT_MS);
        if (nfds < 0) {
            if (errno == EINTR) continue; // 시그널 인터럽트 발생 시 재개
            perror("epoll_wait");
            break;
        }

        // [1] 발생한 epoll I/O 이벤트 처리
        for (int i = 0; i < nfds; i++)
        {
            int current_fd = events[i].data.fd;

            // [Case A] 신규 클라이언트 접속
            if (current_fd == server_fd)
            {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);

                int client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                if (client_sock < 0) {
                    perror("accept failed");
                    continue;
                }

                if (client_sock >= MAX_FDS) {
                    fprintf(stderr, "소켓 FD가 최대 허용 수치(%d)를 초과했습니다.\n", MAX_FDS);
                    close(client_sock);
                    continue;
                }

                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, sizeof(client_ip));
                int client_port = ntohs(client_addr.sin_port);

                // 세션 등록 및 초기화
                sessions[client_sock].active = 1;
                sessions[client_sock].last_active_time = time(NULL);
                strncpy(sessions[client_sock].ip, client_ip, sizeof(sessions[client_sock].ip));
                sessions[client_sock].port = client_port;

                // 새 클라이언트 epoll 등록
                ev.events = EPOLLIN;
                ev.data.fd = client_sock;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sock, &ev) < 0) {
                    perror("epoll_ctl: client_sock");
                    close(client_sock);
                    sessions[client_sock].active = 0;
                    continue;
                }

                printf("[Connected] New Client (FD: %d, %s:%d)\n", client_sock, client_ip, client_port);
            }
            // [Case B] 기존 클라이언트로부터의 데이터 수신
            else
            {
                char buffer[BUFFER_SIZE];
                ssize_t bytes_read = read(current_fd, buffer, sizeof(buffer) - 1);

                if (bytes_read > 0)
                {
                    buffer[bytes_read] = '\0';
                    // 마지막 정상 통신 시각 갱신
                    sessions[current_fd].last_active_time = time(NULL);

                    // 끝부분 개행 문자(\r, \n) 제거
                    buffer[strcspn(buffer, "\r\n")] = '\0';

                    // 요구사항 2: Heartbeat (PING 수신 시 PONG 응답)
                    if (strcmp(buffer, "PING") == 0)
                    {
                        printf("[FD: %d] Heartbeat 수신: 'PING' -> 응답 전송: 'PONG'\n", current_fd);
                        const char *pong_msg = "PONG\n";

                        // 요구사항 1: send() 호출 시 MSG_NOSIGNAL 플래그 적용
                        // 상대방이 이미 종료되었어도 SIGPIPE 없이 -1과 EPIPE 에러 반환
                        if (send(current_fd, pong_msg, strlen(pong_msg), MSG_NOSIGNAL) < 0) {
                            if (errno == EPIPE) {
                                disconnect_client(epoll_fd, current_fd, "SIGPIPE 방어: EPIPE(Broken pipe) 감지");
                            } else {
                                disconnect_client(epoll_fd, current_fd, "send() 실패");
                            }
                        }
                    }
                    else
                    {
                        // 일반 데이터 수신 시 에코 응답
                        printf("[FD: %d (%s:%d)] Received: %s\n",
                               current_fd, sessions[current_fd].ip, sessions[current_fd].port, buffer);

                        // 개행 복원하여 에코 송신
                        strcat(buffer, "\n");
                        if (send(current_fd, buffer, strlen(buffer), MSG_NOSIGNAL) < 0) {
                            if (errno == EPIPE) {
                                disconnect_client(epoll_fd, current_fd, "SIGPIPE 방어: EPIPE(Broken pipe) 감지");
                            } else {
                                disconnect_client(epoll_fd, current_fd, "send() 실패");
                            }
                        }
                    }
                }
                else if (bytes_read == 0)
                {
                    // 정상 종료 (FIN)
                    disconnect_client(epoll_fd, current_fd, "클라이언트 정상 종료(FIN)");
                }
                else
                {
                    // 소켓 비정상 오류 (RST 등)
                    disconnect_client(epoll_fd, current_fd, "소켓 읽기 오류(RST/에러)");
                }
            }
        }

        // [2] 요구사항 2: 정기적 Heartbeat 타임아웃 검사 (15초 이상 핑 미수신 클라이언트 강제 정리)
        time_t now = time(NULL);
        for (int fd = 0; fd < MAX_FDS; fd++)
        {
            if (sessions[fd].active && fd != server_fd)
            {
                double elapsed = difftime(now, sessions[fd].last_active_time);
                if (elapsed >= HEARTBEAT_TIMEOUT)
                {
                    char reason[128];
                    snprintf(reason, sizeof(reason),
                             "Heartbeat 타임아웃 (%.0f초 동안 무응답으로 인한 비정상 단절 간주)", elapsed);
                    disconnect_client(epoll_fd, fd, reason);
                }
            }
        }
    }

    close(epoll_fd);
    close(server_fd);
    return 0;
}
