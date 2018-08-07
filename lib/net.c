/* Copyright (c) 2011, 2016, onitake <onitake@gmail.com>
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
#include "log.h"
#include "list.h"

struct Server {
	DispatchPtr dispatch;
	int listener;
	ListPtr connections;
	size_t framesize;
	ConnectionReceivedFunc recvfn;
	void *arg;
	ConnectionDestroyFunc conndestroy;
	void *conndestroyarg;
};

struct Connection {
	ServerPtr server;
	int socket;
	char *buffer;
	int waiting;
	ConnectionDestroyFunc destroy;
	void *destroyarg;
};

// Queue up 50 connections at most
const unsigned int MAX_CONNECTIONS = 50;

static void server_listener_ready(void *arg);
static void server_listener_error(void *arg, DispatchError err);
static void server_connection_remove(ServerPtr server,	ConnectionPtr conn);

static ConnectionPtr connection_new(ServerPtr server, int socket);
static void connection_free(ConnectionPtr conn);
static void connection_ready(void *arg);
static void connection_error(void *arg, DispatchError err);

ServerPtr server_open(DispatchPtr dispatch, const char *listenaddr, unsigned int port, size_t framesize, ConnectionReceivedFunc recvfn, void *arg) {
	ServerPtr server = malloc(sizeof(struct Server));
	if (server) {
		log_debug("Opening connection on %s:%u", listenaddr, port);
		server->listener = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (server->listener != -1) {
			if (setsockopt(server->listener, SOL_SOCKET, SO_REUSEADDR, & (int) {1}, sizeof(int)) == 0) {
				struct sockaddr_in all_if;
				memset(&all_if, 0, sizeof(all_if));
				all_if.sin_family = AF_INET;
				all_if.sin_port = htons((uint16_t) port);
				if (inet_pton(AF_INET, listenaddr, &all_if.sin_addr) == 1) {
					if (bind(server->listener, (struct sockaddr *) &all_if, sizeof(all_if)) == 0) {
						if (listen(server->listener, MAX_CONNECTIONS) == 0) {
							server->dispatch = dispatch;
							if (dispatch) {
								log_debug("Registering server socket %d", server->listener);
								dispatch_add(dispatch, server->listener, POLLIN, server_listener_ready,
											 server_listener_error, NULL, server);
							}
							server->connections = list_new((ListDataFreeFunc) connection_free);
							server->framesize = framesize;
							server->recvfn = recvfn;
							server->arg = arg;
							server->conndestroy = NULL;
							server->conndestroyarg = NULL;
							return server;
						} else {
							log_error("Error listening on socket: %s", strerror(errno));
						}
					} else {
						log_error("Error binding to socket: %s", strerror(errno));
					}
				} else {
					log_error("Error converting address: %s", strerror(errno));
				}
				close(server->listener);
			} else {
				log_error("Error setting SO_REUSEADDR: %s", strerror(errno));
			}
		} else {
			log_error("Error creating socket: %s", strerror(errno));
		}
		free(server);
	} else {
		log_error("Error allocating server object: %s", strerror(errno));
	}
	return NULL;
}

void server_close(ServerPtr server) {
	if (server) {
		log_info("Closing server %d", server->listener);
		dispatch_remove_fd(server->dispatch, server->listener);
		close(server->listener);
		list_free(server->connections);
		free(server);
	}
}

static void server_listener_ready(void *arg) {
	log_debug("Server %p got a connection", arg);
	ServerPtr server = (ServerPtr) arg;
	if (server) {
		struct sockaddr_in incoming;
		socklen_t incoming_size = sizeof(incoming);
		int socket = accept(server->listener, (struct sockaddr *) &incoming, &incoming_size);
		if (socket == -1) {
			log_error("Error accepting connection: %s", strerror(errno));
		} else {
			if (incoming.sin_family != AF_INET) {
				log_error("Invalid address family from incoming connection %d", incoming.sin_family);
			} else {
				char addr[16];
				log_info("Got connection from %s:%u", inet_ntop(incoming.sin_family, &incoming.sin_addr, addr, sizeof(addr)), incoming.sin_port);
				ConnectionPtr conn = connection_new(server, socket);
				list_enqueue(server->connections, conn);
			}
		}
	}
}

static void server_listener_error(void *arg, DispatchError err) {
	switch (err) {
	case DISPATCH_FD_CLOSED:
		log_info("Server %p was disconnected", arg);
		break;
	default:
		log_error("Server %p got a connection error: %d", arg, err);
		break;
	}
}

static void server_connection_remove(ServerPtr server, ConnectionPtr conn) {
	if (server && conn) {
		ListNodePtr node = list_find(server->connections, list_equal, conn);
		list_remove(server->connections, node);
		connection_free(conn);
	}
}

static ConnectionPtr connection_new(ServerPtr server, int socket) {
	if (server) {
		ConnectionPtr conn = malloc(sizeof(struct Connection));
		if (conn) {
			conn->server = server;
			conn->socket = socket;
			conn->buffer = malloc(server->framesize);
			conn->waiting = 0;
			conn->destroy = server->conndestroy;
			conn->destroyarg = server->conndestroyarg;
			if (server->dispatch) {
				dispatch_add(server->dispatch, socket, -1, connection_ready, connection_error, NULL, conn);
			}
		}
		return conn;
	}
	return NULL;
}

static void connection_free(ConnectionPtr conn) {
	if (conn) {
		log_info("Closing connection %d", conn->socket);
		if (conn->destroy) {
			log_debug("Calling destroy callback before closing connection");
			conn->destroy(conn->destroyarg, conn);
		}
		close(conn->socket);
		if (conn->server && conn->server->dispatch) {
			dispatch_remove_fd(conn->server->dispatch, conn->socket);
		}
		free(conn->buffer);
		free(conn);
	}
}

static void connection_ready(void *arg) {
	ConnectionPtr conn = (ConnectionPtr) arg;
	if (conn) {
		log_debug("Connection %d has data available", conn->socket);
		ssize_t rdbytes = read(conn->socket, conn->buffer, conn->server->framesize);
		if (rdbytes == -1) {
			if (errno != ECONNRESET) {
				log_error("Error reading %ld bytes from %d: %s", conn->server->framesize, conn->socket, strerror(errno));
			} else {
				log_info("Connection %d closed, exiting handler", conn->socket);
			}
			server_connection_remove(conn->server, conn);
		} else if (rdbytes < conn->server->framesize) {
			log_info("Short read on connection %d, only got %ld bytes", conn->socket, rdbytes);
			server_connection_remove(conn->server, conn);
		} else {
			log_debug("Got packet(%ld bytes)", rdbytes);
			if (conn->server->recvfn) {
				conn->waiting = 1;
				conn->server->recvfn(conn->server->arg, conn->buffer, rdbytes, conn);
			}
		}
	}
}

static void connection_error(void *arg, DispatchError err) {
	ConnectionPtr conn = (ConnectionPtr) arg;
	if (conn) {
		switch (err) {
		case DISPATCH_FD_CLOSED:
			log_info("Connection %d was disconnected", conn->socket);
			server_connection_remove(conn->server, conn);
			break;
		case DISPATCH_FD_INVALID:
			log_info("Socket %d is invalid", conn->socket);
			server_connection_remove(conn->server, conn);
			break;
		case DISPATCH_POLL_ERROR:
			log_info("Output error on %d", conn->socket);
			server_connection_remove(conn->server, conn);
			break;
		default:
			log_error("Connection %d got a connection error: %d", conn->socket, err);
			break;
		}
	}
}

void connection_reply(ConnectionPtr conn, const char *buffer, size_t bufsize) {
	if (conn) {
		conn->waiting = 0;
		if (buffer && bufsize > 0) {
			log_debug("Sending reply on connection %d", conn->socket);
			ssize_t wrbytes = write(conn->socket, buffer, bufsize);
			if (wrbytes == -1) {
				if (errno != ECONNRESET) {
					log_error("Error writing %ld bytes to connection %d: %s", bufsize, conn->socket, strerror(errno));
				} else {
					log_info("Connection %d closed, exiting handler", conn->socket);
				}
				server_connection_remove(conn->server, conn);
			} else if (wrbytes < conn->server->framesize){
				log_info("Short write on connection %d, only wrote %ld bytes", conn->socket, wrbytes);
				server_connection_remove(conn->server, conn);
			} else {
				log_debug("Sent packet(%ld bytes)", wrbytes);
			}
		}
	}
}

void server_broadcast(ServerPtr server, const char *buffer, size_t bufsize) {
	if (server && buffer && bufsize > 0) {
		ListNodePtr node;
		for (node = list_first(server->connections); node; node = list_next(node)) {
			ConnectionPtr conn = list_data(node);
			if (!conn->waiting) {
				connection_reply(conn, buffer, bufsize);
			}
		}
	}
}

void server_set_connection_destroy_callback(ServerPtr server, ConnectionDestroyFunc destroy, void *arg) {
	if (server) {
		server->conndestroy = destroy;
		server->conndestroyarg = arg;
	}
}
