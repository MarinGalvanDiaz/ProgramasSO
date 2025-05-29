CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -g
LIBS = -lncurses -lssl -lcrypto -lpthread

all: servidor cliente

servidor: servidor.c common.c
	$(CC) $(CFLAGS) -o servidor servidor.c common.c $(LIBS)

cliente: cliente.c common.c
	$(CC) $(CFLAGS) -o cliente cliente.c common.c $(LIBS)

clean:
	rm -f servidor cliente