#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>

#define CENTRAL_SERVER_IP   "127.0.0.1"
#define CENTRAL_SERVER_PORT 9090  // 센서 데이터를 수집하는 중앙 서버 포트
#define MY_CONTROL_PORT     8080  // 관리자 제어 명령을 수신할 내 포트
#define MAX_EVENTS          10
#define BUFFER_SIZE         1024

int main() 
{
    // UDP 소켓 생성
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    // 수신 포트(8080) 바인딩 (관리자 제어 명령 수신용)
    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr.sin_port = htons(MY_CONTROL_PORT);

    if (bind(sock_fd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        perror("Bind 실패");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // 3. 센서 데이터를 보고할 중앙 서버 주소 준비
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(CENTRAL_SERVER_PORT);
    inet_pton(AF_INET, CENTRAL_SERVER_IP, &server_addr.sin_addr);

    // 4. epoll 생성 및 sock_fd 등록
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll 생성 실패");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sock_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &ev) < 0) {
        perror("epoll_ctl 실패");
        close(sock_fd);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    int timeout_ms = 1000;   // 기본 송신 주기: 1초 (1000ms)
    int sensor_value = 25;   // 가상 센서 데이터 (온도)

    printf("[센서 노드] 실행 시작 (제어 수신 포트: %d, 송신 주기: %d ms)\n", 
            MY_CONTROL_PORT, timeout_ms);

    struct epoll_event events[MAX_EVENTS];

    while (1) 
    {
        // 5. epoll_wait으로 타임아웃(1000ms) 동안 수신 이벤트 대기
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout_ms);

        if (nfds < 0) {
            if (errno == EINTR) continue; // 시그널 인터럽트 무시
            perror("epoll_wait 오류");
            break;
        }

        // ========================================================
        // [케이스 1] nfds == 0 : 타임아웃 발생 (주기적 데이터 송신)
        // ========================================================
        if (nfds == 0) {
            char send_buf[BUFFER_SIZE];
            snprintf(send_buf, sizeof(send_buf), "SENSOR_DATA temp=%d", sensor_value++);
            
            sendto(sock_fd, send_buf, strlen(send_buf), 0,
                    (struct sockaddr *)&server_addr, sizeof(server_addr));
            printf("[주기 전송] 서버로 센서 데이터 전송: '%s'\n", send_buf);
        }
        // ========================================================
        // [케이스 2] nfds > 0 : 수신 이벤트 발생 (관리자 제어 명령 수신)
        // ========================================================
        else {
            for (int i = 0; i < nfds; i++) {
                if (events[i].data.fd == sock_fd && (events[i].events & EPOLLIN)) {
                    char recv_buf[BUFFER_SIZE];
                    struct sockaddr_in sender_addr;
                    socklen_t addr_len = sizeof(sender_addr);

                    ssize_t n = recvfrom(sock_fd, recv_buf, sizeof(recv_buf) - 1, 0,
                                            (struct sockaddr *)&sender_addr, &addr_len);
                    if (n > 0) {
                        recv_buf[n] = '\0';
                        char sender_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, sizeof(sender_ip));

                        printf("\n[명령 수신] %s:%d 로부터 도착: '%s'\n",
                                sender_ip, ntohs(sender_addr.sin_port), recv_buf);

                        // 명령 파싱 예시: "INTERVAL 500"
                        if (strncmp(recv_buf, "INTERVAL ", 9) == 0) {
                            int new_interval = atoi(recv_buf + 9);
                            if (new_interval > 0) {
                                timeout_ms = new_interval;
                                printf(" -> 전송 주기가 %d ms 로 변경되었습니다!\n\n", timeout_ms);
                            }
                        } else if (strcmp(recv_buf, "LED ON") == 0) {
                            printf(" -> [LED 상태] ON 켜짐\n\n");
                        } else if (strcmp(recv_buf, "LED OFF") == 0) {
                            printf(" -> [LED 상태] OFF 꺼짐\n\n");
                        }
                    }
                }
            }
        }
    }

    close(sock_fd);
    close(epoll_fd);
    return 0;
    }