/*
 * ISOBlue "server" application
 *
 * Enables getting at ISOBUS messages over Bluetooth.
 *
 *
 * Author: Alex Layton <awlayton@purdue.edu>
 *
 * Copyright (C) 2013 Purdue University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
 
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
 
#include "../socketcan-isobus/patched/can.h"
#include "../socketcan-isobus/isobus.h"
 
void print_message(char interface[], struct timeval *ts, 
		struct isobus_mesg *mes) {
	int i;

	/* Output CAN message */
	printf("%06d ", mes->pgn);
	for(i = 0; i < mes->dlen; i++) {
		printf(" %02x", mes->data[i]);
	}
	/* Output timestamp and CAN interface */
	printf("\t%ld.%06ld\t%s\n", ts->tv_sec, ts->tv_usec, interface);

	/* Flush output after each message */
	fflush(stdout);
}

void set_filters(int *s, int ns) {
	long count;
	struct isobus_filter *filter;
	int i;

	/* Get number of PGNs */
	if(scanf("%ld", &count) != 1) {
		printf("filter return\n");
		return;
	}

	filter = malloc(count * sizeof(*filter));
	memset(filter, 0, count * sizeof(*filter));
	for(i = 0; i < count; i++) { 
		if(scanf("%d", &filter[i].pgn) != 1) {
			printf("filter return\n");
			return;
		}
		filter[i].pgn_mask = CAN_ISOBUS_PGN_MASK;

		printf("filter PGN:%d\n", filter[i].pgn);
	}

	/* Apply filters */
	for(i = 0; i < ns; i++) {
		setsockopt(s[i], SOL_CAN_ISOBUS, CAN_ISOBUS_FILTER, filter,
				count * sizeof(*filter));
	}
}

int main(int argc, char *argv[]) {
	int *s;
	struct sockaddr_can addr;
	socklen_t addr_len;
	struct isobus_mesg mes;
	struct ifreq ifr;
	struct timeval ts;
	int i;
	fd_set read_fds;

	s = malloc((argc - 1) * sizeof(*s));
	FD_ZERO(&read_fds);

	/* FD_SET(0, &read_fds); */

	/* Initialize sockets */
	for(i = 0; i < argc - 1; i++) {
		if((s[i] = socket(PF_CAN, SOCK_DGRAM, CAN_ISOBUS)) < 0) {
			perror("socket");
			return EXIT_FAILURE;
		}

		/* Set interface name to argument value */
		strcpy(ifr.ifr_name, argv[i + 1]);
		ioctl(s[i], SIOCGIFINDEX, &ifr);
		addr.can_family  = AF_CAN;
		addr.can_ifindex = ifr.ifr_ifindex; 
		addr.can_addr.isobus.addr = CAN_ISOBUS_ANY_ADDR;

		if(bind(s[i], (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			perror("bind");
			return EXIT_FAILURE;
		}

		FD_SET(s[i], &read_fds);
	}

	while(1) {
		if(select(FD_SETSIZE, &read_fds, NULL, NULL, NULL) < 0) {
		   perror("select");
		   return EXIT_FAILURE;
		}

		for(i = 0; i < FD_SETSIZE; i++) {
			if(!FD_ISSET(i, &read_fds)) {
				continue;
			}

			if(i == 0) {
				/* Process commands */
				/* TODO: Actually support commands... */
			} else {
				/* Print received CAN frames */
				addr_len = sizeof(addr);
				if(recvfrom(i, &mes, sizeof(mes), 0,
							(struct sockaddr *)&addr, &addr_len) != -1) {

					/* Find approximate receive time */
					gettimeofday(&ts, NULL);

					/* Find name of receive interface */
					ifr.ifr_ifindex = addr.can_ifindex;
					ioctl(i, SIOCGIFNAME, &ifr);

					/* Print messages */
					print_message(ifr.ifr_name, &ts, &mes);
				} else {
					perror("recvfrom");
				}
			}
		}
	}

	return EXIT_SUCCESS;
}

