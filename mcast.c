#include "net_include.h"
#include "recv_dbg.h"
#define debug 1

void multicast(packet*, my_variables *);
int get_local_ipaddress();
void unicast(packet*, my_variables* ,int);
packet* create_packet(packet_type , payload_def *, int  , int);
int process_ip(packet *, int *, my_variables*);

void debug_log(char *);
void process_green(int *, my_variables *, int );

void send_green(my_variables* ,int *);

void handle_retransmission(my_variables *local_var);

int recvd_by_all(my_variables *local_var);

void write_to_file(my_variables *local_var);

void process_token(my_variables *local_var);

int main(int argc, char* argv[])
{

	//int debug=1;
	/*Pre-Initialization starts here*/
	struct sockaddr_in name;
	struct sockaddr_in multicast_addr,unicast_addr;

	int                mcast_addr;

	struct ip_mreq     mreq;
	unsigned char      ttl_val;

	int                ss,sr;
	fd_set             mask;
	fd_set             dummy_mask,temp_mask;
	int                bytes;
	int                num;
	packet*               mess_buf;
	char mess_buf_start[START_MSG_SIZE];
	char               input_buf[START_MSG_SIZE];

	mcast_addr = MCAST_ADDRESS /* (225.1.2.115*/;

	sr = socket(AF_INET, SOCK_DGRAM, 0); /* socket for receiving */
	if (sr<0) {
		perror("Mcast: socket");
		exit(1);
	}
	if(argc<5){
		printf("\nUsage: <mcast> <no_of_packets> <machine_index> <no_of_machines> <loss_rate>\n");
		exit(1);
	}



	name.sin_family = AF_INET;
	name.sin_addr.s_addr = INADDR_ANY;
	name.sin_port = htons(PORT);

	if ( bind( sr, (struct sockaddr *)&name, sizeof(name) ) < 0 ) {
		perror("Mcast: bind");
		exit(1);
	}

	mreq.imr_multiaddr.s_addr = htonl( mcast_addr );

	/* the interface could be changed to a specific interface if needed */
	mreq.imr_interface.s_addr = htonl( INADDR_ANY );


	if (setsockopt(sr, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&mreq, 
				sizeof(mreq)) < 0) 
	{
		perror("Mcast: problem in setsockopt to join multicast address" );
	}

	ss = socket(AF_INET, SOCK_DGRAM, 0); /* Socket for sending */
	if (ss<0) {
		perror("Mcast: socket");
		exit(1);
	}

	ttl_val = 1;
	if (setsockopt(ss, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&ttl_val, 
				sizeof(ttl_val)) < 0) 
	{
		printf("Mcast: problem in setsockopt of multicast ttl %d - ignore in WinNT or Win95\n", ttl_val );
	}

	/*Used for multicast*/
	multicast_addr.sin_family = AF_INET;
	multicast_addr.sin_addr.s_addr = htonl(mcast_addr);  /* mcast address */
	multicast_addr.sin_port = htons(PORT);

	/*Used for unicast*/
	unicast_addr.sin_family = AF_INET;
	unicast_addr.sin_addr.s_addr = htonl(mcast_addr);  /* mcast address */
	unicast_addr.sin_port = htons(PORT);



	FD_ZERO( &mask );
	FD_ZERO( &dummy_mask );
	FD_SET( sr, &mask );
	FD_SET( (long)0, &mask );    /* stdin */

	/*Pre-Initialization ends here*/


	/*Wait for start mcast*/
	for(;;){
		temp_mask=mask;
		num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);
		if(num>0){
			if ( FD_ISSET( sr, &temp_mask) ) {
				bytes = recv( sr, mess_buf_start, sizeof(mess_buf_start), 0 );
				mess_buf_start[bytes] = 0;
				//	printf( "received : %s\n", mess_buf );
				//	printf("%d",strcmp(mess_buf,"start"));
				//	printf("Done");
				if(strcmp(mess_buf_start,"start")==0){
					printf("\n Received Start\n");
					break;
				}
			}
		}
	}

	/*Got start mcast, now main protocol will start*/

	/*Defining data structures*/
	int token_id=-1;
	struct timeval timeout;
	timeout.tv_sec=0;

	int green_flag=0,i;
	//int recv_machine_id;
	//int recv_token_id;
	//int recv_type;
	//payload_def recv_payload;

	/*Initializing local structure*/
	my_variables local_var;

	/*This is my buffer of data packets*/
	packet* buffer[WINDOW]={NULL};

	/*Intializing file*/
	char file_name[80]={'\0'};
	strcat(file_name,argv[2]);
	strcat(file_name,".out");
	FILE *write = fopen(file_name,"w");

	/*Assigning local variables*/
	local_var.no_of_machines=atoi(argv[3]);
	local_var.machine_id=atoi(argv[2]);
	local_var.my_ip=get_local_ipaddress();
	local_var.ss=ss;
	local_var.multicast_addr=&multicast_addr;
	local_var.unicast_addr=&unicast_addr;
	local_var.my_state=INIT;
	local_var.tok=NULL;
	local_var.prev_tok=NULL;
	local_var.my_timeout=&timeout;
	local_var.buffer=buffer;
	local_var.local_aru=-1;
	local_var.my_file=write;

	int ip_table[local_var.no_of_machines]; // this is number of machines
	int green_table[local_var.no_of_machines];
	for(i=0;i<local_var.no_of_machines;i++){
		ip_table[i]=0;
		green_table[i]=0;
	}	
	ip_table[local_var.machine_id]=local_var.my_ip;

	recv_dbg_init(atoi(argv[4]),atoi(argv[2]));


	/*Creating payload for init packet*/
	payload_def init_payload;
	init_payload.ip_address= local_var.my_ip;
	packet* init_packet=create_packet(INIT_MSG,&init_payload,local_var.machine_id,token_id);
	local_var.my_timeout->tv_usec=INIT_TIMEOUT;
	/*send init_msg with IP*/
	multicast(init_packet,&local_var);
	if(debug){
		printf("\nINIT sent");
	}
	for(;;)
	{
		temp_mask = mask;
		num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, local_var.my_timeout);
		if (num > 0) {
			if ( FD_ISSET( sr, &temp_mask) ) {
				//bytes = recv( sr, mess_buf, sizeof(mess_buf), 0 );

				mess_buf=NULL;
				mess_buf=(packet*)malloc(sizeof(packet));
				bytes=recv_dbg(sr,(char*)mess_buf,sizeof(packet),0);
				if(debug){
					printf("\nReceived something");
				}
				if(mess_buf->machine_id==local_var.machine_id)
					continue;

				if(debug){
					printf("\nReceived from someone else");
				}

				switch(local_var.my_state){



					case INIT:
						if(debug){
							printf("\nInside INIT");
						}
						switch(mess_buf->type){

							case INIT_MSG:  if(!green_flag){
										debug_log("Inside green flag check");
										green_flag=process_ip(mess_buf,ip_table,&local_var);
										green_table[local_var.machine_id]=green_flag;
									}
									break;
							case INIT_REQ_IP:
									if(mess_buf->payload.ip_address==local_var.machine_id){ //this is request for my IP
										multicast(init_packet,&local_var);
									}
									break;
							case INIT_GREEN:
									process_green(green_table,&local_var,mess_buf->machine_id);
									break;
							case DATA:
									local_var.my_state=NO_TOKEN;
									local_var.my_timeout->tv_usec=NO_TOKEN_TIMEOUT;
									//process_data();
									break;


						}

						break;
					case HAS_TOKEN:
						break;
					case HAD_TOKEN:
						break;
					case NO_TOKEN:
						break;


				}
			}
		}
		else{
			switch(local_var.my_state){

				case INIT:
					if(green_flag){
						if(local_var.machine_id==0){
							process_green(green_table,&local_var,local_var.machine_id);							
						}
						else{

							send_green(&local_var,ip_table);
						}
					}
					else{

						int i;
						for(i=0;i<local_var.no_of_machines;i++){
							if(ip_table[i]==0){
								payload_def content;
								content.ip_address = i;


								packet *new_packet=create_packet(INIT_REQ_IP,&content,local_var.machine_id,-1);

								multicast(new_packet, &local_var);
							}
						}
					}
					break;


			}
		}
	}
	return 0;

}

void multicast(packet* send_buff, my_variables *local_var){


	sendto(local_var->ss, (char*)send_buff, sizeof(packet), 0,
			(struct sockaddr *)local_var->multicast_addr, sizeof(*(local_var->multicast_addr)));


}

void unicast(packet *send_buff, my_variables *local_var, int ip_address){


	(*(local_var->unicast_addr)).sin_addr.s_addr=ip_address;
	sendto(local_var->ss, (char*)send_buff, sizeof(packet), 0,
			(struct sockaddr *)local_var->unicast_addr, sizeof(*(local_var->unicast_addr)));

}

packet* create_packet(packet_type new_type, payload_def *content, int machine_id , int token_id){

	packet *new_packet=(packet*)malloc(sizeof(packet));
	//new_packet->payload=content;
	memcpy(&(new_packet->payload),content,sizeof(payload_def)); 
	new_packet->machine_id=machine_id;
	new_packet->token_id=token_id;
	new_packet->type=new_type;
	return new_packet;

}

int process_ip(packet *mess_buf, int *ip_table, my_variables *local_var){

	int i,machine_id=mess_buf->machine_id;
	ip_table[machine_id]=mess_buf->payload.ip_address;
	/*Check for green*/
	for(i=0;i<local_var->no_of_machines;i++){

		if(ip_table[i]==0)
			return 0;
	}
	if(local_var->machine_id!=0){

		send_green(local_var,ip_table);	
	}
	return 1;

}

void send_green(my_variables* local_var,int *ip_table){

	payload_def content;
	content.ip_address=-1;
	packet* new_packet=create_packet(INIT_GREEN,&content,local_var->machine_id, -1);
	unicast(new_packet, local_var, ip_table[0]);

}
void process_green(int *green_table, my_variables *local_var, int green_id){

	int i;
	green_table[green_id]=1;
	for(i=0;i<local_var->no_of_machines;i++){

		if(green_table[i]==0)
			return ;
	}
	debug_log("The world is green!!");
	token_def *init_token=(token_def*)malloc(sizeof(token_def));
	init_token->seq=-1;
	init_token->aru=-1;
	init_token->aru_id=-1;
	init_token->token_id=0;
	for(i=0;i<RTR_SIZE;i++){
		init_token->retransmission_list[i]=-1;
	}

	local_var->my_state=HAS_TOKEN;
	(local_var->my_timeout)->tv_usec=HAS_TOKEN_TIMEOUT;
	exit(0);
}
void debug_log(char *statement){
	//int debug = 1;
	if(debug){
		printf("\n %s\n",statement);
	}
}
void process_token(my_variables *local_var){
	handle_retransmission(local_var);
	write_to_file(local_var);
}

void write_to_file(my_variables *local_var){
	int seq_recvd=recvd_by_all(local_var);
	int i;
	for(i=local_var->prev_write_seq;i<=seq_recvd;i++){
		int index=i%WINDOW;
		packet *my_packet=local_var->buffer[index];
		fprintf(local_var->my_file,"%2d, %8d, %8d\n",my_packet->machine_id, my_packet->payload.data.sequence_num,my_packet->payload.data.random_num);
	}

}

/*This is received by all*/
int recvd_by_all(my_variables *local_var){

	int min=local_var->tok->aru;;
	if(local_var->prev_tok->aru < min)
		min=local_var->prev_tok->aru;

	return min;
}

void handle_retransmission(my_variables *local_var){
	int my_rtr[RTR_SIZE];
	int i,k=0;

	for(i=0;local_var->tok->retransmission_list[i]!=-1;i++){
		int index= (local_var->tok->retransmission_list[i])%WINDOW;
		if(local_var->buffer[index]!=NULL){
			multicast(local_var->buffer[index],local_var);
		}		
	}
	for(i=local_var->local_aru;i<=local_var->tok->seq;i++){
	
		int index= i%WINDOW;
		if(local_var->buffer[index]==NULL){
		
			my_rtr[k++]=i;
			if(k>=RTR_SIZE){
				break;
			}
		}
	}
	for(i=0;i<k;i++){
	
		local_var->tok->retransmission_list[i]=my_rtr[i];
	}
	for(k=i;k<RTR_SIZE;k++){
				
		local_var->tok->retransmission_list[i]=-1;
	}
}
