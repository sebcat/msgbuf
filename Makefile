CFLAGS		:= -Wall -Werror -O2
LDFLAGS 	:= -lpthread
RM			:= rm -f
AR			:= ar rcs
SRCS		:= main.c msgbuf.c

.PHONY: clean distclean

all: msgbuf

libmsgbuf.a: msgbuf.c
	$(CC) $(CFLAGS) -c msgbuf.c
	$(AR) $@ msgbuf.o

msgbuf: libmsgbuf.a main.c
	$(CC) $(CFLAGS) -s -o $@ main.c libmsgbuf.a $(LDFLAGS)

clean:
	$(RM) msgbuf libmsgbuf.a msgbuf.o

distclean: clean
