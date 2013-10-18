/*
 * CAN stress test tool
 *
 * Attempts to run the CAN bus(es) at full rate,
 * sending a sequence of frames incrementing data bytes .
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <linux/can.h>
#include <linux/can/raw.h>

int main(int argc, char *argv[])
{
	int *socks;
	__u64 *nums;
	fd_set fds;
	int nfds;

	int i;
	
	socks = calloc(argc-1, sizeof(*socks));
	nums = calloc(argc-1, sizeof(*nums));

	FD_ZERO(&fds);
	nfds = 0;
	for(i = 0; i < argc-1; i++) {
		struct sockaddr_can addr = { 0 };
		struct ifreq ifr;

		if((socks[i] = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW))
				< 0) {
			perror("socket");
			return EXIT_FAILURE;
		}

		strcpy(ifr.ifr_name, argv[i+1]);
		ioctl(socks[i], SIOCGIFINDEX, &ifr);
		addr.can_family  = AF_CAN;
		addr.can_ifindex = ifr.ifr_ifindex; 
		if(bind(socks[i], (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			perror("bind");
			return EXIT_FAILURE;
		}

		FD_SET(socks[i], &fds);
		nfds = socks[i] > nfds ? socks[i] : nfds;
	}

	struct can_frame cf = {CAN_EFF_FLAG, 8, {0, 0, 0, 0, 0, 0, 0, 0}};
	while(1) {
		fd_set tmp_fds = fds;
		if(select(nfds+1, NULL, &tmp_fds, NULL, NULL) < 0) {

			switch(errno) {
			case EINTR:
				continue;

			default:
				perror("select");
				return EXIT_FAILURE;
			}
		}

		for(i = 0; i < argc-1; i++) {
			if(!FD_ISSET(socks[i], &tmp_fds)) {
				continue;
			}

			memcpy(cf.data, &nums[i], sizeof(*nums));

			if(send(socks[i], &cf, sizeof(cf), 0) < 0) {

				switch(errno) {
				case EAGAIN:
				case ENOBUFS:
					continue;

				default:
					perror("send");
					return EXIT_FAILURE;
				}
			}

			nums[i]++;
		}
	}
}

