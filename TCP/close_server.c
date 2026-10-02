#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define PATH "rev.txt"

int main() 
{
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0)
    { perror("setsockopt"); exit(EXIT_FAILURE); }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { perror("bind"); exit(EXIT_FAILURE); }

    if (listen(socket_fd, 3) < 0) { perror("listen"); exit(EXIT_FAILURE); }

    socklen_t addrlen = sizeof(address);
    int fd = accept(socket_fd, (struct sockaddr *)&address, &addrlen);
    if (fd < 0) { perror("accept"); exit(EXIT_FAILURE); }

    FILE *fp = fopen(PATH, "w");
    if (fp == NULL) { perror("fopen"); exit(EXIT_FAILURE); }

    char buffer[BUFFER_SIZE];
    ssize_t read_bytes = 0;
    size_t total_received = 0;

    while (1)
    {
        int len = read_nbytes(fd, buffer, sizeof(buffer));
        buffer[len] = '\0';

        if (len <= 0) break;

        fputs(buffer, fp);
        read_bytes += len;
    }
    
    sprintf(buffer, "Upload Success! Received %ld bytes", read_bytes);

    send(fd, buffer, strlen(buffer), 0);
    fclose(fp);
    close(socket_fd);

    return 0;
}