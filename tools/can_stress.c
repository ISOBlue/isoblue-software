/*
 * CAN stress test tool
 *
 * Attempts to run the CAN bus(es) at full rate,
 * sending a sequence of frames incrementing data bytes .
 *
 *
 * Author: Alex Layton <alex@layton.in>
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

#define CANSTRESS_VER	"canstress - CAN stress tester"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <argp.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <linux/can.h>
#include <linux/can/raw.h>

/* argp goodies */
#ifdef BUILD_NUM
const char *argp_program_version = CANSTRESS_VER "\n" BUILD_NUM;
#else
const char *argp_program_version = CANSTRESS_VER;
#endif
const char *argp_program_bug_address = "<bugs@isoblue.org>";
static char args_doc[] = "IFACE(S)...";
static char doc[] = "Continually send on IFACE(s) to stress test CAN bus(es).";
static struct argp_option options[] = {
	{NULL, 0, NULL, 0, "About", -1},
	{NULL, 0, NULL, 0, "Configuration", 0},
	{"count", 'c', "<count>", 0, "Stop after <count> frames", 0},
	{"length", 'l', "<bytes>", 0, "Send <bytes> bytes of data per frame", 0},
	{"delay", 'd', "<usecs>", 0, "Put a <usecs> usec delay between frames", 0},
	{ 0 }
};
struct arguments {
	char **ifaces;
	int nifaces;
	int count;
	int length;
	unsigned int delay;
};
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;
	error_t ret = 0;

	switch(key) {
	case 'c':
		arguments->count = atoi(arg);
		break;

	case 'l':
		arguments->length = atoi(arg);
		break;

	case 'd':
		arguments->delay = atoi(arg);
		break;

	case ARGP_KEY_ARGS:
		arguments->ifaces = state->argv + state->next;
		arguments->nifaces = state->argc - state->next;
		break;

	case ARGP_KEY_END:
		if(arguments->nifaces > 1 && arguments->delay > 0) {
			fprintf(stderr,
					"Using a delay with multiple ifaces not yet supported.\n");
			ret = EINVAL;
		}
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return errno = ret;
}
static char *help_filter(int key, const char *text, void *input)
{
	char *buffer = input;

	switch(key) {
	case ARGP_KEY_HELP_HEADER:
		buffer = malloc(strlen(text)+1);
		strcpy(buffer, text);
		return strcat(buffer, ":");

	default:
		return (char *)text;
	}
}
static struct argp argp = {
	options,
	parse_opt,
	args_doc,
	doc,
	NULL,
	help_filter,
	NULL
};

int main(int argc, char *argv[])
{
	int *socks;
	__u64 *nums;
	fd_set fds;
	int nfds;

	int i;

	/* Handle options */
	struct arguments arguments = {
		NULL,
		0,
		0,
		8,
		0,
	};
	if(argp_parse(&argp, argc, argv, 0, 0, &arguments)) {
		perror(NULL);
		return EXIT_FAILURE;
	}
	
	socks = calloc(arguments.nifaces, sizeof(*socks));
	nums = calloc(arguments.nifaces, sizeof(*nums));

	FD_ZERO(&fds);
	nfds = 0;
	for(i = 0; i < arguments.nifaces; i++) {
		struct sockaddr_can addr = { 0 };
		struct ifreq ifr;

		if((socks[i] = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW))
				< 0) {
			perror("socket");
			return EXIT_FAILURE;
		}

		strcpy(ifr.ifr_name, arguments.ifaces[i]);
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

	struct can_frame cf = {CAN_EFF_FLAG, arguments.length, { 0 }};
	do {
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

		for(i = 0; i < arguments.nifaces; i++) {
			if(!FD_ISSET(socks[i], &tmp_fds)) {
				continue;
			}

			memcpy(cf.data, &nums[i], arguments.length);

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

			usleep(arguments.delay);

			nums[i]++;
		}
	} while(!arguments.count || --arguments.count);

	return EXIT_SUCCESS;
}

