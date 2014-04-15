/*
 * ISOBlue "server" application
 *
 * Enables getting at ISOBUS messages over Bluetooth.
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

#define ISOBLUED_VER	"isoblued - ISOBlue daemon 0.3.2"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

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

#include <leveldb/c.h>

#include "../socketcan-isobus/patched/can.h"
#include "../socketcan-isobus/isobus.h"

#include "ring_buf.h"

enum opcode {
	SET_FILTERS = 'F',
	SEND_MESG = 'W',
	MESG = 'M',
	ACK = 'A',
	GET_PAST = 'P',
	OLD_MESG = 'O',
};

/* Registers isoblued with the SDP server */
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

/* argp goodies */
#ifdef BUILD_NUM
const char *argp_program_version = ISOBLUED_VER "\n" BUILD_NUM;
#else
const char *argp_program_version = ISOBLUED_VER;
#endif
const char *argp_program_bug_address = "<bugs@isoblue.org>";
static char args_doc[] = "BUF_FILE [IFACE...]";
static char doc[] = "Connect ISOBlue to IFACE(s), using BUF_FILE as a buffer.";
static struct argp_option options[] = {
	{NULL, 0, NULL, 0, "About", -1},
	{NULL, 0, NULL, 0, "Configuration", 0},
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
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
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
#define POST_DOC_TEXT	"With no IFACE, connect to ib_eng and ib_imp."
#define EXTRA_TEXT	"ISOBlue home page <http://www.isoblue.org>"
static char *help_filter(int key, const char *text, void *input)
{
	char *buffer = input;

	switch(key) {
	case ARGP_KEY_HELP_POST_DOC:
		return strdup(POST_DOC_TEXT);

	case ARGP_KEY_HELP_EXTRA:
		return strdup(EXTRA_TEXT);

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

/* Leveldb stuff */
leveldb_t *db;
leveldb_options_t *db_options;
leveldb_readoptions_t *db_roptions;
leveldb_writeoptions_t *db_woptions;
char *db_err = NULL;
typedef uint32_t db_key_t;
db_key_t db_id = 1, db_stop = 0;
const db_key_t LEVELDB_ID_KEY = 0;
leveldb_iterator_t *db_iter;
static void leveldb_cmp_destroy(void *arg __attribute__ ((unused))) { }
static int leveldb_cmp_compare(void *arg __attribute__ ((unused)) ,
		const char *a, size_t alen __attribute__ ((unused)),
		const char *b, size_t blen __attribute__ ((unused))) {
	return *((db_key_t *)a) - *((db_key_t *)b);
}
static const char * leveldb_cmp_name(void *arg __attribute__ ((unused))) {
	return "isoblued.v1";
}
/* Magic numbers related to past data... */
#define PAST_THRESH	200
#define PAST_CNT	4

/* Function to check if any messages are buffered */
static inline void check_send(struct ring_buffer *buf, int rc,
		fd_set *write_fds)
{
	if(ring_buffer_unread_bytes(buf) || db_iter) {
		FD_SET(rc, write_fds);
	} else {
		FD_CLR(rc, write_fds);
	}
}

/* Function to wait for one or more file descriptors to be ready */
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

/* Fast method to convert to hex */
static inline char nib2hex(uint_fast8_t nib)
{
	nib &= 0x0F;

	return nib >= 10 ? nib - 10 + 'a' : nib + '0';
}

/* Function to handle incoming ISOBUS message(s) */
static inline int read_func(int sock, int iface, struct ring_buffer *buf)
{
	/* Construct msghdr to use to recevie messages from socket */
	static struct isobus_mesg mes;
	static struct sockaddr_can addr;
	static struct iovec iov = {&mes, sizeof(mes)};
	static char cmsgb[CMSG_SPACE(sizeof(struct sockaddr_can)) +
		CMSG_SPACE(sizeof(struct timeval))];
	static struct msghdr msg = {&addr, sizeof(addr), &iov, 1,
			cmsgb, sizeof(cmsgb), 0};

	if(recvmsg(sock, &msg, MSG_DONTWAIT) <= 0) {
		perror("recvmsg");
		exit(EXIT_FAILURE);
	}

	/* Get saddr and approximate arrival time */
	struct sockaddr_can daddr = { 0 };
	struct timeval tv = { 0 };
	struct cmsghdr *cmsg;
	for(cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
			cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if(cmsg->cmsg_level == SOL_CAN_ISOBUS &&
				cmsg->cmsg_type == CAN_ISOBUS_DADDR) {
			memcpy(&daddr, CMSG_DATA(cmsg), sizeof(daddr));
		} else if(cmsg->cmsg_level == SOL_SOCKET &&
				cmsg->cmsg_type == SO_TIMESTAMP) {
			memcpy(&tv, CMSG_DATA(cmsg), sizeof(tv));
		}
	}

	char *sp, *cp;
	cp = sp = ring_buffer_tail_address(buf);

	/* Print opcode (1 char) */
	*(cp++) = MESG;
	/* Print CAN interface index (1 nibble) */
	*(cp++) = nib2hex(iface);
	/* Print DB key */
	*(cp++) = nib2hex(db_id >> 28);
	*(cp++) = nib2hex(db_id >> 24);
	*(cp++) = nib2hex(db_id >> 20);
	*(cp++) = nib2hex(db_id >> 16);
	*(cp++) = nib2hex(db_id >> 12);
	*(cp++) = nib2hex(db_id >> 8);
	*(cp++) = nib2hex(db_id >> 4);
	*(cp++) = nib2hex(db_id);
	/* Print PGN (5 nibbles) */
	*(cp++) = nib2hex(mes.pgn >> 16);
	*(cp++) = nib2hex(mes.pgn >> 12);
	*(cp++) = nib2hex(mes.pgn >> 8);
	*(cp++) = nib2hex(mes.pgn >> 4);
	*(cp++) = nib2hex(mes.pgn);
	/* Print destination address (2 nibbles) */
	*(cp++) = nib2hex(daddr.can_addr.isobus.addr >> 4);
	*(cp++) = nib2hex(daddr.can_addr.isobus.addr);
	/* Print data bytes (4 nibbles length) */
	*(cp++) = nib2hex(mes.dlen >> 12);
	*(cp++) = nib2hex(mes.dlen >> 8);
	*(cp++) = nib2hex(mes.dlen >> 4);
	*(cp++) = nib2hex(mes.dlen);
	int j;
	for(j = 0; j < mes.dlen; j++)
	{
		*(cp++) = nib2hex(mes.data[j] >> 4);
		*(cp++) = nib2hex(mes.data[j]);
	}
	/* Print timestamp (8 nibbles sec, 5 nibbles usec) */
	*(cp++) = nib2hex(tv.tv_sec >> 28);
	*(cp++) = nib2hex(tv.tv_sec >> 24);
	*(cp++) = nib2hex(tv.tv_sec >> 20);
	*(cp++) = nib2hex(tv.tv_sec >> 16);
	*(cp++) = nib2hex(tv.tv_sec >> 12);
	*(cp++) = nib2hex(tv.tv_sec >> 8);
	*(cp++) = nib2hex(tv.tv_sec >> 4);
	*(cp++) = nib2hex(tv.tv_sec);
	*(cp++) = nib2hex(tv.tv_usec >> 16);
	*(cp++) = nib2hex(tv.tv_usec >> 12);
	*(cp++) = nib2hex(tv.tv_usec >> 8);
	*(cp++) = nib2hex(tv.tv_usec >> 4);
	*(cp++) = nib2hex(tv.tv_usec);
	/* Print source address (2 nibbles) */
	*(cp++) = nib2hex(addr.can_addr.isobus.addr >> 4);
	*(cp++) = nib2hex(addr.can_addr.isobus.addr);
	/* Print message ending */
	*(cp++) = '\n';

	ring_buffer_tail_advance(buf, cp-sp);

	/* Put messaged in leveldb */
	leveldb_put(db, db_woptions, (char *)&db_id, sizeof(db_id),
			sp+1, cp-sp-1, &db_err);
	if(db_err) {
		fprintf(stderr, "Leveldb write error.\n");
		leveldb_free(db_err);
		db_err = NULL;
		return  -1;
	}
	db_id++;
	leveldb_put(db, db_woptions, (char *)&LEVELDB_ID_KEY, sizeof(db_key_t),
			(char *)&db_id, sizeof(db_id), &db_err);

	return 1;
}

/* Function to send buffered messages over Bluetooth */
static inline int send_func(int rc, struct ring_buffer *buf)
{
	int chars, sent;
	char *buffer;

	chars = ring_buffer_unread_bytes(buf);

	/* Try to queue up past data for sending */
	if(db_iter && chars < PAST_THRESH) {
		int i;
		char *sp, *cp;
		sp = cp = ring_buffer_tail_address(buf);
		for(i = 0; i < PAST_CNT && leveldb_iter_valid(db_iter); i++) {
			size_t len;
			char *val;

			val = (char *)leveldb_iter_key(db_iter, &len);
			if(*((db_key_t *)val) >= db_stop) {
				leveldb_iter_destroy(db_iter);
				db_iter = NULL;
				break;
			}

			*(cp++) = OLD_MESG;
			val = (char *)leveldb_iter_value(db_iter, &len);
			memcpy(cp, val, len);
			cp += len;

			leveldb_iter_next(db_iter);
		}
		ring_buffer_tail_advance(buf, cp-sp);
	}

	buffer = ring_buffer_curs_address(buf);
	chars = ring_buffer_unread_bytes(buf);

	if((sent = send(rc, buffer, chars, MSG_DONTWAIT)) < 0) {
		perror("send");

		switch(errno) {
		case EAGAIN:
			return 0;

		default:
			return -1;
		}
	}

	//printf("BT sent %d.\n", sent);
	ring_buffer_curs_advance(buf, sent);

	return 1;
}

/* Function to handle commands from Bluetooth */
static inline int command_func(int rc, struct ring_buffer *buf, int *s)
{
	/* Buffer for reassembling commands */
	#define CMD_BUF_SIZE	0x03FFFF
	static char buffer[CMD_BUF_SIZE] = { 0 };
	static int curs = 0, tail = 0;

	printf("HERE\n");

	int chars;
	chars = recv(rc, buffer+tail, CMD_BUF_SIZE-tail, MSG_DONTWAIT);
	if(chars < 0) {
		perror("read");
		return -1;
	}
	tail += chars;

	while(true){
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
		int sock;
		char *args, *end;
		bool invalid;

		op = buffer[0];
		sock = buffer[1] >= 'a' ? buffer[1] + 10 - 'a' : buffer[1] - '0';
		args = buffer + 2;
		end = buffer + curs;
		invalid = false;

		printf("Received command %c.\n", op);

		switch(op) {
		case GET_PAST:
		{
			db_key_t key_start;

			if(sscanf(args, "%8x%8x", &key_start, &db_stop) < 2) {
				fprintf(stderr, "Invalid past data command\n");
				break;
			}

			db_iter = leveldb_create_iterator(db, db_roptions);
			leveldb_iter_seek(db_iter, (char *)&key_start, sizeof(db_key_t));

			break;
		}
		case SET_FILTERS:
		{
			char *p;
			struct isobus_filter *filts;
			int nfilts;

			if(sscanf(args, "%5x", &nfilts) < 1) {
				fprintf(stderr, "Invalid filter command\n");
				break;
			}
			p = args + 5;

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

					if((p + 5) >= end || sscanf(p, "%5x", &pgn) < 1) {
						invalid = true;
						fprintf(stderr, "Invalid filter command\n");
						break;
					}
					p += 5;

					filts[i].pgn = pgn;
					filts[i].pgn_mask = ISOBUS_PGN_MASK;
				}
			}
			if(!invalid) {
				if(setsockopt(s[sock], SOL_CAN_ISOBUS, CAN_ISOBUS_FILTER, filts,
							nfilts * sizeof(*filts)) < 0) {
					perror("setsockopt");
				}

				ring_buffer_clear(buf);
			}

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

			sscanf(args, "%5x%2x%4x%n", &pgn, &dest, &len, &nchars);
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
	}
}

/* Function that does all the work after initialization */
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
		addr.can_addr.isobus.addr = ISOBUS_ANY_ADDR;

		if(bind(s[i], (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			perror("bind can");
			return EXIT_FAILURE;
		}

		const int val = 1;
		/* Record directed address of messages */
		setsockopt(s[i], SOL_CAN_ISOBUS, CAN_ISOBUS_DADDR, &val, sizeof(val));
		/* Timestamp messages */
		setsockopt(s[i], SOL_SOCKET, SO_TIMESTAMP, &val, sizeof(val));

		FD_SET(s[i], &read_fds);
		n_fds = s[i] > n_fds ? s[i] : n_fds;
	}

	/* Initialize Leveldb */
	db_options = leveldb_options_create();
	leveldb_options_set_create_if_missing(db_options, 1);
	leveldb_comparator_t *db_cmp;
	db_cmp = leveldb_comparator_create(NULL,
			leveldb_cmp_destroy, leveldb_cmp_compare, leveldb_cmp_name);
	leveldb_options_set_comparator(db_options, db_cmp);
	db = leveldb_open(db_options, "isoblued_db", &db_err);
	if(db_err) {
		fprintf(stderr, "Leveldb open error.\n");
		exit(EXIT_FAILURE);
	}
	db_woptions = leveldb_writeoptions_create();
	leveldb_writeoptions_set_sync(db_woptions, false);
	db_roptions = leveldb_readoptions_create();
	db_iter = NULL; //leveldb_create_iterator(db, db_roptions);
	db_id = 1;
	size_t read_len;
	char * read = leveldb_get(db, db_roptions, (char *)&LEVELDB_ID_KEY,
			sizeof(db_key_t), &read_len, &db_err);
	if(db_err || !read_len) {
		leveldb_put(db, db_woptions, (char *)&LEVELDB_ID_KEY, sizeof(db_key_t),
				(char *)&db_id, sizeof(db_id), &db_err);
		if(db_err) {
			fprintf(stderr, "Leveldb db init error.\n");
		} else {
			printf("Leveldb init new db.\n");
		}
	} else {
		db_id = *(db_key_t *)read;
	}
	printf("starting at db id %d.\n", db_id);


	/* Do socket stuff */
	loop_func(n_fds, read_fds, write_fds, buf, s, ns, bt);

	sdp_close(session);

	return EXIT_SUCCESS;
}

