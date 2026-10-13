#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "protocol.h"

int main(int argc, char *argv[]) {
    const char *board_name = (argc > 1) ? argv[1] : "Sensor-Node-01";
    const char *fw_version = (argc > 2) ? argv[2] : "v1.0.0";

    // 1. UDP 소켓 생성
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket 생성 실패");
        exit(EXIT_FAILURE);
    }

    // 포트 재사용 옵션 설정 (한 PC에서 여러 테스트 디바이스 구동 가능하도록 설정)
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    // 2. 브로드캐스트 수신을 위해 INADDR_ANY 및 브로드캐스트 포트 바인딩
    struct sockaddr_in dev_addr;
    memset(&dev_addr, 0, sizeof(dev_addr));
    dev_addr.sin_family = AF_INET;
    dev_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dev_addr.sin_port = htons(DISCOVERY_PORT);

    if (bind(sock, (struct sockaddr *)&dev_addr, sizeof(dev_addr)) < 0) {
        perror("bind 실패");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("==========================================\n");
    printf("[Device] 디바이스 시작 완료\n");
    printf(" - 보드 이름: %s\n", board_name);
    printf(" - 펌웨어 버전: %s\n", fw_version);
    printf(" - 수신 포트: %d\n", DISCOVERY_PORT);
    printf(" - 탐색 요청(DISCOVER_DEVICE) 대기 중...\n");
    printf("==========================================\n");

    while (1) {
        char recv_buf[128];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // 3. 브로드캐스트 요청 수신 대기
        ssize_t n = recvfrom(sock, recv_buf, sizeof(recv_buf) - 1, 0,
                             (struct sockaddr *)&client_addr, &client_len);
        if (n < 0) {
            perror("recvfrom 실패");
            continue;
        }
        recv_buf[n] = '\0';

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        // 4. "DISCOVER_DEVICE" 요청 메시지 검증
        if (strcmp(recv_buf, DISCOVERY_REQ_MSG) == 0) {
            printf("[요청 수신] IP: %s:%d -> '%s'\n", client_ip, client_port, recv_buf);

            // 5. 자신의 디바이스 정보를 담은 구조체 준비
            DeviceResponse res;
            memset(&res, 0, sizeof(res));
            strncpy(res.board_name, board_name, sizeof(res.board_name) - 1);
            strncpy(res.status, "OK", sizeof(res.status) - 1);
            strncpy(res.fw_version, fw_version, sizeof(res.fw_version) - 1);

            // 6. 요청자의 IP/포트로 유니캐스트 응답 전송
            ssize_t sent = sendto(sock, &res, sizeof(res), 0,
                                  (struct sockaddr *)&client_addr, client_len);
            if (sent < 0) {
                perror("sendto 응답 실패");
            } else {
                printf("[응답 완료] %s:%d 로 디바이스 정보 전송 완료 (%ld 바이트)\n\n",
                       client_ip, client_port, sent);
            }
        }
    }

    close(sock);
    return 0;
}
