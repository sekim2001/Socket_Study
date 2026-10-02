#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>

#define PORT 8080
#define PATH "tmp.txt"
#define BUFFER_SIZE 4096    //1024*4

int main() 
{
    // 바이너리 읽기 모드 ("rb")
    FILE *fp = fopen(PATH, "rb");
    if (fp == NULL) { perror("fopen"); exit(EXIT_FAILURE); }

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) < 0) { perror("inet_pton"); exit(EXIT_FAILURE); }

    if (connect(sock_fd, (struct sockaddr*)&address, sizeof(address)) < 0) { perror("connect"); exit(EXIT_FAILURE); }
    

    char buffer[BUFFER_SIZE];
    size_t bytes_read = 0;

    while (bytes_read = fread(buffer, 1, sizeof(buffer), fp) > 0)
    {
        send(sock_fd, buffer, strlen(buffer), 0);
    }
    
    fclose(fp);

    // 송신 스트림만 닫아 서버에 FIN 전송 (Half-close)
    shutdown(sock_fd, SHUT_WR);
    
    read(sock_fd, buffer, sizeof(buffer) - 1);
    printf("[client] recevie: %s\n", buffer);

    close(sock_fd);

    return 0;
}