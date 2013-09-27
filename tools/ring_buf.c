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

#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>
 
/* Not technically required, but needed on some UNIX distributions */
#include <sys/types.h>
#include <sys/stat.h>

#include "ring_buf.h"

#define FOOTER_LEN	(sizeof(((struct ring_buffer *)0)->tail_offset) + \
		sizeof(((struct ring_buffer*)0)->head_offset))

static void _ring_buffer_curs_advance(struct ring_buffer *buffer,
		unsigned long count_bytes);
static void _ring_buffer_tail_advance(struct ring_buffer *buffer,
		unsigned long count_bytes);

//Warning order should be at least 12 for Linux
int ring_buffer_create(struct ring_buffer *buffer, unsigned long order,
		char path[])
{
	void *address;
	int status;

	buffer->fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if(buffer->fd < 0)
		return buffer->fd;

	buffer->count_bytes = 1UL << order;

	status = ftruncate(buffer->fd, buffer->count_bytes + FOOTER_LEN);
	if(status)
		return status;

	lseek(buffer->fd, -FOOTER_LEN, SEEK_END);
	read(buffer->fd, &buffer->head_offset, sizeof(buffer->head_offset));
	read(buffer->fd, &buffer->tail_offset, sizeof(buffer->tail_offset));
	buffer->start_offset = buffer->tail_offset;
	buffer->curs_offset = buffer->tail_offset;

	buffer->address = mmap(NULL, buffer->count_bytes << 1, PROT_NONE,
						   MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

	if(buffer->address == MAP_FAILED)
		return -1;

	address = mmap(buffer->address, buffer->count_bytes, PROT_READ | PROT_WRITE,
				 MAP_FIXED | MAP_SHARED, buffer->fd, 0);

	if(address != buffer->address)
		return -1;

	address = mmap(buffer->address + buffer->count_bytes,
				   buffer->count_bytes, PROT_READ | PROT_WRITE,
				   MAP_FIXED | MAP_SHARED, buffer->fd, 0);

	if(address != buffer->address + buffer->count_bytes)
		return -1;

	pthread_cond_init(&buffer->unread_cond, NULL);
	pthread_mutex_init(&buffer->unread_mut, NULL);

	return buffer->fd;
}

static inline unsigned long _buf_mod(struct ring_buffer *buffer,
		unsigned long offset)
{
	return offset & (buffer->count_bytes - 1);
}

#define OFF_DIST(buf, off1, off2) _buf_mod(buf, buf->off2 - buf->off1)

int ring_buffer_free(struct ring_buffer *buffer)
{
	int status;

	status = munmap(buffer->address, buffer->count_bytes << 1);

	return status | close(buffer->fd);
}

void *ring_buffer_head_address(struct ring_buffer *buffer)
{
	return buffer->address + buffer->head_offset;
}

void ring_buffer_head_advance(struct ring_buffer *buffer,
		unsigned long count_bytes)
{
	unsigned long dist_start, dist_curs;

	dist_start = OFF_DIST(buffer, head_offset, start_offset);
	dist_curs = OFF_DIST(buffer, head_offset, curs_offset);

	if(dist_start < count_bytes)
		ring_buffer_start_advance(buffer, count_bytes - dist_start);
	if(dist_curs < count_bytes)
		_ring_buffer_curs_advance(buffer, count_bytes - dist_curs);

	buffer->head_offset += count_bytes;
	buffer->head_offset = _buf_mod(buffer, buffer->head_offset);

	lseek(buffer->fd, -FOOTER_LEN, SEEK_END);
	write(buffer->fd, &buffer->head_offset, sizeof(buffer->head_offset));
}

void *ring_buffer_start_address(struct ring_buffer *buffer)
{
	return buffer->address + buffer->start_offset;
}

void ring_buffer_start_advance(struct ring_buffer *buffer,
		unsigned long count_bytes)
{
	unsigned long dist;

	dist = OFF_DIST(buffer, start_offset, tail_offset);

	if(dist < count_bytes)
		_ring_buffer_tail_advance(buffer, count_bytes - dist);

	buffer->start_offset += count_bytes;
	buffer->start_offset = _buf_mod(buffer, buffer->start_offset);
}

void *ring_buffer_curs_address(struct ring_buffer *buffer)
{
	return buffer->address + buffer->curs_offset;
}

void ring_buffer_seek_curs_head(struct ring_buffer *buffer)
{
	pthread_mutex_lock(&buffer->unread_mut);
	pthread_cleanup_push((void (*)(void *)) pthread_mutex_unlock,
			&buffer->unread_mut);

	buffer->curs_offset = buffer->head_offset;

	pthread_cond_broadcast(&buffer->unread_cond);

	pthread_cleanup_pop(1);
}

void ring_buffer_seek_curs_start(struct ring_buffer *buffer)
{
	pthread_mutex_lock(&buffer->unread_mut);
	pthread_cleanup_push((void (*)(void *)) pthread_mutex_unlock,
			&buffer->unread_mut);

	buffer->curs_offset = buffer->start_offset;

	pthread_cond_broadcast(&buffer->unread_cond);

	pthread_cleanup_pop(1);
}

void ring_buffer_seek_curs_tail(struct ring_buffer *buffer)
{
	pthread_mutex_lock(&buffer->unread_mut);
	pthread_cleanup_push((void (*)(void *)) pthread_mutex_unlock,
			&buffer->unread_mut);

	buffer->curs_offset = buffer->tail_offset;

	pthread_cond_broadcast(&buffer->unread_cond);

	pthread_cleanup_pop(1);
}

static void _ring_buffer_curs_advance(struct ring_buffer *buffer,
		unsigned long count_bytes)
{
	unsigned long dist;

	dist = OFF_DIST(buffer, curs_offset, tail_offset);

	//if(dist < count_bytes)
		//_ring_buffer_tail_advance(buffer, count_bytes - dist);

	if(dist < count_bytes)
		buffer->curs_offset += dist;
	else
		buffer->curs_offset += count_bytes;
	buffer->curs_offset = _buf_mod(buffer, buffer->curs_offset);

	pthread_cond_broadcast(&buffer->unread_cond);
}

void ring_buffer_curs_advance(struct ring_buffer *buffer,
		unsigned long count_bytes)
{
	pthread_mutex_lock(&buffer->unread_mut);
	pthread_cleanup_push((void (*)(void *)) pthread_mutex_unlock,
			&buffer->unread_mut);

	_ring_buffer_curs_advance(buffer, count_bytes);

	pthread_cleanup_pop(1);
}

void *ring_buffer_tail_address(struct ring_buffer *buffer)
{
	return buffer->address + buffer->tail_offset;
}

static void _ring_buffer_tail_advance(struct ring_buffer *buffer,
		unsigned long count_bytes)
{
	unsigned long dist;

	dist = OFF_DIST(buffer, tail_offset, head_offset);

	/* There is a weird egde case when the file is empty... */
	if(dist && dist < count_bytes)
		ring_buffer_head_advance(buffer, count_bytes - dist + 1);

	buffer->tail_offset += count_bytes;
	buffer->tail_offset = _buf_mod(buffer, buffer->tail_offset);

	lseek(buffer->fd, -sizeof(buffer->tail_offset), SEEK_END);
	write(buffer->fd, &buffer->tail_offset, sizeof(buffer->tail_offset));

	pthread_cond_broadcast(&buffer->unread_cond);
}

void ring_buffer_tail_advance(struct ring_buffer *buffer,
		unsigned long count_bytes)
{
	pthread_mutex_lock(&buffer->unread_mut);
	pthread_cleanup_push((void (*)(void *)) pthread_mutex_unlock,
			&buffer->unread_mut);

	_ring_buffer_tail_advance(buffer, count_bytes);

	pthread_cleanup_pop(1);
}

unsigned long ring_buffer_filled_bytes(struct ring_buffer *buffer)
{
	return _buf_mod(buffer, buffer->tail_offset - buffer->head_offset);
}

unsigned long ring_buffer_unread_bytes(struct ring_buffer *buffer)
{
	return _buf_mod(buffer, buffer->tail_offset - buffer->curs_offset);
}

void ring_buffer_wait_unread_bytes(struct ring_buffer *buffer)
{
	pthread_mutex_lock(&buffer->unread_mut);
	pthread_cleanup_push((void (*)(void *)) pthread_mutex_unlock,
			&buffer->unread_mut);

	while(!ring_buffer_unread_bytes(buffer))
		pthread_cond_wait(&buffer->unread_cond, &buffer->unread_mut);

	pthread_cleanup_pop(1);
}

unsigned long ring_buffer_free_bytes(struct ring_buffer *buffer)
{
	return buffer->count_bytes - ring_buffer_filled_bytes(buffer);
}

void ring_buffer_clear(struct ring_buffer *buffer)
{
	buffer->head_offset = 0;
	buffer->tail_offset = 0;
	buffer->start_offset = 0;
	buffer->curs_offset = 0;

	lseek(buffer->fd, -FOOTER_LEN, SEEK_END);
	write(buffer->fd, &buffer->head_offset, sizeof(buffer->head_offset));
	write(buffer->fd, &buffer->tail_offset, sizeof(buffer->tail_offset));

	pthread_cond_broadcast(&buffer->unread_cond);
}

/* 
 * Note, that initial anonymous mmap() can be avoided - after initial mmap() for
 * descriptor fd,
 * you can try mmap() with hinted address as (buffer->address +
 * buffer->count_bytes) and if it fails - another one with hinted address as
 * (buffer->address - buffer->count_bytes).  Make sure MAP_FIXED is not used in
 * such case, as under certain situations it could end with segfault.  The
 * advantage of such approach is, that it avoids requirement to map twice the
 * amount you need initially (especially useful e.g. if you want to use
 * hugetlbfs and the allowed amount is limited) and in context of gcc/glibc -
 * you can avoid certain feature macros (MAP_ANONYMOUS usually requires one of
 * _BSD_SOURCE, _SVID_SOURCE or _GNU_SOURCE).
 */
