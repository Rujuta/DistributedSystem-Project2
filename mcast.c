#include "net_include.h"
#include "recv_dbg.h"
#define debug 0

void multicast(packet*, my_variables *);
int get_local_ipaddress();
void unicast(packet*, my_variables* ,int);
packet* create_packet(packet_type , payload_def *, int  , int);
int process_ip(packet *, int *, my_variables*);

void debug_log(char *);
void process_green(int *, my_variables *, int ,int *);

void send_green(my_variables* ,int *);

void handle_retransmission(my_variables *local_var);

int recvd_by_all(my_variables *local_var);

void write_to_file(my_variables *local_var);

void process_token(my_variables *local_var,int *ip_table);

void send_packets(my_variables *local_var);

void update_token(my_variables *local_var);

void check_eof(my_variables *local_var);

void process_data(my_variables *local_var, packet *mess_buf);

void start_exit(my_variables* local_var);

void process_quit(my_variables *local_var, packet *mess_buf);


void calculate_time(my_variables *local_var);


void send_ip_request(my_variables *local_var, int* ip_table);



/*Logs written to this file*/

FILE *log1=NULL;
int main(int argc, char* argv[])
{

	//int debug=1;
	//
	/*Pre-Initialization starts here*/
	/**The sock addrs are initialized here, there is one sockaadr_in for unicast addresses and one for multicast address
	 * The sending and receiving sockets are created
	 * The multicast address is initialized to the value set up for us*/
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

	//ttl_val = 1;
	ttl_val=1;
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

	/*file log*/

	log1=fopen(argv[2],"w");
	if(log1 == NULL){
		printf("\nError opening file");
		exit(1);

	}
	/*Wait for start mcast*/
	for(;;){
		temp_mask=mask;
		num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);
		if(num>0){
			if ( FD_ISSET( sr, &temp_mask) ) {
				bytes = recv( sr, mess_buf_start, sizeof(mess_buf_start), 0 );
				mess_buf_start[bytes] = 0;
				//	fprintf(log1,, "received : %s\n", mess_buf );
				//	fprintf(log1,,"%d",strcmp(mess_buf,"start"));
				//	fprintf(log1,"Done");
				if(strcmp(mess_buf_start,"start")==0){
					fprintf(log1,"\n Received Start\n");
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

	/*Green flag is set when process receives IP from all other machines*/
	int green_flag=0,i;
	//int recv_machine_id;
	//int recv_token_id;
	//int recv_type;
	//payload_def recv_payload;

	/*Initializing local structure*/
	my_variables local_var;

	/*This is my buffer of data packets BUF_SIZE=2*WINDOW WINDOW is the maximum number of unacknowledged packets that can exist
	 *          * In the worse case scenario, 2*WINDOW number of unacked packets can exist - hence when there are unacked packets ,we need place to store them in case they have not been delivered*/
	packet* buffer[BUF_SIZE]={NULL};

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
	/*This is pointer to the token I receive*/
	local_var.tok=NULL;
	local_var.my_timeout=&timeout;
	local_var.buffer=buffer;
	local_var.local_aru=-1;
	local_var.my_file=write;
	local_var.eof_flag=0;
	local_var.prev_token_seq=-2;
	local_var.prev_token_aru=-2;
	local_var.prev_token_id=-2;
	local_var.prev_write_seq=0;
	local_var.packets_sent=0;
	local_var.total_packets=atoi(argv[1]);
	local_var.current_packets_sent=0;



	/*Gettime of day variable*/
	struct timeval report_time;
	gettimeofday(&report_time,NULL);
	local_var.start_time=report_time.tv_sec+(report_time.tv_usec/TIMECONV);




	/*Initializing random*/
	srand(time(NULL));
	int ip_table[local_var.no_of_machines]; // this is number of machines
	int green_table[local_var.no_of_machines];

	/*Set IP table and Green table to 0 initially*/
	for(i=0;i<local_var.no_of_machines;i++){
		ip_table[i]=0;
		green_table[i]=0;
	}	
	ip_table[local_var.machine_id]=local_var.my_ip;

	recv_dbg_init(atoi(argv[4]),atoi(argv[2]));


	/*Init process starts here*/
	/*Creating payload for init packet*/
	payload_def init_payload;
	init_payload.ip_address= local_var.my_ip;
	packet* init_packet=create_packet(INIT_MSG,&init_payload,local_var.machine_id,token_id);
	local_var.my_timeout->tv_usec=INIT_TIMEOUT;
	/*send init_msg with IP*/
	multicast(init_packet,&local_var);

	multicast(init_packet,&local_var);

	payload_def *tkn;
	if(debug){
		fprintf(log1,"\nINIT sent");
	}
	for(;;)
	{
		temp_mask = mask;
		num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, local_var.my_timeout);
		if(debug){
			fprintf(log1,"in state : %d",local_var.my_state);
			fflush(log1);

		}
		if (num > 0) {
			if ( FD_ISSET( sr, &temp_mask) ) {
				//bytes = recv( sr, mess_buf, sizeof(mess_buf), 0 );

				mess_buf=NULL;
				mess_buf=(packet*)malloc(sizeof(packet));
				bytes=recv_dbg(sr,(char*)mess_buf,sizeof(packet),0);

				/*If message is from self ignore it*/
				if(mess_buf->machine_id==local_var.machine_id)
					continue;


				switch(local_var.my_state){



					case INIT:
						if(debug){
							fprintf(log1,"\nInside INIT");
						}
						switch(mess_buf->type){

							case INIT_MSG:  if(!green_flag){ /*On receiving an INIT message, go into processing the IP - insert it into ip_table*/
										debug_log("Inside green flag check");
										green_flag=process_ip(mess_buf,ip_table,&local_var);
										green_table[local_var.machine_id]=green_flag;
									}
									break;
							case INIT_REQ_IP: /*Message is requesting an IP address*/
									if(mess_buf->payload.ip_address==local_var.machine_id){ //this is request for my IP
										/*Creating payload for init packet*/
										payload_def init_payload;
										init_payload.ip_address= local_var.my_ip;
										packet* init_packet=create_packet(INIT_MSG,&init_payload,local_var.machine_id,token_id);


										multicast(init_packet,&local_var);
									}
									break;
							case INIT_GREEN: /*This message will only be received by process 0 - some other process is sending it the green signal*/
									process_green(green_table,&local_var,mess_buf->machine_id,ip_table);
									break;
							case DATA:
									/*Only a process other than process 0 will receive this message in INIT state. It indicates, process 0
									 * has started sending messages. This process goes into NO_TOKEN state after this*/
									calculate_time(&local_var);
									local_var.my_state=NO_TOKEN;
									local_var.my_timeout->tv_usec=NO_TOKEN_TIMEOUT;
									local_var.my_timeout->tv_sec=NO_TOKEN_TIMEOUT_SEC;
									process_data(&local_var,mess_buf);
									break;
							case TOKEN:/*I need to go into HAS TOKEN if this is true*/ 
									if((mess_buf->token_id)+1 > local_var.prev_token_id){
										local_var.my_state=HAS_TOKEN;
										local_var.my_timeout->tv_usec=HAS_TOKEN_TIMEOUT;
										local_var.my_timeout->tv_sec=HAS_TOKEN_TIMEOUT_SEC;

										local_var.tok=&(mess_buf->payload.token);
										process_token(&local_var,ip_table);
									}

									break;



						}

						break;
					case HAS_TOKEN:
						break;
					case HAD_TOKEN: /*When a process JUST had the token and sent it, it enters this state*/	

						switch(mess_buf->type){
							case TOKEN:/*I need to go into HAS TOKEN if this is true*/ 
								if((mess_buf->token_id)+1 > local_var.prev_token_id){
									local_var.my_state=HAS_TOKEN;
									local_var.my_timeout->tv_usec=HAS_TOKEN_TIMEOUT;
									local_var.my_timeout->tv_sec=HAS_TOKEN_TIMEOUT_SEC;
									local_var.tok=&(mess_buf->payload.token);
									process_token(&local_var,ip_table);
								}

								break;
							case DATA: /*On getting data packet, processes data and goes into NO_TOKEN state because it is sure
								     some other process has received token and it is not responsible for retransmission*
								     anymore in case of token loss and subsequent TIMEOUT**/
								if((mess_buf->token_id) > local_var.prev_token_id){
									debug_log("I am in HADTOKEN, processing data");
									local_var.my_state=NO_TOKEN;
									local_var.my_timeout->tv_usec=NO_TOKEN_TIMEOUT;
									local_var.my_timeout->tv_sec=NO_TOKEN_TIMEOUT_SEC;
									if(mess_buf->payload.data.sequence_num>local_var.local_aru){

										process_data(&local_var,mess_buf);

									}
								}
								break;
							case QUIT:
								/*A packet signalling quit to this process.*/
								process_quit(&local_var,mess_buf);
								break;

						}

						break;
					case NO_TOKEN:
						switch(mess_buf->type){

							case TOKEN:/*I need to go into HAS TOKEN if this is true*/ 
								if((mess_buf->token_id)+1 > local_var.prev_token_id){
									local_var.my_state=HAS_TOKEN;
									local_var.my_timeout->tv_usec=HAS_TOKEN_TIMEOUT;
									local_var.my_timeout->tv_sec=HAS_TOKEN_TIMEOUT_SEC;
									local_var.tok=&(mess_buf->payload.token);
									process_token(&local_var,ip_table);
								}

								break;
							case DATA: /*Simply process data, if the sequence number > what I have received so far*/

								if(mess_buf->payload.data.sequence_num>local_var.local_aru){

									process_data(&local_var,mess_buf);

								}
								//process_data();
								break;
							case QUIT:	process_quit(&local_var,mess_buf); /*Begin QUIT process*/

									break;



						}

						break;


				}
			}
		}
		else{ /*When a process times out, it again switches on state to execute different actions*/
			int i;

			switch(local_var.my_state){

				case INIT: /*Timeout is on INIT, so maybe 'green signal' needs to be resent to process 
					     or IP address may need to be sent*/
					if(green_flag){
						if(local_var.machine_id==0){
							process_green(green_table,&local_var,local_var.machine_id,ip_table);							
						}
						else{

							send_green(&local_var,ip_table);
						}
					}
					else{

						send_ip_request(&local_var,ip_table);
						/*int i;
						  for(i=0;i<local_var.no_of_machines;i++){
						  if(ip_table[i]==0){
						  payload_def content;
						  content.ip_address = i;


						  packet *new_packet=create_packet(INIT_REQ_IP,&content,local_var.machine_id,-1);

						  multicast(new_packet, &local_var);
						  }
						  }*/
					}
					break;
				case HAD_TOKEN: /*Token is lost, so I need to resend the token*/

					//		payload_def *tkn;

					if(debug){
						fprintf(log1,"\nin had token timeout ..resending token..to machine ..%d..with ip ..%d..with token id....%d\n",(local_var.machine_id)+1,ip_table[((local_var.machine_id)+1)%local_var.no_of_machines],(local_var.tok->token_id)-1);
						fflush(log1);
					}
					tkn=(payload_def*)local_var.tok;
					packet *t = create_packet(TOKEN,tkn,local_var.machine_id,(local_var.tok->token_id)-1);
					for(i = 0; i<REDUNDANCY/2; i++){
						unicast(t,&local_var,ip_table[((local_var.machine_id)+1)%local_var.no_of_machines]);
					}
					break;
				case NO_TOKEN: /*Almost a minute is over since you didn't receive data, something is wrong*/
					printf("In NO_TOKEN timeout");
					fprintf(log1,"In NO_TOKEN timeout....exiting");

					calculate_time(&local_var);
					exit(0);
					break;
				case HAS_TOKEN:
					printf("In HAS TOKEN timeout");
					fprintf(log1,"In HAS_TOKEN timeout...exiting");
					exit(0);
					break;



			}
		}
	}
	return 0;

}

/*Function to multicast packets - multicast address set in sockaddr_in multicast*/
void multicast(packet* send_buff, my_variables *local_var){


	sendto(local_var->ss, (char*)send_buff, sizeof(packet), 0,
			(struct sockaddr *)local_var->multicast_addr, sizeof(*(local_var->multicast_addr)));


}

/*Function to unicast. used only by a process in HAS_TOKEN state to send token to next process*/
void unicast(packet *send_buff, my_variables *local_var, int ip_address){


	(*(local_var->unicast_addr)).sin_addr.s_addr=ip_address;
	sendto(local_var->ss, (char*)send_buff, sizeof(packet), 0,
			(struct sockaddr *)local_var->unicast_addr, sizeof(*(local_var->unicast_addr)));

}
/*Packet of specified type created*/
packet* create_packet(packet_type new_type, payload_def *content, int machine_id , int token_id){

	packet *new_packet=(packet*)malloc(sizeof(packet));
	//new_packet->payload=content;
	memcpy(&(new_packet->payload),content,sizeof(payload_def)); 
	new_packet->machine_id=machine_id;
	new_packet->token_id=token_id;
	new_packet->type=new_type;
	return new_packet;

}
/*This function inserts IP of machine in ip_table indexed by its machine id*/
int process_ip(packet *mess_buf, int *ip_table, my_variables *local_var){

	if(debug){

		fprintf(log1,"Entering Process_ip ..%d",mess_buf->machine_id);

	}
	int i,machine_id=mess_buf->machine_id;
	if(ip_table[machine_id]==0){
		ip_table[machine_id]=mess_buf->payload.ip_address;
	}

	if(debug){

		fprintf(log1,"\n%d",ip_table[machine_id]);

	}

	/*Check for green If all IP's exist, send green signal to process 0 and return 1 to set your own green flag*/
	for(i=0;i<local_var->no_of_machines;i++){

		if(ip_table[i]==0)
			return 0;
	}
	if(local_var->machine_id!=0){

		send_green(local_var,ip_table);	
	}

	debug_log("Exiting Process_ip");
	return 1;

}
/*Send a green to process 0*/
void send_green(my_variables* local_var,int *ip_table){

	debug_log("Entering send_green");
	payload_def content;
	content.ip_address=-1;
	packet* new_packet=create_packet(INIT_GREEN,&content,local_var->machine_id, -1);
	unicast(new_packet, local_var, ip_table[0]);

}
/*When a green signal is received by process 0, it sets the flag in green_table indexed by machine id sending
 * incoming green flag. If all values in green table are set, it creates a token and starts sending data*/
void process_green(int *green_table, my_variables *local_var, int green_id,int *ip_table){

	debug_log("Entering Process_green");
	int i;

	green_table[green_id]=1;
	if(green_table[0] != 1){
		send_ip_request(local_var,ip_table);
	}

	for(i=0;i<local_var->no_of_machines;i++){

		if(green_table[i]==0){
			if(debug){
				fprintf(log1,"Not green yet %d", i);
			}
			return ;
		}
	}
	debug_log("The world is green!!");
	token_def *init_token=(token_def*)malloc(sizeof(token_def));
	init_token->seq=-1;
	init_token->aru=-1;
	init_token->aru_id=0;
	init_token->token_id=0;
	for(i=0;i<RTR_SIZE;i++){
		init_token->retransmission_list[i]=-1;
	}
	calculate_time(local_var);
	local_var->my_state=HAS_TOKEN;
	(local_var->my_timeout)->tv_usec=HAS_TOKEN_TIMEOUT;
	(local_var->my_timeout)->tv_sec=HAS_TOKEN_TIMEOUT_SEC;
	local_var->tok=init_token;
	process_token(local_var,ip_table);
}
void debug_log(char *statement){
	//int debug = 1;
	if(debug){
		fprintf(log1,"\n %s\n",statement);
		fflush(log1);	
	}
}
/*On receiving a token, a process executes these actions*/
void process_token(my_variables *local_var,int *ip_table){


	debug_log("Entering Process_token");
	int i;

	if(debug){
		fprintf(log1,"\nprocessing token id %d\n",local_var->tok->token_id);
		fflush(log1);
	}
	/*Checks if the total number of packets to be sent are sent*/
	if(local_var->total_packets == local_var->packets_sent){
		check_eof(local_var);	

	}
	/*Handle retransmissions*/
	handle_retransmission(local_var);

	//	update_token(local_var);
	/*Deliver packets - make their locations NULL subsequently, safe delivery*/
	write_to_file(local_var);
	/*Send my share of data packets*/
	send_packets(local_var);
	/*Update token and related fields*/
	update_token(local_var);
	//	check_eof(local_var);	
	/*Unicast the TOKEN*/
	payload_def *tkn;
	tkn=(payload_def*)local_var->tok;
	packet *t = create_packet(TOKEN,tkn,local_var->machine_id,(local_var->tok->token_id)-1);
	for(i = 0; i<REDUNDANCY; i++){
		unicast(t,local_var,ip_table[((local_var->machine_id)+1)%local_var->no_of_machines]);
	}
	local_var->my_state=HAD_TOKEN;
	(local_var->my_timeout)->tv_usec=HAD_TOKEN_TIMEOUT;
}
/*Write data to file*/
void write_to_file(my_variables *local_var){

	debug_log("Entering write to file");
	/*This value returns the sequence number that every process is guaranteed to have
	 *          * min(previous token ARU, current TOKEN aru)*/
	int seq_recvd=recvd_by_all(local_var);
	if(debug){

		fprintf(log1,"\nsequence recvd: %d previous write seq %d",seq_recvd,local_var->prev_write_seq);
	}
	if(!(seq_recvd < 0)){
		int i;
		//fprintf(log1,"\nPrevious sequence is :%d",local_var->prev_write_seq);
		//
		/*Write to file only uptill what is received by everyone - make those locations NULL*/
		for(i=local_var->prev_write_seq;i<=seq_recvd;i++){
			int index=i%BUF_SIZE;

			//fprintf(log1,"index is :%d",index);
			packet *my_packet=local_var->buffer[index];
			fprintf(local_var->my_file,"%2d, %8d, %8d\n",my_packet->machine_id, my_packet->payload.data.sequence_num,my_packet->payload.data.random_num);
			//	free(local_var->buffer[index]);
			//fprintf(log1,"\nMaking contents of %d NULL", index);
			local_var->buffer[index]=NULL;
		}
		local_var->prev_write_seq=seq_recvd+1;
	}
	if(debug){

		fprintf(log1,"\nprevious write seq %d",seq_recvd,local_var->prev_write_seq);
		fflush(log1);	
	}
}
/*Send packets when process has token*/
void send_packets(my_variables *local_var){


	debug_log("Entering send_packets");
	int seq_recvd=recvd_by_all(local_var);
	int i;
	int no_of_packets= (WINDOW)-(local_var->tok->seq-seq_recvd);
	/*If space is greater than individual window size I send INDV_WINDOW of packets*/
	int count=no_of_packets>INDV_WINDOW?INDV_WINDOW:no_of_packets;
	count=count>(local_var->total_packets - local_var->packets_sent)?((local_var->total_packets - local_var->packets_sent)):count;
	if(local_var->tok->seq==local_var->local_aru){
		local_var->local_aru+=count;

	}

	local_var->current_packets_sent=count;	
	payload_def content;
	for(i=0;i<count;i++){
		content.data.random_num=rand()%RANDOM+1;
		content.data.sequence_num=local_var->tok->seq+1+i;
		packet* data_packet=create_packet(DATA,&content,local_var->machine_id,local_var->tok->token_id);
		int index=content.data.sequence_num%BUF_SIZE;
		local_var->buffer[index]=data_packet;
		multicast(data_packet,local_var);
	}
	local_var->packets_sent+=count;

	if(debug){
		fprintf(log1,"\nsent %d packets uptill seq num : %d..",count,local_var->tok->seq+count);

		fflush(log1);	
	}

	//local_var->tok->seq+=local_var->current_packets_sent;



}

/*This is received by all*/
int recvd_by_all(my_variables *local_var){


	debug_log("Entering recvd_by_all");
	int min=local_var->tok->aru;;
	if(local_var->prev_token_aru < min)
		min=local_var->prev_token_aru;

	return min;
}

void handle_retransmission(my_variables *local_var){


	debug_log("Entering handle_retransmission");
	int my_rtr[RTR_SIZE];
	int i,k=0;

	/*We're checking what we can retransmit*/
	for(i=0; (local_var->tok->retransmission_list[i]!=-1) || (i>=RTR_SIZE) ;i++){
		int index= (local_var->tok->retransmission_list[i])%BUF_SIZE;

		if(debug){
			fprintf(log1,"\nfirst retransmiting %d.. \t",local_var->tok->retransmission_list[i]);

			fflush(log1);	
		}

		if(local_var->buffer[index]!=NULL){

			local_var->buffer[index]->token_id = local_var->tok->token_id;
			if(local_var->buffer[index]->payload.data.sequence_num!=local_var->tok->retransmission_list[i]){
				fprintf(log1,"\nmessed up...exitiing ...buffer seq num = %d...retrans seq num = %d",local_var->buffer[index]->payload.data.sequence_num,local_var->tok->retransmission_list[i]);
				fflush(log1);
				printf("Messed up!!");
				exit(1);
			}

			multicast(local_var->buffer[index],local_var);
			if(debug){
				fprintf(log1,"second retransmiting %d.. \t",local_var->buffer[index]->payload.data.sequence_num);

				fflush(log1);	
			}

		}	



	}


	if(debug){
		fprintf(log1,"\nlocal aru is %d",local_var->local_aru);
		fflush(log1);	
	}

	/*Filling up our retransmission list*/
	k=0;

	for(i=local_var->local_aru+1;i<=local_var->tok->seq;i++){

		int index= i%BUF_SIZE;
		if(local_var->buffer[index]==NULL){

			my_rtr[k++]=i;
			if(k>=RTR_SIZE){
				break;
			}
		}
	}

	/*Putting my list in token*/
	for(i=0;i<k;i++){

		local_var->tok->retransmission_list[i]=my_rtr[i];

		if(debug){

			fprintf(log1,"\nrtr list i ...%d ....seq...%d \t", i , my_rtr[i]);
			fflush(log1);	

		}
	}
	/*Setting rest to -1*/
	for(k=i;k<RTR_SIZE;k++){

		local_var->tok->retransmission_list[i]=-1;
	}

	debug_log("Exiting h rtr");
}

void update_token(my_variables *local_var){


	debug_log("Entering update_token");
	//fprintf(log1,"previous Token sequence: %d",local_var->prev_token_seq);

	local_var->prev_token_id=local_var->tok->token_id;
	//local_var->prev_token_aru=local_var->tok->aru;
	//local_var->prev_token_seq=local_var->tok->seq;

	local_var->tok->token_id++;
	local_var->tok->seq+=local_var->current_packets_sent;
	if(local_var->local_aru<(local_var->tok->aru)){
		local_var->tok->aru=local_var->local_aru;
		local_var->tok->aru_id=local_var->machine_id;
	}
	else{
		if(local_var->tok->aru_id==local_var->machine_id){

			local_var->tok->aru=local_var->local_aru;
			local_var->tok->aru_id=local_var->machine_id;
		}
	}

	local_var->prev_token_aru=local_var->tok->aru;
	local_var->prev_token_seq=local_var->tok->seq;


	debug_log("Now token ID is");
	//fprintf(log1,"%d",local_var->tok->token_id);

	if(debug){
		fprintf(log1,"\nCurrent Token sequence: %d",local_var->tok->seq);
		fprintf(log1,"\nprevious Token sequence: %d",local_var->prev_token_seq);
		fprintf(log1,"\nprevious Token aru: %d",local_var->prev_token_aru);
		fprintf(log1,"\ncurrent Token aru: %d",local_var->tok->aru);

		fprintf(log1,"\ncurrent local aru: %d",local_var->local_aru);
		fflush(log1);	


	}
	//if(local_var->tok->token_id>20){

	//	exit(0);

	//}
}
/*Set flag if EOF process is supposed to begin
 *  * When token's ARU is equal to TOKEN's sequence number and the previous token's sequence number is same as current sequence number 
 *   * for 2 consecutive rounds, then no one has anything new to send as well as everybody has received everything sent so far*/

void check_eof(my_variables *local_var){


	debug_log("Entering check_eof");

	//fprintf(log1,"tok aru:  %d ...tok seq %d....prev tok seq %d.... ",local_var->tok->aru,local_var->tok->seq,local_var->prev_token_seq);

	if(local_var->tok->aru==local_var->tok->seq &&  local_var->prev_token_seq==local_var->tok->seq ){
		debug_log("In first if EOF ");
		if(local_var->eof_flag){
			write_to_file(local_var);
			start_exit(local_var);	

			debug_log("In 2 if EOF ");
		}
		else{

			debug_log("In first else EOF ");
			local_var->eof_flag=1;
		}


	}
	else{


		debug_log("In second else EOF ");
		local_var->eof_flag=0;
	}

	debug_log("Exiting check_eof");

}

/*The first process to realize that everyone has received everything starts the exit procedure
 *  * Sends the first QUIT packet with a COUNTER set, which indicates to the next process how many quit packets
 *  it needs to send before quitting*/
void start_exit(my_variables* local_var){

	int i;
	if(debug){
		fprintf(log1,"\nToken id %d",local_var->tok->token_id);
	}
	debug_log("Entering start_exit");
	payload_def quit_payload;
	quit_payload.ip_address= 10;
	packet* quit_packet=create_packet(QUIT,&quit_payload,local_var->machine_id,-1);
	for(i=0;i<10;i++){
		multicast(quit_packet,local_var);
	}
	fclose(local_var->my_file);
	printf("\nYippee I'm done, exiting, my machine ID is %d",local_var->machine_id);
	calculate_time(local_var);
	exit(0);
}

void calculate_time(my_variables *local_var){

	struct timeval end_time;
	gettimeofday(&end_time,NULL);
	double total_time=end_time.tv_sec + (end_time.tv_usec/TIMECONV);
	double time_taken=total_time-local_var->start_time;
	if(debug){
		fprintf(log1,"\nTime taken is %lf seconds ",time_taken);

	}
	printf("\nTime taken is %lf seconds ",time_taken);
}
/*Functions for receive data*/
void process_data(my_variables *local_var, packet *mess_buf){

	debug_log("Entering process_data");
	int i;
	int wflag=0;
	int sequence=mess_buf->payload.data.sequence_num;
	if(debug){
		fprintf(log1,"\nSequence number is %d",sequence);
		fprintf(log1,"\nARU is %d",local_var->local_aru);
		fflush(log1);	
	}
	if(local_var->buffer[sequence%BUF_SIZE]==NULL){
		local_var->buffer[sequence%BUF_SIZE]=mess_buf;
		wflag==1;
	}
	else{
		if(debug){
			fprintf(log1,"\n location NOT NULL\n");
			fprintf(log1,"\n Value at location is : %d",local_var->buffer[sequence%BUF_SIZE]->payload.data.sequence_num);

			fflush(log1);	
		}
		if(local_var->buffer[sequence%BUF_SIZE]->payload.data.sequence_num != sequence){
			if(debug){
				printf("dude...fucked..");
			}
			exit(1);
		}

	}
	/*In order packet*/
	if(local_var->local_aru+1==sequence){


		i=local_var->local_aru+1;

		int cnt=0;
		int null_flag=1;
		while(local_var->buffer[i%BUF_SIZE]!=NULL){
			//fprintf(log1,"\nsequence number is LOCATION :: %d local ARU %d ",local_var->buffer[i%BUF_SIZE]->payload.data.sequence_num, local_var->local_aru);
			if(local_var->buffer[i%BUF_SIZE]->payload.data.sequence_num>=local_var->local_aru)
			{
				local_var->local_aru++;

			}
			else{
				break;
			}
			//cnt++;
			/*if(cnt==WINDOW){
			//null_flag=0;
			break;
			}*/
			if(debug){
				fprintf(log1,"\tlocal aru is %d",local_var->local_aru);
				fflush(log1);	
			}

			i++;
		}
		//	if (null_flag==1)
		//		local_var->local_aru+=cnt;
		//	else
		//local_var->local_aru=sequence;
	}

	if(debug){
		fprintf(log1,"\nlocal aru is %d",local_var->local_aru);
		fflush(log1);	
	}

	debug_log("Exiting process_data");
}
/*Processes that receive QUIT messages execute this before quitting*/
void process_quit(my_variables *local_var, packet *mess_buf){

	int i;
	debug_log("Entering process_quit");
	write_to_file(local_var);
	payload_def quit_payload;
	quit_payload.ip_address= mess_buf->payload.ip_address-1;
	packet* quit_packet=create_packet(QUIT,&quit_payload,local_var->machine_id,-1);
	for(i=0;i<mess_buf->payload.ip_address-1;i++){
		multicast(quit_packet,local_var);
	}
	fclose(local_var->my_file);
	printf("\nYippee I'm done, exiting, my machine ID is %d",local_var->machine_id);	

	calculate_time(local_var);
	exit(0);


}

void send_ip_request(my_variables *local_var, int* ip_table){

	int i;
	for(i=0;i<local_var->no_of_machines;i++){
		if(ip_table[i]==0){
			payload_def content;
			content.ip_address = i;
			if(debug){
				fprintf(log1,"\n%d ip not present\n",i);
				fflush(log1);	
			}
			packet *new_packet=create_packet(INIT_REQ_IP,&content,local_var->machine_id,-1);

			multicast(new_packet, local_var);
		}
	}


}
