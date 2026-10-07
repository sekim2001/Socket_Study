#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// 각 클라이언트를 전달 처리하는 워커 스레드 함수
void *client_handler (void *arg)
{
    // 전달받은 소켓의 FD 추출 및 동적 메모리 해제
    int client_sock = *((int *)arg);
    free (arg);

    char buffer[BUFFER_SIZE];
    ssize_t buffer_read;


    printf("[Thread %lu] Client connected (FD : %d)\n", (unsigned long)pthread_self(), client_sock);

    while((buffer_read = read(client_sock, buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[buffer_read] = '\0';
        printf("[FD: %d] Received: %s", client_sock, buffer);

        // 수신 데이터를 에코로 송신
        if (write(client_sock, buffer, buffer_read) < 0)
        {
            perror("write error");
            break;
        }
    } 
}

int main() 
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt"); exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_add;
    server_add.sin_family = AF_INET;
    server_add.sin_port = htons(PORT);
    server_add.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_add, sizeof(server_add)) < 0)
    {
        perror("bind"); exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("listen"); exit(EXIT_FAILURE);
    }

    printf("================================\n");
    printf("Multi-thread TCP Server listening on %d\n", PORT);
    printf("================================\n");

    while(1) 
    {
        int new_sock = accept(server_fd, (struct sockaddr *)&server_add, sizeof(server_add));
        if (new_sock < 0) 
        { 
            perror("accept failed"); 
            continue;   // 에러 발생시 서버가 죽지 않고 다음 클라이언트 연결 대기 
        }

        int *client_sock = malloc(sizeof(int));
        *client_sock = client_handler;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_handler, (void *)client_sock) != 0 )
        {
            perror("pthread_creat failed");
            close(new_sock);
            free(client_sock);
            continue;
        }

        // 스레드 분리 : 스레드 종료 시 자원을 자동으로 OS에 반환하도록 설정
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}