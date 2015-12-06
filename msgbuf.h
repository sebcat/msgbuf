#ifndef __MSGBUF_H
#define __MSGBUF_H

typedef struct msgbuf_t msgbuf_t;


msgbuf_t *msgbuf_new(size_t size);
void msgbuf_free(msgbuf_t *buf);
int msgbuf_put(msgbuf_t *buf, void *data, size_t len);
int msgbuf_get(msgbuf_t *buf, void *data, size_t *len);

#endif
