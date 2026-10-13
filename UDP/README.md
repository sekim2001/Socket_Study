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

---

## Broadcast

### 네트워크의 데이터 전송 방식 구분

1. ___유니캐스트(Unicast)__ : 1 대 1 통신 (상대방 IP를 정확히 알고 보냄)
2. __멀티캐스트(Multicast)__ : 1 대 특정 그룹 통신 (특정 채널을 구독한 사람들에게 보냄)
2. __브로드캐스트(Broadcast)__ : 1 대 전체 통신 (네트워크에 속한 모든 장비에게 방송)

### 브로드캐스트 동작 흐름 

1. __송신 (Manager)__ : 목적지 IP를 `255.255.255.255`, L2 MAC 주소를 `FF:FF:FF:FF:FF:FF`로 설정하여 전송
2. __복제 및 전달 (공유기/스위치)__ : 공유기는 브로드캐스트 주소를 확인하고 연결된 모든 유/무선 포트로 패킷을 복제(Flooding)해서 전달. (라우터 경계를 넘어 외부 인터넷으로 송신하지 않음)
3. __수신 및 필터링 (Device)__ : 같은 네트워크의 모든 장비가 패킷 수신, `8888` 포트를 열어둔 프로그램은 패킷을 처리하고, 닫힌 장비는 OS에서 패킷을 버림
4. __회신 (Device -> Manager)__ : 요청을 수신한 보드는 패킷에서 요청자의 IP를 확인하고, 1:1 유니캐스트로 자신의 정보 회신

###  브로드캐스트의 IP, Port 정보

#### IP

+ **제한된 브로드캐스트 (Limited Broadcast)** : 전용 특수 IP `255.255.255.255`
    + 서브넷 상관 없이 모든 로컬 LAN 전체에 송신

+  **서브넷 브로드캐스트 (Directed Broadcast)** : 특정 서브넷으로 LAN 목록을 제한
    + 예 : 서브넷이 `192.168.0.0/24`이면 `192.168.0.255`로 설정

#### Port 
+ 비어있는 포트 `1024~65535` 사이 임의로 설정