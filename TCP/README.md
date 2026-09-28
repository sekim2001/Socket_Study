## TCP 통신 흐름

#### TCP 서버 동작 흐름
    socket() : 통신 앤드포인트 (소켓 디스크럽터) 생성
       ↓
    setsockopt() : 소켓 세부 옵션 (포드 재사용 등) 설정
       ↓
    bind() : 소켓에 IP 주소와 포드 번호 바인딩
       ↓
    listen() : 수동 대기 상태(LISTEN)로 전환 (연결 요청 큐 생성)
       ↓
    accept() : 3-Way Handshake 완료된 연결을 수락 (통신 전용 new_socket 반환)
       ↓ 
    read()/send() : 커널 버퍼를 통한 양방향 바이트 스트림 송수신
       ↓ 
    close() : 4-Way Handshake를 거쳐 연결 종료

#### TCP 클라이언트 동작 흐름
    socket() : 통신 앤드포인트 (소켓 디스크럽터) 생성
       ↓ 
    inet_pton() : 문자열 IP를 네트워크 바이너리 주소로 변환
       ↓ 
    connect() : 서버로 연결 요청 (3-Way Handshake 개시) 및 임시 포트 자동 바인딩
       ↓
    read()/send() : 커널 버퍼를 통한 양방향 바이트 스트림 송수신
       ↓ 
    close() : 4-Way Handshake를 거쳐 연결 종료

## TCP 소켓 함수

### 1. socket()
통신을 위한 엔드포인트(소켓 파일 디스크럽터)를 커널에 생성

```c
int socket(int domian, int type, int protocol);
```
+ `domain` (프로트콜 계열/주소 체계):
    + `AF_INET` : IPv4 인터넷 프로트콜
    + `AF_INET6` : IPv6 인터넷 프로트콜
    + `AF_UNIX` / `AF_LOCAL` : 동일 호스트 내 프로세스 간 IPC 통신용 유닉스 도메인 소켓
 
+  `tpye` (전송 방식/소켓 타입) : 
    +  `SOCK_STREAM` : TCP -> 연결 지향적, 신뢰성 있는 바이트 스트림 (순서보장, 흐름/혼잡 제어)
    +  `SOCK_DGRAM` : UDP -> 비연결 지향적, 데이터그램 전송 (경계 보존, 빠른 전송, 손실 가능)
    +  `SOCK_RAW` : IP 패킷 계층을 직접 제어할 수 있는 원시 소켓 (관리자 권한 필요)

+  `protocol` (구체적안 프로트콜): 
    + 보통 0를 전달하며, 커널이 domain과 type에 맞는 프로토콜을 자동 선택
 
+ __반환값__: 
    + 성공 : 파일 디스크럽터 정수
    + 실패 : -1
    
### 2. setsockopt() 

소켓의 동작 방식을 제어하는 옵션 설정

```c
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
```

+ `sockfd` : 설정을 적용할 소켓 파일 디스크립터

+  `level` (설정 계층) :
    + `SOL_SOCKET` : 소켓 일반 레벨 옵션
    + `IPPROTO_TCP` : TCP 프로트콜 고유 옵션
    + `IPPROTO_IP` : IP 계층 옵션

+ `optname` (옵션 이름)
    + `SO_REUSEADDR` : 서버가 비정상 종료되거나 재시작될 때 TIME_WAIT 상태에 있는 로컬 주소/포트를 즉시 재사용할 수 있도록 허용 (Address already in use 에러 방지)
    + `SO_REUSEPORT`: 동일한 포트에 여러 프로세스/스레드가 각각 소켓을 바인드하여 커널 레벌 로드 밸런싱을 허용 (Linux 3.9+)
    + `SO_KEEPALIVE`: TCP 얀걀 우지를 위해 주기적으로 probe 패킷을 전송
    + `SO_RCVTIMEO / SO_SNDTIMEO`: 수신/송신 타임아웃 지정 (struct timeval 전달)
    + `SO_REVBUF / SO_SNDBUF`: 커널 송/수신 버퍼 크기 변경
    + `TCP_NODELAY` (IPPROTO_TCP 레벨): Nagle 알고리즘을 비활성하여 패킷 지연 없이 즉시 송신
 
+ `optval`: 횔성화 여부나 설정값을 담은 변수의 포인터

+ `oplen`: optval 버퍼의 크기

### 3. bind()

생성된 소켓에 특정 IP 주소와 포트 번호를 할당 =>  커널의 포트 테이블에 소켓 등록

```C
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

+ `sockfd`: 바인딩할 소켓 파일 디스크립터

+ `addr`: 주소 정보가 담긴 구조체 포인터 
    + IPv4 전용 구조체인 `struct sockaddr_in`을 범용 소켓 주소 구조체인 `struct sockaddr *`로 캐스팅

+ `addrlen`: 주소 구조체의 전체 크기

####  주소 구조체(sockaddr_in) 설정 및 바이트 오더링 함수
```c
address.sin_family = AF_INET;
address.sin_addr.s_addr = INADDR_ANY;
address.sin_port = htons(PORT);
```

+ `sin_family`: 주소 체계

+ `sin_addr.s_addr`: IPv4 호스트 주소
    + `INADDR_ANY`: (0.0.0.0): 특정 NIC IP만 고정하지 않고 서버의 모든 네트워크 인터페이스로부터 들어오는 접속을 모두 수신

+ `sin_port`: 포트 번호
    + `htons()`: Host to Network Short => x64 시수템의 Little Endian 바이트 순서를 네트워크 표준인 Big Endian 바이트 순서로 변환
    
### 4. listen() 
소켓을 클라이언트의 연결 요청을 기다리는 __수동 대기 소켓(Passive Socket)__ 으로 전환.

```C
int listen(int sockfd, int blocking);
```
+ 인자 :
    + `sockfd`: 바인딩된 리스닝 소켓 디스크립터
    + `blocking`: 연결 요청 대기 큐의 최대 크기

+  TCP 내부 동작:
    + TCP 소켓 상태가 `CLOSED`에서 `LISTEN` 상태로 전이
    + 커널 내부에서 2개의 큐가 유지됨
        1. __SYN Queue (반개방 연결)__ : 클라이언트가 SYN을 보내고 서버가 SYN+ACK를 보낸 상태.
        2. __Accept Queue (완전 연결)__ : 클라이언트의 마지막 ACK가 도착하여 TCP 3-Way Handshake가 완료된 연결들이 accept() 호출 전까지 머무는 큐
    + `blocking`는 일반적으로 Accept Queue의 한도 정의

### 5. accept()
    
Accept Queue에서 완료된 연결 요청을 하나 꺼내 클라이언트와 통신할 새로운 소켓을 생성

```C
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

+ 인자
    + `sockfd`: 대기 중인 리스닝 소켓 디스크립터
    + `addr`: 접속한 클라이언트의 IP와 포트 번호가 커널에 의해 채워지는 버퍼 구조체 포인터
    + `addrlen`: Value-Result 인자, 입력 시 버퍼 크기를 전달하고, 리턴 시 실제로 기록된 주소 크기로 갱신

+ 동작과 반환값
    + 반환값 : 해당 클라이언트와 1:1로 __매핑된 전용 소켓__ 의 파일 디스크립터 정수 (ESTABLISHED 상태)
    + 기존 `sockfd`는 그대로 남아 다른 클라이언트의 접속을 받음
    +  3-Way Handshake는 커널 백그라운드에서 이미 완료됨

### 6. inet_pton() 
텍스트 IP 주소를 네트워크 바이너리 주소로 변환
```C
int inet_pton(int af, const char *src, void *dst);
```

+ 인자
    + `af`: 주소 체계 (AF_INET은 IPv4, AF_INET6은 IPv6).
    + `src`: 변환할 IP 문자역 포인터
    + `dst`: 반환된 바이너리 주소가 저장될 메모리 주소

+ 반환값
    + 1: 성공
    + 0: 잘못된 형식의 IP 주소 문자열
    + -1: 유효하지 않은 주소 패밀리(af) 전달

### 7. connect()

서버에 연결을 요청하여 TCP 3-Way Handshaake 시작

```C
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

+ 인자
    + `sockfd`: 통신에 사용할 클라이언트 소켓 디스크립터
    + `addr`: 주소 정보가 담긴 구조체 포인터 
        + IPv4 전용 구조체인 `struct sockaddr_in`을 범용 소켓 주소 구조체인 `struct sockaddr *`로 캐스팅
    + `addrlen`: 주소 구조체의 전체 크기

+ __임시 포트 자동 할당(Implicit Bind)__ : 클라이언트는 bind()를 명시적으로 호출하지 않고, connect()가 호출되는 순간 커널이 비어있는 Ephemeral Port 중 하나를 클라이언트 소켓에 자동으로 바인딩

+ __3-Way Handshake 개시__ : 서버로 첫 syn 패킷을 전송하고, 소켓 상태를 `CLOSED` 상태에서 `SYN_CENT`로 변경

+ 반환값
    + 0: 성공
    + -1: 실패 (서버다 닫혀 있으면 ECONNREFUSED, 방화벽 등으로 무응답이면 ETIMEDOUT)

### 8. send()
```C
ssize_t send(int fd, const void *buf, size_t len, int flags);
```

+ 인자 설명:
    + fd: 서버와 연결된 소켓 디스크립터
    + buf: 전송할 데이터가 담긴 메모리 버퍼
    + len: 전송할 바이트 수
    + flags: 전송 옵션 플래그 
        + 0: 일반적인 write() 동작과 동일
        + `MSG_NOSIGNAL`: 상대방이 이미 연결을 끊었을 때 프로세스가 `SIGPIPE` 시그널로 비정상 종료되는 것을 막고 EPIPE 에러를 반환받도록 설정
+ 동작 원리:
    + 사용자의 buf 데이터를 **운영체제 커널의 TCP 송신 버퍼(Send Buffer)** 로 복사 종료 후 즉시 반환
+ 반환값: 커널 송신 버퍼에 성공적으로 복사된 바이트 수, 실패 시 -1.

### 9. send()
```C
ssize_t send(int fd, const void *buf, size_t len, int flags);
```

+ 인자: 
  + fd: 소켓 파일 디스크립터
  + buf: 서버로부터 받은 데이터를 저장할 사용자 버퍼
  + count: 버퍼의 최대 수용 바이트 크기
+ 동작 원리:
    + 커널의 TCP 수신 버퍼에 데이터가 도착할 때까지 대기(블로킹)
    + 데이터가 도착하면 최대 count만큼 사용자 버퍼로 복사
+ 반환값:
    + > 0: 성공적으로 읽어온 바이트 수
    + 0: 상대방(서버)이 연결을 종료함 (EOF) / 서버가 close()를 호출해 FIN 패킷을 보낸 경우 0을 반환
    + -1: 읽기 실패(에러)

### 7. close()
+ 커널이 클라이언트에게 FIN 패킷을 전송하며 TCP 4-Way Handshake 종료 절차 진행
+ FIN 송신 후 연결은 __FIN_WAIT_1 -> FIN_WAIT_2 -> TIME_WAIT__ 상태를 거쳐 정리됨


## 연결형 서비스
### TCP 연결 설정 -> 3-Way Handshake
<p align="center">
    <img width="591" height="390" alt="image" src="https://github.com/user-attachments/assets/31a35d35-9356-476e-92d7-4b6ba32f172b" />
</p>

+ 첫번째 단계 (SYN)
    + 클라이언트가 서버로 연결 요청를 송신
    + __SYN(Synchronize)__ 플래그와 임의의 시작 번호 __Seq=x__를 전송
    + 클라이언트 상태는 SYN_SENT로 바뀜

+ 두번쩨 딘계 (SYN-ACK)
    + 서버가 클라이언트의 요청을 받고 수락 응답을 송신
    + 요청을 수락한다는 Ack(x+1)와 

### TCP 연결 해제 설정 -> 4-Way Handshake
<p align="center">
<img width="506" height="391" alt="image" src="https://github.com/user-attachments/assets/f1f0b0cd-16b6-48a0-b243-450aa8c949c1" />
</p>


