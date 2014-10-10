#include "net_include.h"
#include "recv_dbg.h"


void multicast(packet*,int , struct sockaddr_in *);
int get_local_ipaddress();
int main(int argc, char* argv[])
{
	/*Pre-Initialization starts here*/
	struct sockaddr_in name;
	struct sockaddr_in send_addr;

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

	send_addr.sin_family = AF_INET;
	send_addr.sin_addr.s_addr = htonl(mcast_addr);  /* mcast address */
	send_addr.sin_port = htons(PORT);

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
	long my_ip = get_local_ipaddress();
	printf("\nMy IP is: %d",my_ip);
	recv_dbg_init(atoi(argv[4]),atoi(argv[2]));
	for(;;)
	{
		temp_mask = mask;
		num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);
		if (num > 0) {
			if ( FD_ISSET( sr, &temp_mask) ) {
				//bytes = recv( sr, mess_buf, sizeof(mess_buf), 0 );

				mess_buf=NULL;
				mess_buf=(packet*)malloc(sizeof(packet));
				bytes=recv_dbg(sr,(char*)mess_buf,sizeof(packet),0);
				printf( "received from: %d\n", mess_buf->machine_id );
			}else if( FD_ISSET(0, &temp_mask) ) {
				bytes = read( 0, input_buf, sizeof(input_buf) );
				input_buf[bytes] = 0;
				printf( "there is an input: %s\n", input_buf );
				packet* init_packet = (packet*)malloc(sizeof(packet));
				init_packet->payload.ip_address=20;
				init_packet->machine_id=atoi(argv[2]);
				init_packet->token_id=2;
				init_packet->type=INIT_MSG;
				multicast(init_packet, ss, &send_addr);
			}
		}
	}

	return 0;

}

void multicast(packet* send_buff,int ss, struct sockaddr_in *send_addr){


	sendto(ss, (char*)send_buff, sizeof(packet), 0,
			(struct sockaddr *)send_addr, sizeof(*send_addr));


}


