/* Copyright (c) 2011, onitake <onitake@gmail.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "list.h"
#include "util.h"
#include "usb.h"

const DaliFrame ALL_OFF = { 0xff, 0x00, };
const DaliFrame ALL_ON = { 0xff, 0x08, };
const DaliFrame ALL_DIM = { 0xfe, 0x60, };

// Listen on this port
const unsigned short NET_PORT = 55825;
// Bind to this address
const char *NET_ADDRESS = "0.0.0.0";
// Network protocol:
// Packet {
//     address:uint8_t
//     command:uint8_t
// }

static int running;

typedef struct {
	int error;
} LoopReturn;

void *loop_return_new(int error) {
	LoopReturn *ret = malloc(sizeof(LoopReturn));
	ret->error = error;
	if (error != 0) {
		running = 0;
	}
	return ret;
}

typedef struct {
	ListPtr oqueue;
	ListPtr iqueue;
	int socket;
} HandlerLoopArg;

void *handler_loop(void *arg) {
	printf("Entering handler thread\n");
	HandlerLoopArg *handler_loop_arg = (HandlerLoopArg *) arg;
	int connected = 1;
	while (running && connected) {
		struct pollfd fds[1];
		fds[0].fd = handler_loop_arg->socket;
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		int rdy_fds = poll(fds, 1, 100);
		if (rdy_fds == -1) {
			fprintf(stderr, "Error waiting for data: %s\n", strerror(errno));
			connected = 0;
		}
		if (rdy_fds == 1) {
			if (fds[0].revents & POLLIN) {
				char buffer[2];
				ssize_t rd_bytes = read(handler_loop_arg->socket, buffer, sizeof(buffer));
				if (rd_bytes == -1) {
					if (errno != ECONNRESET) {
						fprintf(stderr, "Error reading from socket: %s\n", strerror(errno));
					} else {
						printf("Connection closed, exiting handler\n");
					}
					connected = 0;
				}
				if (rd_bytes == 2) {
					printf("Got packet\n");
					DaliFrame *frame = daliframe_new(buffer[0], buffer[1]);
					list_enqueue(handler_loop_arg->oqueue, frame);
				}
			}
			if ((fds[0].revents & POLLHUP) || (fds[0].revents & POLLERR)) {
				printf("Connection closed\n");
				connected = 0;
			}
		}
		DaliFrame *frame = list_dequeue(handler_loop_arg->iqueue);
		if (frame) {
			printf("Sending packet\n");
			char buffer[2];
			buffer[0] = frame->address;
			buffer[1] = frame->command;
			ssize_t wr_bytes = write(handler_loop_arg->socket, buffer, sizeof(buffer));
			if (wr_bytes == -1) {
				if (errno != ECONNRESET) {
					fprintf(stderr, "Error writing to socket: %s\n", strerror(errno));
				} else {
					printf("Connection closed, exiting handler\n");
				}
				connected = 0;
			}
			printf("Wrote %ld bytes\n", wr_bytes);
			daliframe_free(frame);
		}
	}
	close(handler_loop_arg->socket);
	printf("Exiting handler thread\n");
	return NULL;
}

typedef struct {
	ListPtr oqueue;
	ListPtr qlist;
} ServerLoopArg;

void *server_loop(void *arg) {
	printf("Entering server thread\n");
	ServerLoopArg *server_loop_arg = (ServerLoopArg *) arg;
	int listener = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener == -1) {
		fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
		return loop_return_new(-1);
	}
	struct sockaddr_in all_if;
	memset(&all_if, 0, sizeof(all_if));
	all_if.sin_family = AF_INET;
	all_if.sin_port = htons(NET_PORT);
	if (inet_pton(AF_INET, NET_ADDRESS, &all_if.sin_addr) == -1) {
		fprintf(stderr, "Error converting address: %s\n", strerror(errno));
		close(listener);
		return loop_return_new(-1);
	}
	if (bind(listener, (struct sockaddr *) &all_if, sizeof(all_if)) == -1) {
		fprintf(stderr, "Error binding socket: %s\n", strerror(errno));
		close(listener);
		return loop_return_new(-1);
	}
	if (listen(listener, 10) == -1) {
		fprintf(stderr, "Error listening on socket: %s\n", strerror(errno));
		close(listener);
		return loop_return_new(-1);
	}
	int connected = 1;
	while (running && connected) {
		struct pollfd fds[1];
		fds[0].fd = listener;
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		int rdy_fds = poll(fds, 1, 100);
		if (rdy_fds == -1) {
			fprintf(stderr, "Error waiting for incoming connections: %s\n", strerror(errno));
			close(listener);
			return loop_return_new(-1);
		}
		if (rdy_fds == 1) {
			if ((fds[0].revents & POLLERR) || (fds[0].revents & POLLHUP)) {
				printf("Connection closed\n");
				connected = 0;
			} else if (fds[0].revents & POLLIN) {
				struct sockaddr_in incoming;
				socklen_t incoming_size = sizeof(incoming);
				int socket = accept(listener, (struct sockaddr *) &incoming, &incoming_size);
				if (socket == -1) {
					fprintf(stderr, "Error accepting connection: %s\n", strerror(errno));
					connected = 0;
				} else {
					if (incoming.sin_family != AF_INET) {
						fprintf(stderr, "Invalid address family from incoming connection %d\n", incoming.sin_family);
					} else {
						char addr[16];
						printf("Got connection from %s:%u\n", inet_ntop(incoming.sin_family, &incoming.sin_addr, addr, sizeof(addr)), incoming.sin_port);
						HandlerLoopArg handler_loop_arg;
						handler_loop_arg.oqueue = server_loop_arg->oqueue;
						handler_loop_arg.iqueue = list_new((ListDataFreeFunc) daliframe_free);
						handler_loop_arg.socket = socket;
						list_enqueue(server_loop_arg->qlist, handler_loop_arg.iqueue);
						pthread_t handler;
						pthread_create(&handler, NULL, handler_loop, &handler_loop_arg);
					}
				}
			}
		}
	}
	close(listener);
	printf("Exiting server thread\n");
	return loop_return_new(0);
}

typedef struct {
	ListPtr oqueue;
	ListPtr qlist;
	UsbDali *dali;
} UsbLoopArg;

void usb_broadcast_callback(UsbDaliError err, DaliFrame *response, void *arg) {
}

void usb_response_callback(UsbDaliError err, DaliFrame *response, void *arg) {
}

void *usb_loop(void *arg) {
	printf("Entering USB thread\n");
	UsbLoopArg *usb_loop_arg = (UsbLoopArg *) arg;
	unsigned int c = 0;
	while (running) {
#ifndef USB_OFF
		if (usb_loop_arg->dali) {
			usbdali_handle(usb_loop_arg->dali);
		}
#endif
		
		DaliFrame *cmd_out = list_dequeue(usb_loop_arg->oqueue);
		if (cmd_out) {
			printf("USB out address=0x%02x command=0x%02x\n", cmd_out->address, cmd_out->command);
#ifndef USB_OFF
			usbdali_queue(usb_loop_arg->dali, cmd_out, usb_response_callback, NULL);
#endif
		}
	}
	printf("Exiting USB thread\n");
	return loop_return_new(0);
}

void signal_handler(int sig) {
	if (running) {
		printf("Signal received, shutting down\n");
		running = 0;
	} else {
		printf("Another signal received, killing process\n");
		kill(getpid(), SIGKILL);
	}
}

int main(int argc, const char **argv) {
	UsbDali *dali = NULL;
#ifndef USB_OFF
	dali = usbdali_open(NULL, usb_broadcast_callback, NULL);
	if (!dali) {
		return -1;
	}
#endif
	
	// Output command queue
	ListPtr out_queue = list_new((ListDataFreeFunc) daliframe_free);
	// List the queues for all connections
	ListPtr queue_list = list_new((ListDataFreeFunc) list_free);
	
	running = 1;
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	
	ServerLoopArg server_loop_arg;
	server_loop_arg.oqueue = out_queue;
	server_loop_arg.qlist = queue_list;
	pthread_t server;
	pthread_create(&server, NULL, server_loop, &server_loop_arg);

	UsbLoopArg usb_loop_arg;
	usb_loop_arg.oqueue = out_queue;
	usb_loop_arg.qlist = queue_list;
	usb_loop_arg.dali = dali;
#ifdef USB_THREAD
	pthread_t usb;
	pthread_create(&usb, NULL, usb_loop, &usb_loop_arg);

	LoopReturn *usb_ret = NULL;
	pthread_join(usb, (void **) &usb_ret);
#else
	LoopReturn *usb_ret = usb_loop(&usb_loop_arg);
#endif
	if (usb_ret) {
		printf("USB thread returned: %d\n", usb_ret->error);
		free(usb_ret);
	}
	
	LoopReturn *server_ret = NULL;
	pthread_join(server, (void **) &server_ret);
	if (server_ret) {
		printf("Server thread returned: %d\n", server_ret->error);
		free(server_ret);
	}
	
	list_free(queue_list);
	list_free(out_queue);
	
#ifndef USB_OFF
	usbdali_close(dali);
#endif
	
	return 0;
}
