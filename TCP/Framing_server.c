#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// n바이트를 읽거나, EOF(상대방 종료)/에러가 발생할 때까지 반복
size_t read_nbytes (int sock, void *buf, size_t n)
{
    size_t total_read = 0;
    char *ptr = (char *)buf;

    while (total_read < n)
    {
        ssize_t bytes_read = read(sock, ptr + total_read, n - total_read);

        if (bytes_read == 0)  // EOF 
        {
            return total_read;
        }
        else if (bytes_read < 0)  // Error
        {
            return -1;
        }

        total_read += bytes_read;
    }
    
    return total_read;
}

int main()
{
    // 소켓 디스크립터 생성
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == 0) 
    { 
        perror("socket"); exit(EXIT_FAILURE);
    }

    // 소켓 옵션 설정
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0)
    { 
        perror("setsockopt"); exit(EXIT_FAILURE);
    }

    // IPv4 주소 정보 정의
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // binding
    if (bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    { 
        perror("bind"); exit(EXIT_FAILURE);
    }

    // start listen
    if (listen(socket_fd, 3) < 0)
    { 
        perror("listen"); exit(EXIT_FAILURE);
    }

    // open socket
    socklen_t addrlen = sizeof(address);
    int new_socket = accept(socket_fd, (struct sockaddr *)&address, &addrlen);
    if (new_socket < 0) 
    { 
        perror("accept"); exit(EXIT_FAILURE);
    }

    printf("[Server] Start listening\n");

    char buffer[BUFFER_SIZE];

    while (1)
    {
        uint32_t net_len = 0;

        // Header 읽기
        ssize_t res = read_nbytes(new_socket, &net_len, sizeof(net_len));
        if (res <= 0) break;

        // read data body length from header 
        uint32_t body_len = ntohl(net_len);
        if (body_len >= BUFFER_SIZE)
        {
            fprintf(stderr, "[server] Message size is too large... \n");
            break;
        }

        // read body data
        res = read_nbytes(new_socket, &buffer, body_len);
        if (res <= 0)
        {
            fprintf(stderr, "[server] disconnect while reading... \n");
            break;
        }

        // 문자열 출력을 위해 널 문자 추가
        buffer[body_len] = '\0';
        printf("[server] received : %s\n", buffer);
    }

    close(new_socket);
    close(socket_fd);

    return 0;
}