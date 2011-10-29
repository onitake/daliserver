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

#include "net.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct Server {
	DispatchPtr dispatch;
	int listener;
};

// Queue up 50 connections at most
const unsigned int MAX_CONNECTIONS = 50;

static void server_listener_ready(void *arg);
static void server_listener_error(void *arg, DispatchError err);

ServerPtr server_open(DispatchPtr dispatch, const char *listenaddr, unsigned int port) {
	ServerPtr server = malloc(sizeof(struct Server));
	if (server) {
		server->listener = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (server->listener != -1) {
			struct sockaddr_in all_if;
			memset(&all_if, 0, sizeof(all_if));
			all_if.sin_family = AF_INET;
			all_if.sin_port = htons((uint16_t) port);
			if (inet_pton(AF_INET, listenaddr, &all_if.sin_addr) == 1) {
				if (bind(server->listener, (struct sockaddr *) &all_if, sizeof(all_if)) == 0) {
					if (listen(server->listener, MAX_CONNECTIONS) == 0) {
						server->dispatch = dispatch;
						if (dispatch) {
							dispatch_add(dispatch, server->listener, POLLIN, server_listener_ready, server_listener_error, NULL, server);
						}
						return server;
					} else {
						fprintf(stderr, "Error listening on socket: %s\n", strerror(errno));
					}
				} else {
					fprintf(stderr, "Error binding socket: %s\n", strerror(errno));
				}
			} else {
				fprintf(stderr, "Error converting address: %s\n", strerror(errno));
			}
			close(server->listener);
		} else {
			fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
		}
		free(server);
	} else {
		fprintf(stderr, "Error allocating server object: %s\n", strerror(errno));
	}
	return NULL;
}

void server_close(ServerPtr server) {
	if (server) {
		dispatch_remove_fd(server->dispatch, server->listener);
		close(server->listener);
		free(server);
	}
}

static void server_listener_ready(void *arg) {
	printf("Server %p got a connection\n", arg);
}

static void server_listener_error(void *arg, DispatchError err) {
	printf("Server %p got a connection error: %d\n", arg, err);
}
