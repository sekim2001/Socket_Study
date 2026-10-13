#ifndef PROTOCOL_H
#define PROTOCOL_H

#define DISCOVERY_PORT 8888
#define DISCOVERY_REQ_MSG "DISCOVER_DEVICE"

// 네트워크 패킷 구조체 (패딩 바이트 방지를 위해 1바이트 정렬)
#pragma pack(push, 1)
typedef struct {
    char board_name[32];   // 보드 이름 (예: Sensor-Node-01)
    char status[16];       // 상태 (예: OK, ERROR)
    char fw_version[16];   // 펌웨어 버전 (예: v1.0.0)
} DeviceResponse;
#pragma pack(pop)

#endif // PROTOCOL_H
