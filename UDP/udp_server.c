#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9090
#define BUFFER_SIZE 1024

int main() {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // 1. UDP 소켓 생성 (SOCK_DGRAM 사용)
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 서버 주소 구조체 초기화
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // 모든 인터페이스로부터 수신
    server_addr.sin_port = htons(PORT);

    // 2. 소켓에 IP와 Port 바인딩
    if (bind(server_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("UDP Server listening on port %d...\n", PORT);

    // UDP는 listen/accept 없이 바로 송수신 가능
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        // 3. 클라이언트로부터 데이터 수신 (송신자의 주소 정보가 client_addr에 저장됨)
        ssize_t received_len = recvfrom(server_fd, buffer, BUFFER_SIZE - 1, 0,
                                        (struct sockaddr *)&client_addr, &client_len);
        if (received_len < 0) {
            perror("recvfrom error");
            continue;
        }

        buffer[received_len] = '\0';
        printf("[Client %s:%d] %s\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

        // 4. 수신한 주소(client_addr)로 에코(Echo) 응답 전송
        char reply[BUFFER_SIZE + 64];
        // snprintf(reply, sizeof(reply), "Echo from UDP Server: %s", buffer);

        // sendto(server_fd, reply, strlen(reply), 0,
        //        (const struct sockaddr *)&client_addr, client_len);
    }

    close(server_fd);
    return 0;
}
