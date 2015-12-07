#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include "msgbuf.h"

#define MSGBUF_SZ	8

/* fields set by parent signal handler */
static int _child_is_dead = 0;
static int _child_exit = 0;

static void sigchld_handler(int signum, siginfo_t *si, void *unused) {
	int status;
	int old_errno = errno;

	if(si->si_code != CLD_EXITED) {
		return;
	}

	if (waitpid(si->si_pid, &status, 0) >= 0) {
		_child_is_dead = 1;
		if (WIFEXITED(status)) {
			_child_exit = WEXITSTATUS(status);
		} else {
			_child_exit = EXIT_FAILURE;
		}
	}

	errno = old_errno;
}


/* read CHLD_READ_LIMIT messages */
#define CHLD_READ_LIMIT	1000000
static void child(msgbuf_t *buf) {
	int i;
	char data[MSGBUF_SZ+1];
	size_t len;

	memset(data, 0, sizeof(data));
	for(i=0; i<CHLD_READ_LIMIT; i++) {
		if (msgbuf_get(buf, data, &len) < 0) {
			exit(1);
		}
	}
}

/* write messages to child until death of child. Returns
 * EXIT_SUCCESS if child dies properly, EXIT_FAILURE otherwise */
static int parent(msgbuf_t *buf, pid_t cpid, char *msg, size_t msglen) {
	int ret, nread=0;
	struct timespec start, end, diff;

	// CLOCK_MONOTONIC is used for portability reasons
	clock_gettime(CLOCK_MONOTONIC, &start);
	while(!_child_is_dead) {
		ret = msgbuf_put(buf, msg, msglen);
		if (ret == -1 && errno != EINTR) {
			return EXIT_FAILURE;
		} else if (ret >= 0) {
			nread++;
		}
	}

	clock_gettime(CLOCK_MONOTONIC, &end);
	if ((end.tv_nsec-start.tv_nsec) < 0) {
		diff.tv_sec = end.tv_sec-start.tv_sec-1;
		diff.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		diff.tv_sec = end.tv_sec-start.tv_sec;
		diff.tv_nsec = end.tv_nsec-start.tv_nsec;
	}

	printf("%d messages written in %ld.%lds (child exit %d)\n", nread, (long)diff.tv_sec, diff.tv_nsec, _child_exit);
	return _child_exit;
}

int main() {
	msgbuf_t *msgbuf;
	pid_t pid;
	int ret = EXIT_FAILURE;
	struct sigaction sa;

	msgbuf = msgbuf_new(MSGBUF_SZ);
	if (msgbuf == NULL) {
		perror("msgbuf_new");
		return EXIT_FAILURE;
	}

	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		return EXIT_FAILURE;
	}

	pid = fork();
	if (pid < 0) {
		perror("fork");
		return EXIT_FAILURE;
	} else if (pid == 0) {
		child(msgbuf);
		exit(0);
	} else {
		ret = parent(msgbuf, pid, "pingping", 8);
	}

	msgbuf_free(msgbuf);
	return ret;
}
