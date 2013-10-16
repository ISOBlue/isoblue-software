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
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

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

#include "ring_buf.h"

enum opcode {
	SET_FILTERS = 'F',
	SEND_MESG = 'W',
};

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
    sdp_data_t *channel = 0;

    sdp_record_t *record = sdp_record_alloc();

    /* set the general service ID */
    sdp_uuid128_create(&svc_uuid, &service_uuid_int);
    sdp_set_service_id(record, svc_uuid);

    /* make the service record publicly browsable */
    sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
    root_list = sdp_list_append(0, &root_uuid);
    sdp_set_browse_groups(record, root_list);

    /* set l2cap information */
    sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
    l2cap_list = sdp_list_append(0, &l2cap_uuid);
    proto_list = sdp_list_append(0, l2cap_list);

    /* set rfcomm information */
    sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
    channel = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
    rfcomm_list = sdp_list_append(0, &rfcomm_uuid);
    sdp_list_append(rfcomm_list, channel);
    sdp_list_append(proto_list, rfcomm_list);

    /* attach protocol information to service record */
    access_proto_list = sdp_list_append(0, proto_list);
    sdp_set_access_protos(record, access_proto_list);

    /* set the name, provider, and description */
    sdp_set_info_attr(record, service_name, service_prov, service_dsc);

    /* connect to the local SDP server, register the service record, and
    disconnect */
    sdp_session_t *session = 0;
    session = sdp_connect(BDADDR_ANY, BDADDR_LOCAL, SDP_RETRY_IF_BUSY);
    if(sdp_record_register(session, record, 0) < 0) {
		perror("sdp_record_register");
		exit(EXIT_FAILURE);
	}

    /* cleanup */
    sdp_data_free(channel);
    sdp_list_free(l2cap_list, 0);
    sdp_list_free(rfcomm_list, 0);
    sdp_list_free(root_list, 0);
    sdp_list_free(access_proto_list, 0);

    return session;
}

static inline void loop_func(int n_fds, fd_set read_fds, fd_set write_fds,
		struct ring_buffer buf, int *s, int ns, int bt);
static inline int wait_func(int n_fds, fd_set *tmp_rfds, fd_set *tmp_wfds);
static inline int read_func(int sock, int iface, struct ring_buffer *buf);
static inline int send_func(int rc, struct ring_buffer *buf);
static inline int command_func(int rc, struct ring_buffer *buf, int *s);

/* argp goodies */
#define ISOBLUED_VER	"isoblued 0.3.0"
#ifdef BUILD_NUM
const char *argp_program_version = ISOBLUED_VER "\n" BUILD_NUM;
#else
const char *argp_program_version = ISOBLUED_VER;
#endif
const char *argp_program_bug_address = "<bugs@isoblue.org>";
static char doc[] = "ISOBlue Daemon -- communicates with libISOBlue";
static char args_doc[] = "BUF_FILE [IFACE]...";
static struct argp_option options[] = {
	{"channel", 'c', "<channel>", 0, "RFCOMM Channel", 0},
	{"buffer-order", 'b', "<order>", 0, "Use a 2^<order> MB buffer", 0},
	{ 0 }
};
struct arguments {
	char *file;
	char **ifaces;
	int nifaces;
	int channel;
	int buf_order;
};
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct arguments *arguments = state->input;

	switch(key) {
	case 'c':
		arguments->channel = atoi(arg);
		break;

	case 'b':
		arguments->buf_order = atoi(arg);
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
static struct argp argp = {options, parse_opt, args_doc, doc, NULL, NULL, NULL};

int main(int argc, char *argv[]) {
	fd_set read_fds, write_fds;
	int n_fds; 

	struct sockaddr_rc rc_addr = { 0 };
	socklen_t len;
	sdp_session_t *session;

	int bt;
	int ns;
	int *s;
	struct ring_buffer buf;

	/* Handle options */
	#define DEF_IFACES	((char*[]) {"ib_eng", "ib_imp"})
	struct arguments arguments = {
		"isoblue.log",
		DEF_IFACES,
		sizeof(DEF_IFACES) / sizeof(*DEF_IFACES),
		0,
		0,
	};
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	s = calloc(arguments.nifaces, sizeof(*s));
	ns = arguments.nifaces;
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	n_fds = 0;

	if((bt = socket(PF_BLUETOOTH, SOCK_STREAM | SOCK_NONBLOCK, BTPROTO_RFCOMM))
			< 0) {
		perror("socket (bt)");
		return EXIT_FAILURE;
	}
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
	FD_SET(bt, &read_fds);
	n_fds = bt > n_fds ? bt : n_fds;

	session = register_service(rc_addr.rc_channel);

	ring_buffer_create(&buf, 20 + arguments.buf_order, arguments.file);

	/* Initialize ISOBUS sockets */
	int i;
	for(i = 0; i < arguments.nifaces; i++) {
		struct sockaddr_can addr = { 0 };
		struct ifreq ifr;

		if((s[i] = socket(PF_CAN, SOCK_DGRAM | SOCK_NONBLOCK, CAN_ISOBUS))
				< 0) {
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
		n_fds = s[i] > n_fds ? s[i] : n_fds;
	}

	/* Do socket stuff */
	loop_func(n_fds, read_fds, write_fds, buf, s, ns, bt);

	sdp_close(session);

	return EXIT_SUCCESS;
}

static inline void check_send(struct ring_buffer *buf, int rc,
		fd_set *write_fds)
{
	if(ring_buffer_unread_bytes(buf)) {
		FD_SET(rc, write_fds);
	} else {
		FD_CLR(rc, write_fds);
	}
}

static inline void loop_func(int n_fds, fd_set read_fds, fd_set write_fds,
		struct ring_buffer buf, int *s, int ns, int bt)
{
	int rc = -1;

	while(1) {
		fd_set tmp_rfds = read_fds, tmp_wfds = write_fds;
		if(wait_func(n_fds, &tmp_rfds, &tmp_wfds) == 0) {
			continue;
		}

		/* Read ISOBUS */
		int i;
		for(i = 0; i < ns; i++) {
			if(!FD_ISSET(s[i], &tmp_rfds)) {
				continue;
			}

			read_func(s[i], i, &buf);
		}

		/* Check RFCOMM connection */
		if(rc > 0) {
			/* Read RFCOMM */
			if(FD_ISSET(rc, &tmp_rfds)) {
				if(command_func(rc, &buf, s) < 0) {
					FD_CLR(rc, &read_fds);
					FD_CLR(rc, &write_fds);
					FD_SET(bt, &read_fds);
					close(rc);
					rc = -1;
					continue;
				}
			}

			/* Write RFCOMM */
			if(FD_ISSET(rc, &tmp_wfds)) {
				if(send_func(rc, &buf) < 0) {
					FD_CLR(rc, &read_fds);
					FD_CLR(rc, &write_fds);
					FD_SET(bt, &read_fds);
					close(rc);
					rc = -1;
					continue;
				}
			}

			/* Check send buffer */
			check_send(&buf, rc, &write_fds);
		} else {
			/* Accept Bluetooth connection */
			if(FD_ISSET(bt, &tmp_rfds)) {
				if((rc = accept(bt, NULL, NULL)) < 0) {
					perror("accept");
				} else {
					FD_SET(rc, &read_fds);
					FD_SET(rc, &write_fds);
					n_fds = rc > n_fds ? rc : n_fds;
					FD_CLR(bt, &read_fds);
				}
			}
		}
	}
}

static inline int wait_func(int n_fds, fd_set *tmp_rfds, fd_set *tmp_wfds)
{
	int ret;

	if((ret = select(n_fds + 1, tmp_rfds, tmp_wfds, NULL, NULL)) < 0) {
		perror("select");

		switch(errno) {
		case EINTR:
			return 0;

		default:
			exit(EXIT_FAILURE);
		}
	}

	return ret;
}

static inline int read_func(int sock, int iface, struct ring_buffer *buf)
{
	struct isobus_mesg mes = { 0 };
	struct sockaddr_can addr = { 0 };
	struct timeval ts;

	/* Buffer received CAN frames */
	struct msghdr msg = { 0 };
	struct iovec iov = { 0 };
	char ctrlmsg[CMSG_SPACE(sizeof(struct sockaddr_can))];
	/* Construct msghdr to use to recevie messages from socket */
	msg.msg_name = &addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = &iov;
	msg.msg_control = ctrlmsg;
	msg.msg_controllen = sizeof(ctrlmsg);
	msg.msg_iovlen = 1;
	iov.iov_base = &mes;
	iov.iov_len = sizeof(mes);

	if(recvmsg(sock, &msg, 0) <= 0) {
		perror("recvmsg");
		exit(0);
	}

	/* Get saddr */
	struct sockaddr_can daddr = { 0 };
	struct cmsghdr *cmsg;
	for(cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
			cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if(cmsg->cmsg_level == SOL_CAN_ISOBUS &&
				cmsg->cmsg_type == CAN_ISOBUS_DADDR) {
			memcpy(&daddr, CMSG_DATA(cmsg), sizeof(daddr));
			break;
		}
	}

	/* Find approximate receive time */
	gettimeofday(&ts, NULL);

	/* Print messages */
	int chars;
	chars = 0;
	chars += sprintf(ring_buffer_tail_address(buf),
			"\n%d %06d %d ",
			iface,
			mes.pgn,
			mes.dlen);

	int j;
	for(j = 0; j < mes.dlen; j++)
	{
		chars += sprintf(ring_buffer_tail_address(buf) + chars,
				"%02x ", mes.data[j]);
	}

	chars += sprintf(ring_buffer_tail_address(buf) + chars,
			"%ld.%06ld %02x %02x",
			ts.tv_sec, ts.tv_usec,
			addr.can_addr.isobus.addr,
			daddr.can_addr.isobus.addr);

	ring_buffer_tail_advance(buf, chars);
	*(char *)ring_buffer_tail_address(buf) = '\n';

	//printf("Message received\n");

	return 1;
}

static inline int send_func(int rc, struct ring_buffer *buf)
{
	int chars;
	char *mesg;

	if(sscanf(ring_buffer_curs_address(buf),
			"\n%m[^\n]\n", &mesg) != 1) {
		perror("sscanf");
		return 0;
	}
	chars = strlen(mesg);

	mesg[chars++] = '\n';
	if(send(rc, mesg, chars, 0) < 0) {
		perror("send");

		switch(errno) {
		case EAGAIN:
			return 0;

		default:
			return -1;
		}
	}

	//mesg[chars-1] = '\0';
	//printf("%s\n", mesg);

	ring_buffer_curs_advance(buf, chars);
	free(mesg);

	return 1;
}

static inline int command_func(int rc, struct ring_buffer *buf, int *s)
{
	/* Buffer for reassembling commands */
	#define CMD_BUF_SIZE	0x01FFFF
	static char buffer[CMD_BUF_SIZE] = { 0 };
	static int curs = 0, tail = 0;

	int chars;
	chars = recv(rc, buffer+tail, CMD_BUF_SIZE-tail, 0);
	printf("%*s\n", chars, buffer+tail);
	if(chars < 0) {
		perror("read");
		return -1;
	}
	tail += chars;

	bool done = false;
	while(curs < tail) {
		if(buffer[curs] == '\n' || buffer[curs] == '\r') {
			done = true;
			buffer[curs++] = '\0';
			break;
		}

		curs++;
	}
	if(!done) {
		return 0;
	}

	char op;
	char *args;
	op = buffer[0];
	args = buffer+2;
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

		ring_buffer_clear(buf);
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
	
	memmove(buffer, buffer+curs, tail-curs);
	tail -= curs;
	curs = 0;

	return 0;
}

