#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"

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
    int client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_fd < 0) 
    {
        perror ("create socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid server address");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    uint32_t num = 0;
    SensorPacket send_data;

    printf("send start\n");

    while (1)
    {
        send_data.msg_type = htons(1);
        send_data.seq_num = htonl(num++);
        send_data.temperature = htons(rand() % 1000);
        send_data.humidity = htons(rand() % 100);

        sendto(client_fd, &send_data, sizeof(SensorPacket), 0, (struct sockaddr *)&server_addr, server_len);
        
        if (num == INT_MAX) num = 0;
        sleep(1);
    }
    
    close(client_fd);
    return 0;
}
