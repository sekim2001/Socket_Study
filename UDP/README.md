## TCP vs UDP 소켓 프로그래밍 차이

| **구분** | **TCP (SOCK_STREAM)** | **UDP (SOCK_DGRAM)** |
| :--- | :--- | :--- |
| **연결 방식** | 연결 지향 (3-Way-HandShake) | 비연결 지향 |
| **흐름 제어 함수** | listen(), accept(), connect() 필수 | 함수 불필요 (바로 송수신) |
| **송수신 함수** | read()/write() 또는 send()/recv() | sendto(), recvfrom() (DESR/SRC 주소 지정) |
| **데이터 경계** | 바이트 스트림 (Framing 처리 필요) | 데이터그램 단위 (패킷 간 경계 유지) |

---

## Setsockopt() 

### TCP 서버에서 Setsockopt()를 필수로 설정한 이유

```c
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
```

+ TCP 서버 코드에서 관례적으로 소켓을 `SO_REUSEADDR`을 설정함
+ TCP는 연결을 끊을 때 4-Way Handshake를 거치며 소켓이 `TIME_WAIT` 상태로 유지, 이때 프로그램을 강제 종료휴 재시작하면 'Address already in use' 에러가 발생
+ UDP는 연결 개념이 없어 소켓을 닫으면 즉시 운영체제에 반환함으로 필요한 상황이 아니면 옵션을 설정하지 않음

### UDP에서 setsockopt()를 자주 설정하는 상황

#### 1. 수신 타임 아웃 설정 - SO_RCVTIMEO

+ __문제점__ : UDP는 상대방의 패킷을 받지 못하면 `recvfrom()` 함수에서 무한 브로킹 상태에 놓인다. 
+ RTOS나 임베디드 리눅스에서 태스크가 무한 블로킹되면 Watchdog 타이머가 터져서 시스템이 재부팅될 수 있음
+ 수신 타임 아웃 설정 시, 에러를 반환 받아 동작을 처리할 수 있음

```C
    struct timeval tv;
    tv.tv_sec = 3;    // 3초 타임 아웃
    tv.tv_usec = 0;

    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeoof(tv));

    // 3초 동안 데이터가 안 오면 recvfrom()이 -1을 반환하고 errno는 EAGAIN 또는 EWOULDBLOCK이 됨
```

#### 2. 브로드캐스트 전송 - SO_BROADCAST

+ 로컬 네트워크의 모든 장비에 패킷을 송신하고 싶을 때(장치 검색, IP 장동 탐색, DHCP 등) 사용 
+ OS는 기본적으로 브로드캐스트 주소 (255.255.255.255 / 서브넷 브로드캐스트)로 전송을 차단한다 (Permission denied)

```C
    int broadcast_enable = 1;

    setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable))
```

#### 3. 멀티캐스트 그룹 가입 - IP_ADD_MEMBERSHIP

+ 특정 그룹 IP(예: 224.0.0.1 ~ 239.255.255.255)를 구독하여 해당 그룹으로 오는 패킷만 수신하도록 설정

```C
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr("239.255.0.1");   //멀티캐스트 그룹 IP
    mreq.imr_interface.s_addr = htonl(INADDR_ANY)           
    
    setsockopt(sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
```

#### 4. 수신 버퍼 확장 - SO_RCVBUF

+ 커널 수신 버퍼가 꽉 차면 들어오는 UDP 패킷을 경고 없이 버림 (Silent Drop)
+ 고속 센서 테이터 등을 받을 때 버퍼를 늘려 패킷 유실을 방지

```C
    int rcv_buf_size = 1024 * 1024;  // 1MB
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &rcv_buf_size, size_of(rcv_buf_size));
```




  ──────
  ### [3단계] 브로드캐스트 기반 장치 자동 탐색 (Device Discovery)

  네트워크에 물려있는 임베디드 보드들의 IP를 몰라도 자동으로 찾아내는 기능입니다 (예: 공유기
  관리툴, 스마트홈 기기 탐색).

  • 실무 배경: 공장 출하된 보드가 DHCP로 어떤 IP를 할당받았는지 PC에서 찾을 때 필수적으로 사용됩니다.
  • 과제 요구사항:
      1. 탐색자(Manager/PC):
          • 소켓에 SO_BROADCAST 옵션 활성화.
          • 255.255.255.255:PORT로 "DISCOVER_DEVICE" 패킷 브로드캐스트 전송.
          • 수신 대기 타임아웃(예: 3초) 동안 응답해 오는 모든 보드의 정보를 화면에 출력:
          [발견 장치] IP: 192.168.0.15, 보드 이름: Sensor-Node-01, 상태: OK
      2. 디바이스(보드):
          • 브로드캐스트 포트를 bind()하고 대기.
          • "DISCOVER_DEVICE" 요청을 받으면 자신의 디바이스 정보(장비명, 펌웨어 버전 등)를 담아
          요청자 IP로 유니캐스트 응답.

  • 핵심 학습 포인트: SO_BROADCAST, INADDR_BROADCAST, 다수 디바이스 응답 수집.
  ──────
  ### [4단계] I/O 다중화(select/epoll)를 이용한 비동기 센서 제어기 (종합 실전)

  TCP에서 배우셨던 epoll을 UDP와 결합하여 주기적 데이터 송신과 비동기 명령 수신을 동시에처리하는 구조입니다.

  • 실무 배경: 센서 노드는 1초마다 중앙 서버로 데이터를 송신해야 하면서, 동시에 관리자로부터 언제 올지 모르는 원격 제어 명령(LED On/Off, 주기 변경)을 즉시 수신해야 합니다. 단일 스레드로 이를 구현할 때 I/O 멀티플렉싱을 사용합니다.
  • 과제 요구사항:
      1. 단일 프로세스/단일 스레드 구조.
      2. epoll 또는 select의 타임아웃을 1000ms(1초)로 설정:
          • 타임아웃 발생 시: 1초가 지났으므로 서버로 현재 센서 데이터 전송.
          • 수신 이벤트(EPOLLIN) 발생 시: 외부에서 제어 패킷이 도착한 것이므로 즉시 명령 파싱 및
          처리 (예: "송신 주기를 500ms로 변경").

  • 핵심 학습 포인트: UDP 소켓의 select()/epoll 결합, 비동기 이벤트 루프 구현.