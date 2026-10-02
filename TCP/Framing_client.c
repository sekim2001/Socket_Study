#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

const char *message[5] = 
{
    "abc",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "1234567890",
    "TCP Packet Framing Test",
    "Final message for testing custom packet framing!"
};

int main()
{
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0){
        printf("\n Socket creation error \n"); return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // 루프백 주소 변환
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n"); return -1;
    }

    // 서버 연결
    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n"); return -1;
    }

    for (int i = 0;  i < 5; i++)
    {
        uint32_t body_len = strlen(message[i]);
        uint32_t net_len = htonl(body_len);

        send(sock_fd, &net_len, sizeof(net_len), 0);
        send(sock_fd, message[i], body_len, 0);
    }

    close(sock_fd);
    
    return 1;
}