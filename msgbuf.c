/* producer-comsumer style, MAP_ANONYMOUS|MAP_SHARED mmap'd shared memory
 * IPC. Uses C99 flexible array member. Message buffer size is fixed, too large 
 * messages are truncated. Receiver buffers should be at least the same size as
 * the message buffer itself. Messages are copied into and copied out of the buffer
 * which may be wasteful depending on the use case. Only one producer and one consumer
 * is allowed. If more are wanted/needed, add a mutex for changing buf and len.
 *
 * If the parent is trying to put/get something to the child
 * and the child dies, the parent will get SIGCHLD. If that happens,
 * sem_wait in msgbuf_get will return -1 and errno will be EINTR. Parent
 * may need to check this to determine a suitable cause of action */

#include <stdio.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <string.h>
#include "msgbuf.h"

struct msgbuf_t {
	// number of slots (1 or 0) available to put anything into 
	sem_t nslots_avail;
	// number of slots (1 or 0) currently occupied
	sem_t nslots_used;
	// size of buf
	size_t size;
	// number of char's used in buf 
	size_t len;
	// message buffer
	char buf[];
};

msgbuf_t *msgbuf_new(size_t size) {
	msgbuf_t *buf = NULL;

	buf = mmap(NULL, sizeof(msgbuf_t) + size, PROT_READ|PROT_WRITE,
			MAP_ANONYMOUS|MAP_SHARED, -1, 0);
	if (buf == NULL) {
		return NULL;
	}

	memset(buf, 0, sizeof(msgbuf_t));
	buf->size = size;
	if (sem_init(&buf->nslots_avail, 1, 1) < 0) {
		munmap(buf, buf->size+sizeof(msgbuf_t));
		return NULL;
	}

	if (sem_init(&buf->nslots_used, 1, 0) < 0) {
		sem_destroy(&buf->nslots_avail);
		munmap(buf, buf->size+sizeof(msgbuf_t));
		return NULL;
	}

	return buf;
}

void msgbuf_free(msgbuf_t *buf) {
	if (buf != NULL) {
		sem_destroy(&buf->nslots_avail);
		sem_destroy(&buf->nslots_used);
		munmap(buf, buf->size+sizeof(msgbuf_t));
	}
}

int msgbuf_put(msgbuf_t *buf, void *data, size_t len) {
	size_t actual;

	if (sem_wait(&buf->nslots_avail) < 0) {
		return -1;
	}

	actual = len > buf->size ? buf->size : len;
	memcpy(buf->buf, data, actual);
	buf->len = actual;

	if (sem_post(&buf->nslots_used) < 0) {
		return -1;
	}

	return 0;
}

int msgbuf_get(msgbuf_t *buf, void *data, size_t *len) {
	if (sem_wait(&buf->nslots_used) < 0) {
		return -1;
	}

	memcpy(data, buf->buf, buf->len);
	*len = buf->len;

	if (sem_post(&buf->nslots_avail) < 0) {
		return -1;
	}

	return 0;
}

