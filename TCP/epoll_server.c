#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_EVENTS 64 // 한 번에 수신할 최대 이벤트 수

int main()
{
	// SIGPIPE 시 프로세스 종료 방지
	signal(SIGPIPE, SIG_IGN);

	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

	if (listen(server_fd, SOMAXCONN) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	int epoll_fd = epoll_create1(0);
	if (epoll_fd < 0) {
		perror("epoll_create1");
		close(server_fd);
		exit(EXIT_FAILURE);
	}

	struct epoll_event ev, events[MAX_EVENTS];
	ev.events = EPOLLIN;
	ev.data.fd = server_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
		perror("epoll_ctl: server_fd");
		close(server_fd);
		close(epoll_fd);
		exit(EXIT_FAILURE);
	}

	printf("==================================================\n");
	printf("epoll I/O Multiplexing TCP Server listening on port %d\n", PORT);
	printf("==================================================\n");

	while (1)
	{
		int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		if (nfds < 0) {
			if (errno == EINTR) continue; // 시그널에 의한 중단 시 재시도
			perror("epoll_wait");
			break;
		}

		for (int i = 0; i < nfds; i++)
		{
			int current_fd = events[i].data.fd;

			if (current_fd == server_fd)
			{
				struct sockaddr_in client_addr;
				socklen_t client_len = sizeof(client_addr);

				int client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
				if (client_sock < 0) {
					perror("accept failed");
					continue;
				}

				char client_ip[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, sizeof(client_ip));
				int client_port = ntohs(client_addr.sin_port);

				printf("Client connected: %s:%d (FD: %d)\n", client_ip, client_port, client_sock);

				ev.events = EPOLLIN; // 읽기 가능 이벤트 감시
				ev.data.fd = client_sock;
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sock, &ev) < 0) {
					perror("epoll_ctl: client_sock");
					close(client_sock);
				}
			}
			else
			{
				char buffer[BUFFER_SIZE];
                    ssize_t bytes_read = read(current_fd, buffer, sizeof(buffer) - 1);
    
                    if (bytes_read > 0)
                    {
                        buffer[bytes_read] = '\0';
                        printf("[FD: %d] Received: %s\n", current_fd, buffer);
                        fflush(stdout);
    
                        // 에코 송신 (SIGPIPE 방지 플래그 유지)
                        if (send(current_fd, buffer, bytes_read, MSG_NOSIGNAL) < 0) {
                            perror("send error");
                        }
                    }
                    else
                    {
                        if (bytes_read == 0) {
                            printf("Client disconnected (FD: %d)\n", current_fd);
                        } else {
                            perror("read error");
                        }

                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                        close(current_fd);
                    }
                }
            }
        }

        close(epoll_fd);
        close(server_fd);
        return 0;
    }