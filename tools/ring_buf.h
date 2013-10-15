/*
 * Circular Buffer Library
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

#ifndef CIRC_BUF_H
#define CIRC_BUF_H

/* #include <pthread.h> */

struct ring_buffer
{
	char *address;
	int fd;

	/* pthread_cond_t unread_cond; */
	/* pthread_mutex_t unread_mut; */

	unsigned long count_bytes;
	unsigned long tail_offset;
	unsigned long head_offset;
	unsigned long start_offset;
	unsigned long curs_offset;
};

int ring_buffer_create(struct ring_buffer *buffer, unsigned long order,
		char path[]);
int ring_buffer_free(struct ring_buffer *buffer);
void *ring_buffer_head_address(struct ring_buffer *buffer);
void ring_buffer_head_advance(struct ring_buffer *buffer,
		unsigned long count_bytes);
void *ring_buffer_start_address(struct ring_buffer *buffer);
void ring_buffer_start_advance(struct ring_buffer *buffer,
		unsigned long count_bytes);
void *ring_buffer_curs_address(struct ring_buffer *buffer);
void ring_buffer_curs_advance(struct ring_buffer *buffer,
		unsigned long count_bytes);
void *ring_buffer_tail_address(struct ring_buffer *buffer);
void ring_buffer_tail_advance(struct ring_buffer *buffer,
		unsigned long count_bytes);
void ring_buffer_seek_curs_head(struct ring_buffer *buffer);
void ring_buffer_seek_curs_start(struct ring_buffer *buffer);
void ring_buffer_seek_curs_tail(struct ring_buffer *buffer);
unsigned long ring_buffer_filled_bytes(struct ring_buffer *buffer);
unsigned long ring_buffer_unread_bytes(struct ring_buffer *buffer);
void ring_buffer_wait_unread_bytes(struct ring_buffer *buffer);
unsigned long ring_buffer_free_bytes(struct ring_buffer *buffer);
void ring_buffer_clear(struct ring_buffer *buffer);

#endif /* CIRC_BUF_H */
