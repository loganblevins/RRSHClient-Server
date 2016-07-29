CC=gcc
CFLAGS=-g -O1 -Wall
LDLIBS=-lpthread

all: rrsh-server rrsh-client

rrsh-server: rrsh-server.h rrsh-server.c parser.c csapp.h csapp.c
rrsh-client: rrsh-client.c csapp.h csapp.c

clean:
	rm -f *.o *~ *.exe rrsh-server rrsh-client csapp.o core

