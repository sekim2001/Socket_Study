#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// 스레드에 전달할 클라이언트 정보 구조체
typedef struct {
    int sock_fd;
    struct sockaddr_in addr;
} client_info_t;

void *client_handler(void *arg)
{
    client_info_t *client = (client_info_t *)arg;
    int client_sock = client->sock_fd;
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client->addr.sin_addr), client_ip, sizeof(client_ip));
    int client_port = ntohs(client->addr.sin_port);

    free(client); // 동적 할당 메모리 해제

    char buffer[BUFFER_SIZE];
    ssize_t buffer_read;

    printf("[Thread %lu] Client connected: %s:%d (FD: %d)\n",
            (unsigned long)pthread_self(), client_ip, client_port, client_sock);

    while ((buffer_read = read(client_sock, buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[buffer_read] = '\0';
        printf("[%s:%d] Received: %s\n", client_ip, client_port, buffer);
        fflush(stdout);

        // MSG_NOSIGNAL 플래그로 비정상 종료 시 SIGPIPE 방지
        if (send(client_sock, buffer, buffer_read, MSG_NOSIGNAL) < 0)
        {
            perror("send error");
            break;
        }
    }

    if (buffer_read == 0)
        printf("[Thread %lu] Client disconnected: %s:%d (FD: %d)\n",
                (unsigned long)pthread_self(), client_ip, client_port, client_sock);
    else if (buffer_read < 0)
        perror("read failed");

    close(client_sock);
    return NULL;
}

int main()
{
    // 클라이언트 강제 종료 시 write/send로 인한 SIGPIPE 크래시 방지
    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt"); exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind"); exit(EXIT_FAILURE);
    }

    if (listen(server_fd, SOMAXCONN) < 0)
    {
        perror("listen"); exit(EXIT_FAILURE);
    }

    printf("==================================================\n");
    printf("Multi-thread TCP Server listening on port %d\n", PORT);
    printf("==================================================\n");

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int new_sock = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (new_sock < 0)
        {
            perror("accept failed");
            continue;
        }

        client_info_t *client_info = malloc(sizeof(client_info_t));
        if (client_info == NULL)
        {
            perror("malloc failed");
            close(new_sock);
            continue;
        }
        client_info->sock_fd = new_sock;
        client_info->addr = client_addr;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_handler, (void *)client_info) != 0)
        {
            perror("pthread_create failed");
            close(new_sock);
            free(client_info);
            continue;
        }

        // 스레드 분리
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}