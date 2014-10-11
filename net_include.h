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
#include <time.h>

#define PORT	     10150


#define MAX_MESS_LEN 1400


/*Added definitions*/
#define MCAST_ADDRESS 225 << 24 | 1 << 16 | 2 << 8 | 115
#define WINDOW 1000
#define DATA_SIZE 1200
#define HAD_TOKEN_TIMEOUT 100
#define INIT_TIMEOUT 10000
#define NO_TOKEN_TIMEOUT 100
#define HAS_TOKEN_TIMEOUT 100
#define START_MSG_SIZE 80
#define RTR_SIZE 296
#define INDV_WINDOW 50
#define RANDOM 1000000
typedef enum {INIT_MSG=0,INIT_REQ_IP, INIT_GREEN,TOKEN, DATA, QUIT } packet_type;
typedef enum {INIT=0,HAS_TOKEN, HAD_TOKEN,NO_TOKEN } state;

typedef struct  token_def{

	int seq;
	int aru;
	int aru_id;
	int token_id; //handle retransmissions
	int retransmission_list[RTR_SIZE] ;
}token_def;

typedef struct data_def{
	int sequence_num;
	char data_content[DATA_SIZE];
	int random_num;

}data_def;

typedef union payload_def{

	token_def token ;
	int ip_address; 
	data_def data;

}payload_def;


typedef struct packet{
	packet_type type; 
	payload_def payload;	
	int machine_id;
	int token_id; //corresponding to which token version this packet was sent
}packet;

typedef struct my_variables{

	int ss;
	struct sockaddr_in *multicast_addr;
	struct sockaddr_in *unicast_addr;
	int machine_id;
	int no_of_machines;
	int my_ip;
	state my_state;
	token_def *tok;
	int prev_token_seq;
	int prev_token_id;
	int prev_token_aru;
	struct timeval *my_timeout;
	packet** buffer;
	int local_aru;
	int prev_write_seq;
	FILE *my_file;
	int packets_sent;
	int total_packets;
	int eof_flag;
	int current_packets_sent;
}my_variables;
