#include <stdio.h>

#include <stdlib.h>

#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h> 
#include <netdb.h>

#include <errno.h>

#define PORT	     10150


#define MAX_MESS_LEN 1400

/*Added definitions*/
#define MCAST_ADDRESS 225 << 24 | 1 << 16 | 2 << 8 | 115
#define WINDOW 1000
#define DATA_SIZE 1200
#define HAD_TOKEN_TIMEOUT 100
#define INIT_TIMEOUT 100

typedef enum {INIT_MSG=0,INIT_REQ_IP, TOKEN, DATA, QUIT } packet_type;

typedef struct  token_def{

	int seq;
	int aru;
	int aru_id;
	int token_id; //handle retransmissions
	int retransmission_list[WINDOW] ;
}token_def;

typedef struct data_def{
	unsigned int sequence_num;
	char data[DATA_SIZE];
	unsigned int random_num;

}data_def;

typedef union payload_def{

	token_def token ;
	long ip_address; 
	data_def data;

}payload_def;


typedef struct packet{
	packet_type type; 
	payload_def payload;	
	int machine_id;
	int token_id; //corresponding to which token version this packet was sent
}packet;

