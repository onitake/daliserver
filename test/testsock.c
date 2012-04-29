#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif
#include <sys/socket.h>

void *producer_loop(void *arg) {
	printf("Entering producer\n");

	int socket = *(int *) arg;
	
	int connected = 1;
	unsigned int i;
	for (i = 0; connected && i < 3; i++) {
		char buf[30];
		snprintf(buf, sizeof(buf), "This is message #%u", i);
		ssize_t sent = send(socket, buf, strlen(buf), 0);
		if (sent == -1) {
			fprintf(stderr, "Error sending data: %s\n", strerror(errno));
			connected = 0;
		} else {
			printf("Sent %ld bytes\n", sent);
			usleep(1000000);
		}
	}
	
	close(socket);
	
	printf("Exiting producer\n");

	return NULL;
}

void *consumer_loop(void *arg) {
	printf("Entering consumer\n");

	int socket = *(int *) arg;

	int connected = 1;
	while (connected) {
		struct pollfd fds[1];
		fds[0].fd = socket;
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		int rdy_fds = poll(fds, 1, 2000);
		if (rdy_fds == -1) {
			fprintf(stderr, "Error waiting for data: %s\n", strerror(errno));
			connected = 0;
		} else if (rdy_fds == 1) {
			if ((fds[0].revents & POLLERR) || (fds[0].revents & POLLHUP)) {
				fprintf(stderr, "Socket closed\n");
				connected = 0;
			} else if (fds[0].revents & POLLIN) {
				char buf[30];
				ssize_t recvd = recv(socket, buf, sizeof(buf), 0);
				if (recvd == -1) {
					fprintf(stderr, "Error receiving data: %s\n", strerror(errno));
					connected = 0;
				} else {
					printf("Received %ld bytes: %s\n", recvd, buf);
				}
			}
		} else {
			printf("Timeout waiting for messages, revents = %x\n", fds[0].revents);
			connected = 0;
		}
	}

	printf("Exiting consumer\n");

	return NULL;
}

int main(int argc, char **argv) {
#ifdef HAVE_PTHREAD

	printf("Setting up IPC\n");

	int sockets[2];
	// SOCK_STREAM delivers data in the same chunks as they were sent, at least on Mach.
	// If this is not the case in your OS, SOCK_DGRAM must be used, but this has
	// the implication that sockets can't be closed. Termination must be signalled some other way.
	if (socketpair(PF_LOCAL, SOCK_STREAM, 0, sockets) == -1) {
		fprintf(stderr, "Error creating socket pair: %s\n", strerror(errno));
		return -1;
	}

	pthread_t consumer;
	if (pthread_create(&consumer, NULL, consumer_loop, &sockets[0]) == -1) {
		fprintf(stderr, "Error creating consumer thread: %s\n", strerror(errno));
		return -1;
	}

	pthread_t producer;
	if (pthread_create(&producer, NULL, producer_loop, &sockets[1]) == -1) {
		fprintf(stderr, "Error creating producer thread: %s\n", strerror(errno));
		return -1;
	}

	printf("Waiting for threads to complete\n");

	if (pthread_join(producer, NULL) == -1) {
		fprintf(stderr, "Error joining producer thread: %s\n", strerror(errno));
	}
	if (pthread_join(consumer, NULL) == -1) {
		fprintf(stderr, "Error joining consumer thread: %s\n", strerror(errno));
	}
	
#else
	printf("Socket test skipped (thread support not enabled)\n");
#endif

	return 0;
}
