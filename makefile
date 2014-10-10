CC=gcc

CFLAGS = -I .
DEPS=net_include.h recv_dbg.h

all: mcast start_mcast

mcast: mcast.c
	$(CC) -o mcast mcast.c recv_dbg.c get_ip.c

start_mcast: start_mcast.c 
	$(CC) -o start_mcast start_mcast.c 



clean: 
	rm mcast
	rm start_mcast
	
%.o:    %.c
	$(CC) $(CFLAGS) $*.c


