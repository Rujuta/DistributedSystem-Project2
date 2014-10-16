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

int main(int argc, char* argv[])
{

	//int debug=1;
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

	payload_def *tkn;
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

				/*If message is from self*/
				if(mess_buf->machine_id==local_var.machine_id)
					continue;


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
									process_green(green_table,&local_var,mess_buf->machine_id,ip_table);
									break;
							case DATA:
									local_var.my_state=NO_TOKEN;
									local_var.my_timeout->tv_usec=NO_TOKEN_TIMEOUT;
									process_data(&local_var,mess_buf);
									break;
							case TOKEN:/*I need to go into HAS TOKEN if this is true*/ 
									if((mess_buf->token_id)+1 > local_var.prev_token_id){
										local_var.my_state=HAS_TOKEN;
										local_var.my_timeout->tv_usec=HAS_TOKEN_TIMEOUT;
										local_var.tok=&(mess_buf->payload.token);
										process_token(&local_var,ip_table);
									}

									break;



						}

						break;
					case HAS_TOKEN:
						break;
					case HAD_TOKEN:	

						switch(mess_buf->type){
							case TOKEN:/*I need to go into HAS TOKEN if this is true*/ 
								if((mess_buf->token_id)+1 > local_var.prev_token_id){
									local_var.my_state=HAS_TOKEN;
									local_var.my_timeout->tv_usec=HAS_TOKEN_TIMEOUT;
									local_var.tok=&(mess_buf->payload.token);
									process_token(&local_var,ip_table);
								}

								break;
							case DATA:
								if((mess_buf->token_id) > local_var.prev_token_id){
									debug_log("I am in HADTOKEN, processing data");
									local_var.my_state=NO_TOKEN;
									local_var.my_timeout->tv_usec=NO_TOKEN_TIMEOUT;
									if(mess_buf->payload.data.sequence_num>local_var.local_aru){

										process_data(&local_var,mess_buf);

									}
								}
								break;
							case QUIT:

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
									local_var.tok=&(mess_buf->payload.token);
									process_token(&local_var,ip_table);
								}

								break;
							case DATA:

								if(mess_buf->payload.data.sequence_num>local_var.local_aru){

									process_data(&local_var,mess_buf);

								}
								//process_data();
								break;
							case QUIT:	process_quit(&local_var,mess_buf);

									break;



						}

						break;


				}
			}
		}
		else{
			switch(local_var.my_state){

				case INIT:
					if(green_flag){
						if(local_var.machine_id==0){
							process_green(green_table,&local_var,local_var.machine_id,ip_table);							
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
				case HAD_TOKEN:

					//		payload_def *tkn;
					tkn=(payload_def*)local_var.tok;
					packet *t = create_packet(TOKEN,tkn,local_var.machine_id,(local_var.tok->token_id)-1);
					unicast(t,&local_var,ip_table[((local_var.machine_id)+1)%local_var.no_of_machines]);
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

	debug_log("Entering Process_ip");
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

	debug_log("Exiting Process_ip");
	return 1;

}

void send_green(my_variables* local_var,int *ip_table){

	debug_log("Entering send_green");
	payload_def content;
	content.ip_address=-1;
	packet* new_packet=create_packet(INIT_GREEN,&content,local_var->machine_id, -1);
	unicast(new_packet, local_var, ip_table[0]);

}
void process_green(int *green_table, my_variables *local_var, int green_id,int *ip_table){

	debug_log("Entering Process_green");
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
	init_token->aru_id=0;
	init_token->token_id=0;
	for(i=0;i<RTR_SIZE;i++){
		init_token->retransmission_list[i]=-1;
	}

	local_var->my_state=HAS_TOKEN;
	(local_var->my_timeout)->tv_usec=HAS_TOKEN_TIMEOUT;
	local_var->tok=init_token;
	process_token(local_var,ip_table);
}
void debug_log(char *statement){
	//int debug = 1;
	if(debug){
		printf("\n %s\n",statement);
	}
}
void process_token(my_variables *local_var,int *ip_table){


	debug_log("Entering Process_token");
	if(local_var->total_packets == local_var->packets_sent){
		check_eof(local_var);	

	}
	handle_retransmission(local_var);

	//	update_token(local_var);
	write_to_file(local_var);
	send_packets(local_var);
	update_token(local_var);
	//	check_eof(local_var);	
	payload_def *tkn;
	tkn=(payload_def*)local_var->tok;
	packet *t = create_packet(TOKEN,tkn,local_var->machine_id,(local_var->tok->token_id)-1);
	unicast(t,local_var,ip_table[((local_var->machine_id)+1)%local_var->no_of_machines]);
	local_var->my_state=HAD_TOKEN;
	(local_var->my_timeout)->tv_usec=HAD_TOKEN_TIMEOUT;
}

void write_to_file(my_variables *local_var){

	debug_log("Entering write to file");
	int seq_recvd=recvd_by_all(local_var);
	if(local_var->machine_id>=0){

		//printf("\nsequence recvd: %d previous write seq %d",seq_recvd,local_var->prev_write_seq);
	}
	if(!(seq_recvd < 0)){
		int i;
		//printf("\nPrevious sequence is :%d",local_var->prev_write_seq);
		for(i=local_var->prev_write_seq;i<=seq_recvd;i++){
			int index=i%WINDOW;

			//printf("index is :%d",index);
			packet *my_packet=local_var->buffer[index];
			fprintf(local_var->my_file,"%2d, %8d, %8d\n",my_packet->machine_id, my_packet->payload.data.sequence_num,my_packet->payload.data.random_num);
			//	free(local_var->buffer[index]);
			//printf("\nMaking contents of %d NULL", index);
			local_var->buffer[index]=NULL;
		}
		local_var->prev_write_seq=seq_recvd+1;
	}
}

void send_packets(my_variables *local_var){


	debug_log("Entering send_packets");
	int seq_recvd=recvd_by_all(local_var);
	int i;
	int no_of_packets= WINDOW-(local_var->tok->seq-seq_recvd);
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
		int index=content.data.sequence_num%WINDOW;
		local_var->buffer[index]=data_packet;
		multicast(data_packet,local_var);
	}
	local_var->packets_sent+=count;

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

	for(i=0;local_var->tok->retransmission_list[i]!=-1;i++){
		int index= (local_var->tok->retransmission_list[i])%WINDOW;

		if(debug){
			printf("retransmiting %d.. \t",index);

		}

		if(local_var->buffer[index]!=NULL){

			local_var->buffer[index]->token_id = local_var->tok->token_id;
			multicast(local_var->buffer[index],local_var);

			if(debug){
				printf("retransmiting %d.. \t",index);

			}

		}	



	}


	if(debug){
		printf("\nlocal aru is %d",local_var->local_aru);
	}

	for(i=local_var->local_aru+1;i<=local_var->tok->seq;i++){

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

		if(debug){

			//printf("rtr list i ...%d ....seq...%d \t", i , my_rtr[i]);

		}
	}
	for(k=i;k<RTR_SIZE;k++){

		local_var->tok->retransmission_list[i]=-1;
	}

	debug_log("Exiting h rtr");
}

void update_token(my_variables *local_var){


	debug_log("Entering update_token");
	//printf("previous Token sequence: %d",local_var->prev_token_seq);

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
	//printf("%d",local_var->tok->token_id);

	if(debug){
		printf("\nCurrent Token sequence: %d",local_var->tok->seq);
		printf("\nprevious Token sequence: %d",local_var->prev_token_seq);
		printf("\nprevious Token aru: %d",local_var->prev_token_aru);
		printf("\ncurrent Token aru: %d",local_var->tok->aru);

		printf("\ncurrent local aru: %d",local_var->local_aru);


	}
	//if(local_var->tok->token_id>20){

	//	exit(0);

	//}
}

void check_eof(my_variables *local_var){


	debug_log("Entering check_eof");

	//printf("tok aru:  %d ...tok seq %d....prev tok seq %d.... ",local_var->tok->aru,local_var->tok->seq,local_var->prev_token_seq);

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

void start_exit(my_variables* local_var){

	int i;
	printf("\nToken id %d",local_var->tok->token_id);
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
	printf("\nTime taken is %lf seconds ",time_taken);
}
/*Functions for receive data*/
void process_data(my_variables *local_var, packet *mess_buf){

	debug_log("Entering process_data");
	int i;
	int wflag=0;
	int sequence=mess_buf->payload.data.sequence_num;
	if(debug){
		printf("\nSequence number is %d",sequence);
		printf("\nARU is %d",local_var->local_aru);
	}
	if(local_var->buffer[sequence%WINDOW]==NULL){
	local_var->buffer[sequence%WINDOW]=mess_buf;
	wflag==1;
	}
	else{
		//printf("\n location NOT NULL\n");
		//printf("\n Value at location is : %d",local_var->buffer[sequence%WINDOW]->payload.data.sequence_num);
		//exit(1);
	}
	/*In order packet*/
	if(local_var->local_aru+1==sequence){


		i=local_var->local_aru+1;

		int cnt=0;
		int null_flag=1;
		while(local_var->buffer[i%WINDOW]!=NULL){
			//printf("\nsequence number is LOCATION :: %d local ARU %d ",local_var->buffer[i%WINDOW]->payload.data.sequence_num, local_var->local_aru);
			if(local_var->buffer[i%WINDOW]->payload.data.sequence_num>=i)
				local_var->local_aru++;
			cnt++;
			if(cnt==WINDOW){
				null_flag=0;
				break;
			}
			if(debug){
				printf("\tlocal aru is %d",local_var->local_aru);
			}

			i++;
		}
	//	if (null_flag==1)
	//		local_var->local_aru+=cnt;
	//	else
			//local_var->local_aru=sequence;
	}

	if(debug){
		printf("\nlocal aru is %d",local_var->local_aru);
	}

	debug_log("Exiting process_data");
}

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
