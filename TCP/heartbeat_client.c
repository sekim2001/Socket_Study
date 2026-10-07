#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define PING_INTERVAL 5 // 5초마다 PING 전송

int main(int argc, char *argv[])
{
    // SIGPIPE 무시 처리
    signal(SIGPIPE, SIG_IGN);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("=================================================================\n");
    printf("서버 연결 성공 (127.0.0.1:%d)\n", PORT);
    printf("Heartbeat PING 전송 주기: %d초\n", PING_INTERVAL);
    printf("-----------------------------------------------------------------\n");
    int max_pings = (argc > 1) ? atoi(argv[1]) : -1;
    int ping_count = 0;
    char buffer[BUFFER_SIZE];

    while (1)
    {
        // 테스트 모드: 지정된 횟수만큼 PING을 보낸 후 전송을 중단하여 서버 타임아웃 유도
        if (max_pings > 0 && ping_count >= max_pings)
        {
            printf("\n[Client] 지정된 PING 횟수(%d회) 완료.\n", max_pings);
            printf("[Client] 의도적으로 PING 전송을 중단하고 대기합니다.\n");
            printf("[Client] 15초 후 서버가 이 소켓을 강제 정리하는지 기다립니다...\n");

            // 소켓을 닫지 않은 채로 서버의 close(FIN/EOF)를 대기
            ssize_t ret = read(sock, buffer, sizeof(buffer) - 1);
            if (ret == 0) {
                printf("[Client] 서버가 15초 무응답을 감지하고 연결을 강제 종료했습니다 (EOF 수신).\n");
            } else if (ret < 0) {
                perror("[Client] read error");
            }
            break;
        }

        const char *ping_msg = "PING\n";
        printf("[Client] Sending PING (#%d)...\n", ping_count + 1);

        if (send(sock, ping_msg, strlen(ping_msg), MSG_NOSIGNAL) < 0) {
            if (errno == EPIPE) {
                printf("[Client] 서버 연결이 끊어졌습니다 (EPIPE).\n");
            } else {
                perror("send 실패");
            }
            break;
        }

        ssize_t bytes_read = read(sock, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printf("[Client] 서버가 연결을 종료했습니다 (EOF).\n");
            } else {
                perror("read 실패");
            }
            break;
        }

        buffer[bytes_read] = '\0';
        buffer[strcspn(buffer, "\r\n")] = '\0';
        printf("[Client] Received from Server: %s\n", buffer);

        ping_count++;
        sleep(PING_INTERVAL);
    }

    close(sock);
    return 0;
}
