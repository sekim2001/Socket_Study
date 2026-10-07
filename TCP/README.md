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
    + 0 < : 성공적으로 읽어온 바이트 수
    + 0: 상대방(서버)이 연결을 종료함 (EOF) / 서버가 close()를 호출해 FIN 패킷을 보낸 경우 0을 반환
    + -1: 읽기 실패(에러)

### 7. close()
+ 커널이 클라이언트에게 FIN 패킷을 전송하며 TCP 4-Way Handshake 종료 절차 진행
+ FIN 송신 후 연결은 __FIN_WAIT_1 -> FIN_WAIT_2 -> TIME_WAIT__ 상태를 거쳐 정리됨

## TCP 헤더 구조

<p align="center">
    <img width="546" height="320" alt="image" src="https://github.com/user-attachments/assets/da190cfc-4046-4af8-9512-0e9e9ac0363f" />
</p>

+ Sourse/Destination Port Number (각 16 비트)
    + Socket Address => IP Address + Port Address

+ Sequence Number (32 비트)
    + `SYN` flag = 1 : __초기 순서 번호 (ISN, Initial Sequence Number)__ 으로 사용 => 난수 
    + `SYN` flag = 1 : 바이트 단위로 구분되어 순서화되는 번호 => 임의 값으로 시작, 최대값(2^32) 이후 `0`부터 시작함

+ Acknowledgement Number (확인응답 번호 / 승인 번호) (32 비트)
    + <img width="313" height="124" alt="image" src="https://github.com/user-attachments/assets/3ad25d69-b936-4e4c-9bcc-6fdcf88612d6" />
    + `ACK` flag = 1으로 설정된 경우, 수신하기를 기대하는 다음 sequence number => 마지막 수신 성공한 __순서번호 + 1__ 
    + 송신 메세지가 정상 수신되었음을, 수신측에서 송신측으로 확인응답하는 것

+ 헤더 길이 필드 (Header length, HLEN, 4 비트)
    + TCP 헤더 길이를 4bytes (32bits) 단위로 표시
    + 범위: 20bytes ~ 60bytes => 5 ~ 2^4 - 1

+ 예약 비트 (Reserved, 3~6bits)
    + 프로토콜 확장을 위해 남겨둔 예비 비트, 일반적으로 0
      
+ 제어 플래그 Flag bits (URG, ACK, PSH, RST, SYN, FIN)
    + `URG`(Urgent): 긴급 데이터가 포함되어 있음을 표시 => Urgent Pointer 필드에 값이 채워져있음을 알림
    + `ACK`(Acknowledgment): 확인 응답 번호 필드에 유효한 값이 들어있음을 표시
        +  __SYN 세그먼트__ 송신 이후 항상 모든 세그먼트는 __1__ 유지
    + `PSH`(Push): 수신 측에게 버퍼가 찰 때까지 기다리지 말고 데이터를 즉시 응용프로그램에 전달하도록 요청
    + `RST`(Reset): 연결에 문제가 있어 강제로 연결을 초기화 하고자 할 때
        + LISTEN,SYN_RCVD 상태일때 => RST 수신한 경우에 => LISTEN 상태로 들어감
        + 그밖의 상태 일때 => RST 수신한 경우에 => 연결 끓고 CLOSED 상태로 들어감
    + `SYN`(Synchronize): 연결을 시작할 때 초기 시퀀스 번호를 동기화하기 위해 (3-Way Handshake 시작)
    + `FIN`(Finish): 데이터 전송이 완료되어 정상적으로 연결을 종료하고 싶을 때

+ 윈도우 크기 (Window size, 16 비트)
    + TCP 흐름제어를 위해 송신자에게 자신의 수신 버퍼 여유용량 크기를 지속적으로 통보

+ Urgent pointer (16 비트)
    + 현재 일련번호(sequence number)로부터 긴급 데이터까지의  바이트 오프셋(offset) 


## 연결형 서비스
+ TCP 연결 설정
    + 연결요청  : SYN=1, ACK=0   (SYN 세그먼트)
    + 연결허락  : SYN=1, ACK=1   (SYN+ACK 세그먼트)
    + 연결설정  : ACK=1          (ACK 세그먼트)

+ TCP 연결 종료
    + 종결요청 : FIN=1           (FIN 세그먼트)
    + 종결응답 : FIN=1, ACK=1    (FIN+ACK 세그먼트)

### TCP 연결 설정 -> 3-Way Handshake
+ 연결이 시작됨과 옵션이 교환됐음을 알림
+ ISN(랜덤한 초기 순서번호)를 생성하고 교환했음을 알림

<p align="center">
    <img width="584" height="292" alt="image" src="https://github.com/user-attachments/assets/937a9e20-f177-4fe3-8dee-e3b3e74def4d" />
</p>

1. **1단계: SYN (Synchronize)**
   - **행동**: 클라이언트가 서버에 연결 요청 메시지를 송신
   - **데이터**: TCP 헤더의 `SYN` 플래그를 `1`로 설정하고, 임의의 시퀀스 번호(`seq: x`)를 생성
   - **상태**: 클라이언트는 `SYN-SENT` 상태가 되고, 서버는 `SYN-RECEIVED` 상태로 전환됩니다.

2. **2단계: SYN-ACK (Synchronize-Acknowledgment)**
   - **행동**: 서버가 클라이언트의 요청을 받고, 연결을 수락함과 동시에 자신도 클라이언트와 연결하겠다는 요청을 함께 송신
   - **데이터**: `SYN`과 `ACK` 플래그를 모두 `1`로 설정합니다.
     - 클라이언트의 시퀀스 번호에 `1`을 더한 확인 응답 번호(`ack: x + 1`)
     - 서버가 생성한 시퀀스 번호(`seq: y`)

3. **3단계: ACK (Acknowledgment)**
   - **행동**: 클라이언트가 서버의 응답을 확인하고 최종적으로 연결이 완료되었음을 알림
   - **데이터**: `ACK` 플래그를 `1`로 설정하고, 서버의 시퀀스 번호에 `1`을 더한 확인 응답 번호(`ack: y + 1`) 성신
   - **결과**: 이 단계가 완료되면 클라이언트와 서버 모두 `ESTABLISHED` 상태가 되어 데이터를 송수신할 수 있음

### TCP 연결 해제 설정 -> 4-Way Handshake
+ 정상 종료인 경우에, TCP 연결 종료는, 양방향 2개 연결을 각 측이 독립적으로 닫게 됨

+ TCP 연결 종료 구분
    + 정상 종료 (Normal Close) : `4-Way Handshake`에 의해 양방향 모두 종료
    + 반 종료 (Half Close) : 양측이 동시에 회선 종료 않고, 한쪽 연결이 열린채로 놔두고 종료
    + 동시 종료 (Simultaneous Close): 거의 동시에 양측에서 FIN 세그먼트를 보내는 경우
    + TCP Reset 요구(RESET 세그먼트) 기능

<p align="center">
    <img width="511" height="462" alt="image" src="https://github.com/user-attachments/assets/dc084d90-1064-4f4c-89c6-0ec762982dd2" />
</p>

1. **1단계: FIN (Finish)**
   - **행동**: 클라이언트가 서버에 연결 종료 요청 메시지를 전송함
   - **데이터**: TCP 헤더의 `FIN` 플래그를 `1`로 설정하고, 연결된 상태에서 마지막 시퀀스 번호보다 `1` 큰 시퀀스 번호(`seq: K`)를 전송함
   - **확인 응답 번호**: 별도의 확인 응답 번호는 설정하지 않음
   - **상태**: 클라이언트는 `FIN-WAIT-1` 상태로 전환됨

2. **2단계: ACK (Acknowledgment)**
   - **행동**: 서버가 클라이언트의 연결 종료 요청을 확인하고 응답함
   - **데이터**: `ACK` 플래그를 `1`로 설정하고, 클라이언트의 시퀀스 번호에 `1`을 더한 확인 응답 번호(`ack: K + 1`)를 전송함
     - 서버의 시퀀스 번호(`seq: L`) 포함됨
   - **상태**
     - 서버는 `CLOSE-WAIT` 상태로 전환됨
     - 클라이언트는 `FIN-WAIT-2` 상태로 전환됨

3. **3단계: FIN + ACK (Finish + Acknowledgment)**
   - **행동**: 서버가 자신의 연결 종료를 알리고, 클라이언트의 `FIN` 세그먼트를 확인했다는 응답을 함께 전송함
   - **데이터**: `FIN`과 `ACK` 플래그를 모두 `1`로 설정함
     - 서버의 시퀀스 번호(`seq: L`) 전송함
     - 클라이언트의 `FIN`에 대한 확인 응답 번호(`ack: K + 1`) 전송함
   - **상태**: 서버는 `LAST-ACK` 상태로 전환됨

4. **4단계: ACK (Acknowledgment)**
   - **행동**: 클라이언트가 서버의 연결 종료 요청을 확인하고 최종 응답함
   - **데이터**: `ACK` 플래그를 `1`로 설정하고, 서버의 시퀀스 번호에 `1`을 더한 확인 응답 번호(`ack: L + 1`)를 전송함
   - **상태**
     - 클라이언트는 `TIME-WAIT` 상태로 전환됨
     - 일정 시간이 경과하면 `CLOSED` 상태로 전환됨
     - 서버는 최종 `ACK` 수신 후 `CLOSED` 상태로 전환됨
   - **결과**: 양단의 TCP 연결이 최종적으로 종료됨

---

## 동적 메모리 할당 (Dynamic Memory Allocation)

+ 주요 함수 목록 (<stdlib.h>)
| 함수명 | 역할 | 특징 |
| :--: | :--: | :--: |
| malloc | 메모리 할당 | 인자로 받은 바이트 크기만큼 할당, 초기화 안됨 (쓰레기 값) |
| calloc | 배열 형태 메모리 할당 | 개수와 크기를 받아 할당, 모든 비트를 `0`으로 자동 초기화 |
| realloc | 메모리 크기 재할당 | 이미 할당된 메모리의 크기를 확장하거나 축소할 때 사용 |
| free |  메모리 해제 | 힙 영역에 할당된 메모리를 시스템에 반환 (메모리 누수 방지) |

---

## 스레드 함수 (pthread.h)

+ pthread_create
    ```C
    int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);
    ```
    + 새로운 스레드를 생성하여 `start_routine` 함수를 실행, 인자 `arg`는 포인터 형태로 전달
    + 성공 시 0를 반환하고, 생성된 스레드는 독립적인 흐름을 가짐

+ pthread_join
    ```C
    int pthread_join(pthread_t thread, void **retval);
    ```
    + 대상 스레드가 종료될 때까지 호출한 스레드가 __블로킹(대기)__ 하고, 스레드가 반환한 값 `retval`을 회수하고 관련 커널 자원을 정리
    + 회수 하지 않은 자원은 메모리에 남아 좀비 스레드가 된다

+ pthread_detach 
    ```C
    int pthread_detach(pthread_t thread);
    ```
    + 스레드를 메인 스레드와 분리, 스레드 종료 시 즉시 운영체제가 자원을 즉시 자동 회수

+ pthread_exit 
    ```C
    void pthread_exit(void *retval);
    ```
    + 현재 실행 중인 스레드를 즉시 종료하고 반환값을 `retval`로 반환

+ pthread_self 
    ```C
    pthread_t pthread_self(void);
    ```
    + 현재 코드가 실행 중인 스레드의 고유 식별자 `pthread_t`를 반환

+ 여러 스레드가 공유 메모리(전역 변수, 큐 등)에 동시 접근할 때 데이터를 보호하기 위해 스레드 동기화(Mutex & Condition Variable) 사용

---

## signal(SIGPIPE, SIG_IGN);

+ 이미 연결이 끊어진 소켓에 write() 또는 send()를 시도하면 커널이 프로세스에 `SIGPIPE` 시그널을 전달하며 프로세스가 `비정상 종료` 될 수 있다

### SIGPIPE의 발생 흐름과 방어 메커니즘

1. 클라이언트가 강제 종료 되며 서버 커널에 FIN 또는 RST 도착
2. 서버가 해당 소켓에 write() / send() 시도
3. 클라이언트 OS 가 "받을 프로세스 없을"으로 RST 패킷 회신
4. 끊어진 파이프/소켓에 데이터를 쓰러는 시도를 하여 커널이 서버 프로세스에 SIGPIPE 시그널 발송
    
    + [기본 동작] : 프로세스가 즉시 비정상 종료 (Terminate / Crash)
        + 서버가 종료 되면서 다른 클라이언트 또한 통신이 불가능해짐
    
    + [signal(SIGPIPE, SIG_IGN)] : 시그널을 무시
        + 서버 프로세스는 죽지 않고 write()/send()가 -1를 반환하고 errno에 `EPIPE` 설정

--- 

## epoll - I/O 멀티플렉싱(Multiplexing) 메커니즘

+ 서버의 멀티스레드/멀티프로세스 모델은 클라이언트당 하나의 스레드가 필요하므로 컨텍스트 스위칭 오버헤드와 메모리 낭비가 심함
+ epoll은 __단 하나의 스레드(프로세스)로__ 많은 소켓을 감시하고, I/O(읽기/쓰기/접속) 이벤트가 발생한 소켓만 처리함

### select/poll 과의 차이점

| 구분 | select/poll | epoll |
| --- | --- | --- |
| FD 개수 제한 | select: 기본 1024개 제한 | 무제한 (시스템 자원 한도 내) |
| 커널 전달 방식 | 호출마다 전체 감시 FD 목록을 유저 -> 커널로 복사 | 최초 등록 시 커널에 1회만 등록 (epoll_ctl) |
| 자료 구조 | 단순 배열 / 비트마스크 | Red-Black Tree (감시 대상) + Double Linked List (준비 큐) |
| 복잡도 | 어떤 FD에서 이벤트가 발생했는지 확인을 위해 전체 루프 순회 | 이벤트가 발생한 FD 목록만 반환 |

### epoll 주요 시스템 콜 API

#### epoll_create1(int flags)

+ epoll 인스턴스 (커널 내부 객체)를 생성하고 이를 가리키는 파일 디스크립터 (epoll_fd)를 반환

+ `flags`
    + `0`
        + defult 값으로 아무 플래그 없이 epoll 인스턴스 생성
    + `EPOLL_COLEXEC`
        + 새롭게 생성된 epoll 인스턴스의 FD에 `FD_CLOEXEC`(Close-on-exec) 프래그 설정
        + execve() 같은 시스템 콜로 새로운 프로그램을 실행할 때 epoll FD를 자식 프로세스에게 상속하지 않고 종료

#### epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)

+ 감시할 FD을 등록, 수정, 삭제 

+ `op`옵션 
    + `EPOLL_CTL_ADD` : 새 FD를 감시 목록에 추가
    + `EPOLL_CTL_MOD` : 기존 FD의 감시 이벤트 변경
    + `EPOLL_CTL_DEL` : 감시 목록에서 FD 제거

+ 주요 이벤트 플래그 (event.events)
    + `EPOLLIN`: 읽을 데이터가 들어옴 (listen 소켓의 경우 새 연결 요청 도착)
    + `EPOLLOUT`: 소켓 송신 버퍼에 공간이 생겨 쓰기가 가능해짐
    + `EPOLLET`: Edge-Triggered 모드로 동작 (기본값은 Level-Triggered)
      
    ```c
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);
    ```

#### epoll_wait (int epfd, struct epoll_event *events, int maxevents, int timeout)

+ 등록된 FD 중 이벤트가 발생할 때까지 블로킹
+ 이벤트가 발생하면 커널이 events 배열에 발생한 FD(소켓) 정보를 채워주고, 발생한 이벤트의 총 개수(nfds)를 반환
+ `timeout` : `-1`이면 무한 대기, `0`이면 non-blocking, `양수`면 ms 단위 타임아웃

#### epoll_server.c 의 전체 종작 흐름

1. 초기화 <br>
    socket() -> bind() -> listen()<br>
    epoll_create1() 으로 epoll 인스턴스 생성<br>
    epoll_ctl(ADD) 로 server_fd (리슨 소켓)를 감시 목록에 등록<br>

2. 이벤트 루트 <br>
       epoll_wait() 호출 (이벤트 발생 시까지 대기)<br>
            │<br>
            ├─► [이벤트 발생한 FD 개수(nfds)만큼 for문 순회]<br>
            │<br>
            ├─► Case A: events[i].data.fd == server_fd (신규 접속)<br>
            │     - accept() 로 새 client_sock 획득<br>
            │     - epoll_ctl(ADD) 로 client_sock을 epoll 감시 목록에 등록 (EPOLLIN)<br>
            │<br>
            └─► Case B: events[i].data.fd == client_sock (기존 클라이언트 데이터)<br>
                  - read() 로 데이터 수신<br>
                  - if (read > 0)  : send() 로 응답 처리<br>
                  - if (read <= 0) : 클라이언트 종료 감지<br>
                                     epoll_ctl(DEL) 감시 해제 후 close(client_sock)<br>
