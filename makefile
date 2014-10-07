CC=gcc

CFLAGS = -ansi -c -Wall -pedantic -D_GNU_SOURCE

all: start_mcast mcast

start_mcast: start_mcast.o
	$(CC) -o start_mcast start_mcast.o

mcast: mcast.o recv_dbg.o
	$(CC) -o mcast mcast.o recv_dbg.o

clean:

	rm *.o
	rm start_mcast
	rm mcast
