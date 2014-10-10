#include "net_include.h"

int main()
{
	struct sockaddr_in name;
	struct sockaddr_in send_addr;

	int                mcast_addr;

	struct ip_mreq     mreq;
	unsigned char      ttl_val;

	int                ss;
	fd_set             mask;
	fd_set             dummy_mask,temp_mask;
	int                bytes;
	int                num;
	char               mess_buf[MAX_MESS_LEN];
	char               input_buf[80];

	mcast_addr = MCAST_ADDRESS /* (225.1.2.115*/;

	mreq.imr_multiaddr.s_addr = htonl( mcast_addr );

	/* the interface could be changed to a specific interface if needed */
	mreq.imr_interface.s_addr = htonl( INADDR_ANY );

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
	strcpy(input_buf,"start");
	sendto( ss, input_buf, strlen(input_buf), 0, 
			(struct sockaddr *)&send_addr, sizeof(send_addr) );


	return 0;

}

