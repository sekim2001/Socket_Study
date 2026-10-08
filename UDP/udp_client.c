#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock_fd;
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE];

    // 1. UDP 소켓 생성 (SOCK_DGRAM)
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 서버 주소 구조체 설정
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid server address");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // UDP 클라이언트는 connect() 없이 바로 sendto()로 전송 가능
    int count = 0;
    while (1) {
        snprintf(message, sizeof(message), "UDP Client Msg %d", count++);

        // 2. 서버로 데이터 전송 (목적지 주소 명시)
        sendto(sock_fd, message, strlen(message), 0,
               (const struct sockaddr *)&server_addr, server_len);
        printf("[Sent] %s\n", message);

        // 3. 서버 응답 수신
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t received_len = recvfrom(sock_fd, buffer, BUFFER_SIZE - 1, 0,
                                        (struct sockaddr *)&server_addr, &server_len);
        if (received_len > 0) {
            buffer[received_len] = '\0';
            printf("[Received] %s\n", buffer);
        }

        sleep(1);
    }

    close(sock_fd);
    return 0;
}
