#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
 
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
 
#include "../socketcan-isobus/patched/can.h"
#include "../socketcan-isobus/isobus.h"
 
int main(int argc, char *argv[]) {
	int s;
	int nbytes;
	struct sockaddr_can addr;
	struct ifreq ifr;
	int i;
	struct msghdr msg;
	struct iovec iov;

	char ctrlmsg[CMSG_SPACE(sizeof(struct timeval))+CMSG_SPACE(sizeof(__u32))];
	struct isobus_mesg mesg;

	if((s = socket(PF_CAN, SOCK_DGRAM, CAN_ISOBUS)) < 0) {
		perror("Error while opening socket");
		return -1;
	}

	/* Set interface name to first argument */
	strcpy(ifr.ifr_name, argv[1]);
	ioctl(s, SIOCGIFINDEX, &ifr);

	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex; 
	sscanf(argv[2], "%2x", &addr.can_addr.isobus.addr);

	if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Error in socket bind");
		return -2;
	}

	/* Construct msghdr to use to recevie messages from socket */
	msg.msg_iov = &iov;
	msg.msg_control = &ctrlmsg;
	msg.msg_controllen = sizeof(ctrlmsg);
	msg.msg_iovlen = 1;
	iov.iov_base = &mesg;
	iov.iov_len = sizeof(mesg);

	mesg.pgn = ISOBUS_PGN_REQUEST;
	mesg.dlen = 1;
	mesg.data[0] = 0xAA;

	/* Send an ISOBUS message */
	if(sendto(s, &mesg, sizeof(mesg), 0, (struct sockaddr *)&addr,
				sizeof(addr)) < 0) {
		perror("Error sending");
		return -3;
	}

	/* Listen for ISOBUS messages */
	while(1)
	{
		nbytes = recvmsg(s, &msg, 0);
		if(nbytes == -1)
		{
			perror("recvmesg");
			return -1;
		}

		printf("PGN:%6d data:", mesg.pgn);
		for(i = 0; i < mesg.dlen; i++)
			printf("%02x", mesg.data[i]);
		printf("\n");
		fflush(0);
	}

	return 0;
}

