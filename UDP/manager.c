#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "protocol.h"

int main() {
    // 1. UDP 소켓 생성
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket 생성 실패");
        exit(EXIT_FAILURE);
    }

    // 2. SO_BROADCAST 옵션 활성화 (브로드캐스트 전송 허용)
    int broadcast_enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("setsockopt SO_BROADCAST 실패");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // 3. 수신 대기 타임아웃(3초) 설정 (SO_RCVTIMEO)
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt SO_RCVTIMEO 실패");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // 4. 브로드캐스트 목적지 주소 설정 (255.255.255.255:DISCOVERY_PORT)
    struct sockaddr_in bcast_addr;
    memset(&bcast_addr, 0, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(DISCOVERY_PORT);
    bcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); // 255.255.255.255

    printf("==========================================\n");
    printf("[Manager] 장치 탐색(Device Discovery) 시작\n");
    printf(" - 목적지: 255.255.255.255:%d\n", DISCOVERY_PORT);
    printf(" - 요청 메시지: '%s'\n", DISCOVERY_REQ_MSG);
    printf(" - 대기 시간: 3초\n");
    printf("==========================================\n");

    // 5. "DISCOVER_DEVICE" 패킷 브로드캐스트 전송
    ssize_t sent = sendto(sock, DISCOVERY_REQ_MSG, strlen(DISCOVERY_REQ_MSG), 0,
                          (struct sockaddr *)&bcast_addr, sizeof(bcast_addr));
    if (sent < 0) {
        perror("sendto 브로드캐스트 전송 실패");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("브로드캐스트 패킷 전송 완료. 디바이스 응답 수집 중...\n\n");

    // 6. 타임아웃(3초) 동안 응답해 오는 모든 보드의 정보 수집 및 출력
    int device_count = 0;
    while (1) {
        DeviceResponse res;
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);

        ssize_t n = recvfrom(sock, &res, sizeof(res), 0,
                             (struct sockaddr *)&from_addr, &from_len);

        if (n < 0) {
            // 3초 동안 더 이상 응답 패킷이 들어오지 않아 타임아웃 발생
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("\n[탐색 종료] 3초 수신 대기 시간이 만료되었습니다.\n");
                break;
            }
            perror("recvfrom 오류");
            break;
        }

        // 수신된 패킷 크기 유효성 확인
        if (n == sizeof(DeviceResponse)) {
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &from_addr.sin_addr, ip_str, sizeof(ip_str));

            // 요구사항 출력 형식: [발견 장치] IP: 192.168.0.15, 보드 이름: Sensor-Node-01, 상태: OK
            printf("[발견 장치] IP: %s, 보드 이름: %s, 상태: %s (FW: %s)\n",
                   ip_str, res.board_name, res.status, res.fw_version);
            device_count++;
        } else {
            printf("[경고] 비정상적인 크기(%ld bytes)의 패킷을 수신했습니다.\n", n);
        }
    }

    printf("=> 총 %d대의 디바이스를 발견했습니다.\n", device_count);

    close(sock);
    return 0;
}
