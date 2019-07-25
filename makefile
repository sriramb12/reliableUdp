CC = gcc
CFLAGS = -g -Wall
all: srv cli

srv:	server.o util.o makefile
	$(CC) -o $@ server.o util.o

cli:	client.o util.o makefile
	$(CC) -o $@ client.o util.o

srvdebug.o: util.c makefile common.h server.h client.h
	$(CC) -c $(CFLAGS) srvdebug.c -o srvdebug.o

util.o: util.c makefile common.h server.h client.h
	$(CC) -c $(CFLAGS) util.c -o util.o

server.o: server.c makefile common.h server.h
	$(CC) -c $(CFLAGS) server.c -o server.o

client.o: client.c makefile common.h server.h client.h
	$(CC) -c $(CFLAGS) client.c -o client.o

clean:
	rm -f *.o srv cli
