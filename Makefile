CC=gcc
CFLAGS=-Wall
LIBS=-lpthread

all: client server

client: client.c
	$(CC) -o client client.c $(CFLAGS) $(LIBS)

server: server.c
	$(CC) -o server server.c $(CFLAGS) $(LIBS)

clean:
	rm -f server client
