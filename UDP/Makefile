CC = gcc
CFLAGS = -Wall -Wextra

TARGETS = udp_server udp_client

all: $(TARGETS)

udp_server: udp_server.c
	$(CC) $(CFLAGS) -o $@ $<

udp_client: udp_client.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean
