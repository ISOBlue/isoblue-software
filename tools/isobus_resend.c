/*
 * ISOBUS resend tool
 *
 * Resend the ISOBUS messages recorded in a file.
 *
 *
 * Author: Alex Layton <alex@layton.in>
 *
 * Copyright (C) 2014 Purdue University
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

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/time.h>

#include <argp.h>
#include <sqlite3.h>

#include "../socketcan-isobus/patched/can.h"
#include "../socketcan-isobus/isobus.h"

#define PROGRAM_NAME	"isobus_resend - ISOBUS message resender"

/* argp goodies */
#ifdef BUILD_NUM
const char *argp_program_version = PROGRAM_NAME "\n" BUILD_NUM;
#else
const char *argp_program_version = PROGRAM_NAME;
#endif
const char *argp_program_bug_address = "<bugs@isoblue.org>";
static char args_doc[] = "FILE";
static char doc[] = "Resend ISOBUS messages recorded in the given file.";
static struct argp_option options[] = {
	{NULL, 0, NULL, 0, "About", -1},
	{"max-delay", 1, "<usecs>", OPTION_HIDDEN, "Max delay time messages", 0},
	{NULL, 0, NULL, 0, "CAN Setup", 0},
	{"implement", 'i', "<iface>", 0, "CAN interface for implement bus", 0},
	{"tractor", 't', "<iface>", OPTION_ALIAS, NULL, 0},
	{"engine", 'e', "<iface>", 0, "CAN interface for engine bus", 0},
	{ 0 }
};
struct arguments {
	char *can_imp;
	char *can_eng;
	char *fname;
	long long max_delay;
};
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;
	error_t ret = 0;

	switch(key) {
	case 'i':
		arguments->can_imp = arg;
		break;

	case 'e':
		arguments->can_eng = arg;
		break;

	case 1:
		arguments->max_delay = atoll(arg);
		break;

	case ARGP_KEY_ARG:
		if(state->arg_num == 0)
			arguments->fname = arg;
		else
			return ARGP_ERR_UNKNOWN;
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

static int init_socket(char *ifname)
{
	struct sockaddr_can addr = { 0 };
	struct ifreq ifr;
	int sock;

	if((sock = socket(PF_CAN, SOCK_DGRAM | SOCK_NONBLOCK, CAN_ISOBUS))
			< 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	/* Set interface name to argument value */
	strcpy(ifr.ifr_name, ifname);
	ioctl(sock, SIOCGIFINDEX, &ifr);
	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	addr.can_addr.isobus.addr = ISOBUS_ANY_ADDR;

	if(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

	return sock;
}

#define SQL_STMT	"SELECT bus, pgn, data, LENGTH(data) AS dlen, time " \
	"FROM isobus_messages " \
	"WHERE time > 0 AND dlen > 0 " \
	"ORDER BY time;"
int main(int argc, char *argv[])
{
	int socks[2];
	sqlite3 *db;
	sqlite3_stmt *stmt;
	struct sockaddr_can addr;
	long long prev_time = 0;
	long long cur_time = 0;
	struct timeval prev_tv = { 0 };
	struct timeval cur_tv = { 0 };
	long long delay;
	int sock;
	struct isobus_mesg mesg;

	/* Handle options */
	struct arguments arguments = {
		"ib_imp",
		"ib_eng",
		NULL,
		1000000LL,
	};
	if(argp_parse(&argp, argc, argv, 0, 0, &arguments)) {
		perror("argp_parse");
		return EXIT_FAILURE;
	}

	/* Create ISOBUS sockets */
	socks[0] = init_socket(arguments.can_eng);
	socks[1] = init_socket(arguments.can_imp);
	addr.can_family = AF_CAN;
	addr.can_addr.isobus.addr = ISOBUS_GLOBAL_ADDR;

	/* SQLite goodies */
	if(sqlite3_open(arguments.fname, &db)) {
		fprintf(stderr, "SQLite3 error: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return EXIT_FAILURE;
	}
	if(sqlite3_prepare_v2(db, SQL_STMT, -1, &stmt, NULL)) {
		fprintf(stderr, "Prepared statement failed.\n");
		return EXIT_FAILURE;
	}

	while(sqlite3_step(stmt) == SQLITE_ROW) {

		switch(sqlite3_column_text(stmt, 0)[0])
		{
		/* Send on engine bus */
		case 'e':
			sock = ((int *)socks)[0];
			break;

		/* Send on implement bus */
		case 't':
		case 'i':
			sock = ((int *)socks)[1];
			break;
		default:
			continue;
		}

		/* Construct message */
		mesg.pgn = sqlite3_column_int(stmt, 1);
		mesg.dlen = sqlite3_column_int(stmt, 3);
		memcpy(&mesg.data, sqlite3_column_blob(stmt, 2), mesg.dlen);

		/* Figure out when to send this message */
		cur_time = sqlite3_column_int64(stmt, 4);
		if(gettimeofday(&cur_tv, NULL)) {
			perror("gettimeofday");
			return EXIT_FAILURE;
		}
		delay = (cur_time - prev_time) -
			((cur_tv.tv_sec - prev_tv.tv_sec) * 1000000LL +
			 (cur_tv.tv_usec - prev_tv.tv_usec));
		if(delay > 0) {
			//printf("delay: %lld\n", delay);
			usleep(delay > arguments.max_delay ? arguments.max_delay : delay);
		}

		/* Send message */
		/* TODO: Set SA and DA based on log file */
		while(sendto(sock, &mesg, sizeof(mesg), 0, (struct sockaddr *)&addr,
					sizeof(addr)) < 0) {
			/* Wait briefly, try agian */
			usleep(1);
		}
		prev_time = cur_time;
		prev_tv = cur_tv;
	}

	/* Close stuff */
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return EXIT_SUCCESS;
}

