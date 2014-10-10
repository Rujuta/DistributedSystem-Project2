CC=gcc

CFLAGS = -I .
DEPS=net_include.h

all: mcast start_mcast

mcast: mcast.c
	$(CC) -o mcast mcast.c 

start_mcast: start_mcast.c 
	$(CC) -o start_mcast start_mcast.c 



clean: 
	rm mcast
	rm start_mcast
	
%.o:    %.c
	$(CC) $(CFLAGS) $*.c


