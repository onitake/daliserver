#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include "dispatch.h"

static void readyfn(void *arg) {
	if (!arg) {
		printf("readyfn got a NULL arg\n");
		exit(1);
	} else {
	}
}

static void errorfn(void *arg, DispatchError err) {
	if (!arg) {
		printf("errorfn got a NULL arg\n");
		exit(1);
	} else {
		switch (err) {
		case DISPATCH_FD_CLOSED:
			printf("errorfn got fd closed\n");
			break;
		case DISPATCH_POLL_ERROR:
			printf("errorfn got poll error\n");
			break;
		case DISPATCH_FD_INVALID:
			printf("errorfn got fd invalid\n");
			break;
		default:
			printf("errorfn got unknown error %d\n", err);
			break;
		}
	}
}

static void indexfn(void *arg, size_t index) {
	if (!arg) {
		printf("indexfn got a NULL arg\n");
		exit(1);
	} else {
	}
}

static int createtmp(const char *template) {
	char *tempfile = strdup(template);
	int fd = mkstemp(tempfile);
	if (fd == -1) {
		perror("Can't open temp file");
		exit(1);
	}
	free(tempfile);
	return fd;
}

struct DispatchEntry {
	void *arg;
	DispatchReadyFunc readyfn;
	DispatchErrorFunc errorfn;
	DispatchIndexFunc indexfn;
};

struct Dispatch {
	size_t numentries;
	size_t allocentries;
	struct DispatchEntry *entries;
	struct pollfd *fds;
};

static void testfd(DispatchPtr disp, int index, int fd, void *arg) {
	if (disp->fds[index].fd != fd) {
		printf("Queue out of order: fd %d should be %d\n", disp->fds[index].fd, fd);
		exit(1);
	}
	if (disp->entries[index].arg != arg) {
		printf("Queue out of order: arg %p should be %p\n", disp->entries[index].arg, arg);
		exit(1);
	}
}

int main(int argc, char **argv) {
	printf("Test 1: Data layout\n");
	
	DispatchPtr disp = dispatch_new();
	if (!disp) {
		printf("Couldn't allocate dispatch queue\n");
		return 1;
	}

	size_t i;
	for (i = 0; i < 20; i++) {
		dispatch_add(disp, i, -1, NULL, NULL, NULL, (void *) (i + 20));
	}
	
	if (disp->numentries != 20) {
		printf("Queue contains wrong number of entries: %lu expected: %u\n", disp->numentries, 20);
		return 1;
	}
	
	void *entries[20];
	for (i = 0; i < 20; i++) {
		if (disp->fds[i].fd != i) {
			printf("Queue out of order: %d should be %lu\n", disp->fds[i].fd, i);
			return 1;
		}
		entries[i] = disp->entries[i].arg;
	}
	
	dispatch_remove_fd(disp, 1);
	dispatch_remove_fd(disp, 5);
	dispatch_remove_fd(disp, 11);
	dispatch_remove_fd(disp, 19);

	if (disp->numentries != 16) {
		printf("Queue contains wrong number of entries: %lu expected: %u\n", disp->numentries, 16);
		return 1;
	}

	testfd(disp, 0, 0, (void *) 20);
	testfd(disp, 1, 16, (void *) 36);
	testfd(disp, 2, 2, (void *) 22);
	testfd(disp, 3, 3, (void *) 23);
	testfd(disp, 4, 4, (void *) 24);
	testfd(disp, 5, 18, (void *) 38);
	testfd(disp, 6, 6, (void *) 26);
	testfd(disp, 7, 7, (void *) 27);
	testfd(disp, 8, 8, (void *) 28);
	testfd(disp, 9, 9, (void *) 29);
	testfd(disp, 10, 10, (void *) 30);
	testfd(disp, 11, 17, (void *) 37);
	testfd(disp, 12, 12, (void *) 32);
	testfd(disp, 13, 13, (void *) 33);
	testfd(disp, 14, 14, (void *) 34);
	testfd(disp, 15, 15, (void *) 35);
	
	dispatch_free(disp);
	
	printf("Test 2: Dispatch\n");

	disp = dispatch_new();

	int fd1 = createtmp("dali-test-dispatch-000-XXXXXX");
	dispatch_add(disp, fd1, -1, readyfn, errorfn, indexfn, disp);

	int status = dispatch_run(disp, 1000);
	switch (status) {
	case DISPATCH_ERROR:
		printf("dispatch_run error\n");
		break;
	case DISPATCH_TIMEOUT:
		printf("dispatch_run timeout\n");
		break;
	case DISPATCH_EVENT_HANDLED:
		printf("dispatch_run handled event\n");
		break;
	case DISPATCH_NO_EVENTS:
		printf("dispatch_run no event waiting\n");
		break;
	default:
		printf("dispatch_run unknown status code: %d\n", status);
		break;
	}

	dispatch_free(disp);

	return 0;
}

