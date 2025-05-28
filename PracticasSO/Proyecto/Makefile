CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11
LIBS = -lncurses -lpthread -lcrypto

all: server client

server: Server.c
	$(CC) $(CFLAGS) -o server Server.c $(LIBS)

client: Client.c
	$(CC) $(CFLAGS) -o client Client.c $(LIBS)

clean:
	rm -f server client