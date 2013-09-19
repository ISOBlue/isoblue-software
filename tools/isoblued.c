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
#include <argp.h>
 
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <bluetooth/rfcomm.h>
 
#include "../socketcan-isobus/patched/can.h"
#include "../socketcan-isobus/isobus.h"

#include <pthread.h>

#include "ring_buf.h"

enum opcode {
	SET_FILTERS = 'F',
	SEND_MESG = 'W',
};
 
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

sdp_session_t *register_service(uint8_t rfcomm_channel)
{
    uint128_t service_uuid_int = { {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0xAB, 0xCD} };
    const char *service_name = "ISOBlue";
    const char *service_dsc = "Use ISOBUS over Bluetooth";
    const char *service_prov = "ISOBlue";

    uuid_t root_uuid, l2cap_uuid, rfcomm_uuid, svc_uuid;
    sdp_list_t *l2cap_list = 0, 
               *rfcomm_list = 0,
               *root_list = 0,
               *proto_list = 0, 
               *access_proto_list = 0;
    sdp_data_t *channel = 0, *psm = 0;

    sdp_record_t *record = sdp_record_alloc();

    // set the general service ID
    sdp_uuid128_create( &svc_uuid, &service_uuid_int );
    sdp_set_service_id( record, svc_uuid );

    // make the service record publicly browsable
    sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
    root_list = sdp_list_append(0, &root_uuid);
    sdp_set_browse_groups( record, root_list );

    // set l2cap information
    sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
    l2cap_list = sdp_list_append( 0, &l2cap_uuid );
    proto_list = sdp_list_append( 0, l2cap_list );

    // set rfcomm information
    sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
    channel = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
    rfcomm_list = sdp_list_append( 0, &rfcomm_uuid );
    sdp_list_append( rfcomm_list, channel );
    sdp_list_append( proto_list, rfcomm_list );

    // attach protocol information to service record
    access_proto_list = sdp_list_append( 0, proto_list );
    sdp_set_access_protos( record, access_proto_list );

    // set the name, provider, and description
    sdp_set_info_attr(record, service_name, service_prov, service_dsc);

	int err = 0;
    sdp_session_t *session = 0;

    // connect to the local SDP server, register the service record, and 
    // disconnect
    session = sdp_connect( BDADDR_ANY, BDADDR_LOCAL, SDP_RETRY_IF_BUSY );
    err = sdp_record_register(session, record, 0);

    // cleanup
    sdp_data_free( channel );
    sdp_list_free( l2cap_list, 0 );
    sdp_list_free( rfcomm_list, 0 );
    sdp_list_free( root_list, 0 );
    sdp_list_free( access_proto_list, 0 );

    return session;
}

void *bt_func(void *);
void *send_func(void *);
void *command_func(void *);

int bt, rc;
int ns;
int *s;
struct ring_buffer buf;

/* argp goodies */
const char *argp_program_version = "isoblued 0.2";
const char *argp_program_bug_address = "<bugs@isoblue.org>";
static char doc[] = "ISOBlue Daemon -- communicates with libISOBlue";
static char args_doc[] = "BUF_FILE [IFACE]...";
static struct argp_option options[] = {
	{"channel", 'c', "CHANNEL", 0, "RFCOMM Channel", 0},
	{ 0 }
};
struct arguments {
	char *file;
	char **ifaces;
	int nifaces;
	int channel;
};
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct arguments *arguments = state->input;

	switch(key) {
	case 'c':
		arguments->channel = atoi(arg);
		break;

	case ARGP_KEY_ARG:
		if(state->arg_num == 0)
			arguments->file = arg;
		else
			return ARGP_ERR_UNKNOWN;
		break;

	case ARGP_KEY_ARGS:
		arguments->ifaces = state->argv + state->next;
		arguments->nifaces = state->argc - state->next;
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}
static struct argp argp = {options, parse_opt, args_doc, doc};

int main(int argc, char *argv[]) {
	struct sockaddr_can addr = { 0 };
	socklen_t addr_len;
	struct isobus_mesg mes = { 0 };
	struct ifreq ifr;
	struct timeval ts;
	int i;
	fd_set read_fds;
	int n_read_fds; 

	pthread_t bt_thread;
	char byte;

	struct sockaddr_rc rc_addr = { 0 };
	socklen_t len;
	sdp_session_t *session;

	/* Handle options */
	#define DEF_IFACES	((char*[]) {"ib_eng", "ib_imp"})
	struct arguments arguments = {
		"isoblue.log",
		DEF_IFACES,
		sizeof(DEF_IFACES) / sizeof(*DEF_IFACES),
		0,
	};
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	s = calloc(arguments.nifaces, sizeof(*s));
	ns = arguments.nifaces;
	FD_ZERO(&read_fds);
	n_read_fds = 0;

	if((bt = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM)) < 0) {
		perror("socket (bt)");
		return EXIT_FAILURE;
	}
	n_read_fds = bt > n_read_fds ? bt : n_read_fds;
	rc_addr.rc_family = AF_BLUETOOTH;
	rc_addr.rc_bdaddr = *BDADDR_ANY;
	rc_addr.rc_channel = arguments.channel;

	if(bind(bt, (struct sockaddr *)&rc_addr, sizeof(rc_addr)) < 0) {
		perror("bind bt");
		return EXIT_FAILURE;
	}
	listen(bt, 1);
	len = sizeof(rc_addr);
	if(getsockname(bt, (struct sockaddr *)&rc_addr, &len) < 0) {
		perror("getsockname");
		return EXIT_FAILURE;
	}

	session = register_service(rc_addr.rc_channel);

	ring_buffer_create(&buf, 15, arguments.file);
	if(pthread_create(&bt_thread, NULL, bt_func, NULL) != 0) {
		perror("bt_thread");
		exit(EXIT_FAILURE);
	}

	/* Initialize ISOBUS sockets */
	for(i = 0; i < arguments.nifaces; i++) {
		if((s[i] = socket(PF_CAN, SOCK_DGRAM, CAN_ISOBUS)) < 0) {
			perror("socket (can)");
			return EXIT_FAILURE;
		}

		/* Set interface name to argument value */
		strcpy(ifr.ifr_name, arguments.ifaces[i]);
		ioctl(s[i], SIOCGIFINDEX, &ifr);
		addr.can_family  = AF_CAN;
		addr.can_ifindex = ifr.ifr_ifindex; 
		addr.can_addr.isobus.addr = CAN_ISOBUS_ANY_ADDR;

		if(bind(s[i], (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			perror("bind can");
			return EXIT_FAILURE;
		}

		FD_SET(s[i], &read_fds);
		n_read_fds = s[i] > n_read_fds ? s[i] : n_read_fds;
	}

	while(1) {
		if(select(n_read_fds + 1, &read_fds, NULL, NULL, NULL) < 0) {
		   perror("select");
		   return EXIT_FAILURE;
		}

		for(i = 0; i < ns; i++) {
			if(!FD_ISSET(s[i], &read_fds)) {
				continue;
			}

			/* Buffer received CAN frames */
			struct msghdr msg;
			struct iovec iov;
			char ctrlmsg[CMSG_SPACE(sizeof(struct sockaddr_can))];
			/* Construct msghdr to use to recevie messages from socket */
			msg.msg_name = &addr;
			msg.msg_namelen = sizeof(addr);
			msg.msg_iov = &iov;
			msg.msg_control = &ctrlmsg;
			msg.msg_controllen = sizeof(ctrlmsg);
			msg.msg_iovlen = 1;
			iov.iov_base = &mes;
			iov.iov_len = sizeof(mes);

			addr_len = sizeof(addr);
			if(recvmsg(s[i], &msg, 0) != -1) {
				/* Get saddr */
				struct sockaddr_can saddr;
				struct cmsghdr *cmsg;
				for(cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
						cmsg = CMSG_NXTHDR(&msg, cmsg)) {
					if(cmsg->cmsg_level == SOL_CAN_ISOBUS &&
							cmsg->cmsg_type == CAN_ISOBUS_SADDR) {
						saddr = *(struct sockaddr_can *)CMSG_DATA(cmsg);
					}
				}

				/* Find approximate receive time */
				gettimeofday(&ts, NULL);

				/* Print messages */
				int chars;
				chars = 0;
				chars += sprintf(ring_buffer_tail_address(&buf),
						"\n%d %06d %d ",
						i,
						mes.pgn,
						mes.dlen);

				int j;
				for(j = 0; j < mes.dlen; j++)
				{
					chars += sprintf(ring_buffer_tail_address(&buf) + chars,
							"%02x ", mes.data[j]);
				}

				chars += sprintf(ring_buffer_tail_address(&buf) + chars,
						"%ld.%06ld %2x %2x",
						ts.tv_sec, ts.tv_usec,
						addr.can_addr.isobus.addr,
						saddr.can_addr.isobus.addr);

				ring_buffer_tail_advance(&buf, chars);
				*(char *)ring_buffer_tail_address(&buf) = '\n';

				printf("Message received\n");
			} else {
				perror("recvmsg");
			}
		}
	}

	return EXIT_SUCCESS;
}

pthread_t send_thread, command_thread;
void *bt_func(void *ptr)
{
	while(1) {
		struct sockaddr_rc addr = { 0 };
		socklen_t len;

		len = sizeof(addr);
		rc = accept(bt, (struct sockaddr *)&addr, &len);

		if(pthread_create(&send_thread, NULL, send_func, NULL) != 0) {
			perror("send_thread");
			exit(EXIT_FAILURE);
		}
		if(pthread_create(&command_thread, NULL, command_func, NULL) != 0) {
			perror("command_thread");
			exit(EXIT_FAILURE);
		}

		pthread_join(command_thread, NULL);
		pthread_join(send_thread, NULL);

		close(rc);
	}

	return NULL;
}

void *send_func(void *ptr)
{
	while(1) {
		int chars;
		char *mesg;

		pthread_setcancelstate(PTHREAD_CANCEL_DEFERRED, NULL);
		ring_buffer_wait_unread_bytes(&buf);
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		sscanf(ring_buffer_curs_address(&buf),
				"\n%m[^\n]%n\n", &mesg, &chars);

		mesg[chars] = '\n';
		chars++;

		if(send(rc, mesg, chars, 0) < 0) {
			perror("send");
			pthread_cancel(command_thread);
			break;
		}
		printf("%s\n", mesg);

		ring_buffer_curs_advance(&buf, chars);
		free(mesg);
	}

	return NULL;
}
void *command_func(void *ptr)
{
	FILE *fp;

	fp = fdopen(rc, "r");
	setvbuf(fp, NULL, _IONBF, 0);
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	while(1) {
		char op;
		char *args;

		pthread_testcancel();
		if(fscanf(fp, "%c %m[^\n\r]%*1[\n\r]", &op, &args) != 2) {
			perror("fscanf");
			pthread_cancel(send_thread);
			break;
		}
		printf("op %c, %s\n", op, args);

		switch(op) {
		case SET_FILTERS:
		{
			int sock;
			int nchars;
			char *p;
			struct isobus_filter *filts;
			int nfilts;

			sscanf(args, "%d %d %n", &sock, &nfilts, &nchars);
			p = args + nchars;

			if(nfilts == 0) {
				/* Receive everything when 0 filters given */
				nfilts = 1;
				filts = malloc(sizeof(*filts));

				filts[0].pgn = 0;
				filts[0].pgn_mask = 0;
				filts[0].daddr = 0;
				filts[0].daddr_mask = 0;
				filts[0].saddr = 0;
				filts[0].saddr_mask = 0;
			} else {
				filts = calloc(nfilts, sizeof(*filts));
				memset(filts, 0, nfilts * sizeof(*filts));

				int i;
				for(i = 0; i < nfilts; i++) {
					int pgn;

					sscanf(p, "%d%n", &pgn, &nchars);
					p += nchars;

					filts[i].pgn = pgn;
					filts[i].pgn_mask = CAN_ISOBUS_PGN_MASK;
				}
			}
			if(setsockopt(s[sock], SOL_CAN_ISOBUS, CAN_ISOBUS_FILTER, filts,
						nfilts * sizeof(*filts)) < 0) {
				perror("setsockopt");
			}

			ring_buffer_clear(&buf);
			free(filts);
			break;
		}

		case SEND_MESG:
		{
			int nchars;
			char *p;
			int pgn;
			int len;
			int dest;
			int sock;

			sscanf(args, "%d %d %d %d %n", &sock, &dest, &pgn, &len, &nchars);
			p = args + nchars;

			int i;
			unsigned char *data;
			data = calloc(len, sizeof(*data));
			for(i = 0; i < len; i++) {
				sscanf(p, "%2hhx%n", &data[i], &nchars);
				p += nchars;
			}

			struct isobus_mesg mesg = { 0 };
			mesg.pgn = pgn;
			mesg.dlen = len;
			for(i = 0; i < len; i++)
				mesg.data[i] = data[i];

			struct sockaddr_can addr = { 0 };
			addr.can_family = AF_CAN;
			addr.can_addr.isobus.addr = dest;

			sendto(s[sock], &mesg, sizeof(mesg), 0, (struct sockaddr *)&addr,
					sizeof(addr));
			
			break;
		}
		}

		free(args);
	}

	fclose(fp);

	return NULL;
}

