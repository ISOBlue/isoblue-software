#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
 
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
 
#include "socketcan-isobus/patched/can.h"
#include "socketcan-isobus/pdu.h"
 
int main(void) {
	int s;
	int nbytes;
	struct sockaddr_can addr;
	struct ifreq ifr;
	int i;
	struct pdu p;
	struct msghdr msg;
	struct iovec iov;

	char ctrlmsg[CMSG_SPACE(sizeof(struct timeval))+CMSG_SPACE(sizeof(__u32))];
	char *ifname = "vcan0";

	if((s = socket(PF_CAN, SOCK_RAW, CAN_PDU)) < 0) {
		perror("Error while opening socket");
		return -1;
	}

	strcpy(ifr.ifr_name, ifname);
	ioctl(s, SIOCGIFINDEX, &ifr);

	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex; 

	if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Error in socket bind");
		return -2;
	}

	msg.msg_iov = &iov;
	msg.msg_control = &ctrlmsg;
	msg.msg_controllen = sizeof(ctrlmsg);
	msg.msg_iovlen = 1;
	iov.iov_base = &p;
	iov.iov_len = sizeof(p);

	while(1)
	{
		nbytes = recvmsg(s, &msg, 0);
		if(nbytes == -1)
		{
			perror("recvmesg");
			return -1;
		}

		printf("pri:%1x edp:%1x dp:%1x pf:%02x ps:%02x sa:%02x len:%1x data:",
			p.priority,
			p.extended_data_page,
			p.data_page,
			p.format,
			p.specific,
			p.source_address,
			p.data_len);
		for(i = 0; i < p.data_len; i++)
			printf("%02x", p.data[i]);
		printf("\n");
		fflush(0);
	}

	return 0;
}

