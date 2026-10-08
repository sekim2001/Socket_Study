#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

#pragma pack(push, 1) 
typedef struct {
    uint16_t msg_type;      // 1: 센서 데이터, 2: 제어 명령 등
    uint32_t seq_num;       // 패킷 순서 번호
    int16_t  temperature;   // 온도 (예: 25.4도 -> 254로 전송)
    uint16_t humidity;      // 습도
} SensorPacket;
#pragma pack(pop)

int main() 
{
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0) 
    {
        perror ("create socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_add;
    memset(&server_add, 0, sizeof(server_add));
    server_add.sin_family = AF_INET;
    server_add.sin_port = htons(PORT);
    server_add.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (const struct sockaddr *)&server_add, sizeof(server_add)) < 0)
    {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("UDP Server listening on port %d...\n", PORT);

    SensorPacket rcv_data;
    struct sockaddr_in client_add;
    socklen_t client_len = sizeof(client_add);
    
    while(1)
    {
        ssize_t rcv_len = recvfrom(server_fd, &rcv_data, sizeof(SensorPacket), 0, (struct sockaddr *)&client_add, &client_len);
        if (rcv_len < (ssize_t)sizeof(SensorPacket))
        {
            if (rcv_len < 0) perror("recvfrom failed");
            else printf("Received incomplete packet (%zd bytes)\n", rcv_len);
            continue;
        }

        int16_t temp = (int16_t)ntohs(rcv_data.temperature);

        printf("Receive Data [%s:%d]\n", inet_ntoa(client_add.sin_addr), ntohs(client_add.sin_port));
        printf("msg_type : %d\n", ntohs(rcv_data.msg_type));
        printf("seq_num : %d\n", ntohl(rcv_data.seq_num));
        printf("temperature : %.1f\n", temp / 10.0f);
        printf("humidity : %d\n", ntohs(rcv_data.humidity));
        printf("-------------------------------\n");
    }

    close(server_fd);
    return 0;
}
