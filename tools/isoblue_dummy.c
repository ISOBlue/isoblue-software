/*
 * ISOBlue "dummy" client application
 *
 * Pretneds to be the library, connecting to the ISOBlue daemon.
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

#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

int main(int argc, char *argv[])
{
	int bt;
	struct sockaddr_rc rc_addr = { 0 };

	char buf[1024] = { 0 };
	int chars;

	/* Check arguments */
	if(argc != 3) {
		fprintf(stderr, "wrong argument count\n");
		return EXIT_FAILURE;
	}

	if((bt = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM)) < 0) {
		perror("socket");
		return EXIT_FAILURE;
	}

	/* Parse address */
	rc_addr.rc_family = AF_BLUETOOTH;
	if(str2ba(argv[1], &rc_addr.rc_bdaddr) < 0) {
		perror("str2ba");
		return EXIT_FAILURE;
	}
	rc_addr.rc_channel = atoi(argv[2]);

	/* Connect to ISOBlue Daemon */
	if(connect(bt, (struct sockaddr *)&rc_addr, sizeof(rc_addr)) < 0) {
		perror("connect");
		return EXIT_FAILURE;
	}

	/* Send filter(s) to ISOBlue */
	static const char filt_cmd[] = "F000000\nF100000\n";
	if(send(bt, filt_cmd, strlen(filt_cmd), MSG_WAITALL) != strlen(filt_cmd)) {
		perror("send");
		return EXIT_FAILURE;
	}

	/* Send past data command to ISOBlue */
	static const char past_cmd[] = "P00000000100000010\n";
	if(send(bt, past_cmd, strlen(past_cmd), MSG_WAITALL) != strlen(past_cmd)) {
		perror("send");
		return EXIT_FAILURE;
	}

	/* Print all received data */
	while((chars = recv(bt, buf, sizeof(buf)-1, 0)) > 0) {
		buf[chars] = '\0';
		fputs(buf, stdout);
	}

	/* Should not end */
	return EXIT_FAILURE;
}

