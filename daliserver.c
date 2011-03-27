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
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "list.h"
#include "util.h"
#include "usb.h"
#include "ipc.h"

// Network protocol:
// BusMessage {
//     address:uint8_t
//     command:uint8_t
// }
// Request {
//     address:uint8_t
//     command:uint8_t
// }
// Response {
//     status:uint8_t
//     0:uint8_t
// }

// IPC protocol:
// NetToUsb {
//     type:uint8_t [0 = command]
//     command {
//         address:uint8_t
//         command:uint8_t
//         0:uint8_t
//         ident:uint32_t
//     }
// }
// UsbToNet {
//     type:uint8_t [1 = outofband, 2 = response, 3 = error]
//     outofband {
//         address:uint8_t
//         command:uint8_t
//         0:uint8_t
//         0:uint32_t
//     }
//     response {
//         status:uint8_t
//         0:uint8_t
//         0:uint8_t
//         0:uint32_t
//     }
//     error {
//         code:uint8_t [0 = unknown, 1 = usb, 2 = format, 3 = internal, 4 = queuefull]
//         0:uint8_t
//         0:uint8_t
//         0:uint32_t
//     }
// }

enum {
	IPC_COMMAND = 0,
	IPC_OUTBAND = 1,
	IPC_RESPONSE = 2,
	IPC_ERROR = 3,
};
enum {
	IPC_EUNKNOWN = 0,
	IPC_EUSB = 1,
	IPC_EFORMAT = 2,
	IPC_EINTERNAL = 3,
	IPC_EQUEUE = 4,
};

const struct DaliFrame ALL_OFF = { 0xff, 0x00, };
const struct DaliFrame ALL_ON = { 0xff, 0x08, };
const struct DaliFrame ALL_DIM = { 0xfe, 0x60, };

// Dequeue this many commands before going back to handling USB events max.
const unsigned int MAX_DEQUEUE = 10;
// Handle this many connections max.
const unsigned int MAX_CONNECTIONS = 50;
// Wait this many msecs for each poll to complete
const int WAIT_POLL = 100;
// Wait this many usecs if no connection handler is available
const useconds_t WAIT_CONGEST = 100000;
// Listen on this port
const unsigned short NET_PORT = 55825;
// Bind to this address
const char *NET_ADDRESS = "0.0.0.0";

struct ThreadReturn {
	int error;
};
typedef struct ThreadReturn *ThreadReturnPtr;
ThreadReturnPtr thread_return_new(int error);
void thread_return_free(void *ret);

struct Server {
	ListPtr connections;
	IpcPtr ipc;
	pthread_t thread;
};
typedef struct Server *ServerPtr;
ServerPtr server_new();
void server_free(void *arg);
void *server_loop(void *arg);
int server_start(ServerPtr server);

struct Connection {
	int socket;
	// TODO: uint8_t ought to be enough - we only allow 50 connections anyway.
	uint32_t ident;
	IpcPtr ipc;
	ServerPtr server;
	int waiting;
	pthread_t thread;
};
typedef struct Connection *ConnectionPtr;
ConnectionPtr connection_new(ServerPtr server, int socket, uint32_t ident);
void connection_free(void *arg);
int connection_is_unused(void *data, void *arg);
int connection_has_id(void *data, void *arg);
void *connection_loop(void *arg);
int connection_start(ConnectionPtr conn);

struct Usb {
	ServerPtr server;
	UsbDaliPtr dali;
	pthread_t thread;
};
typedef struct Usb *UsbPtr;
UsbPtr usb_new(ServerPtr server);
void usb_free(void *arg);
void *usb_loop(void *arg);

void dali_outband_callback(UsbDaliError err, DaliFramePtr frame, unsigned int response, void *arg);
void dali_inband_callback(UsbDaliError err, DaliFramePtr frame, unsigned int response, void *arg);

static int running;

ThreadReturnPtr thread_return_new(int error) {
	ThreadReturnPtr ret = malloc(sizeof(struct ThreadReturn));
	if (ret) {
		ret->error = error;
	}
	return ret;
}

void thread_return_free(void *ret) {
	free(ret);
}

ServerPtr server_new() {
	ServerPtr ret = malloc(sizeof(struct Server));
	if (ret) {
		ret->connections = list_new(connection_free);
		memset(&ret->thread, 0, sizeof(ret->thread));
		ret->ipc = ipc_new();
	}
	return ret;
}

void server_free(void *arg) {
	ServerPtr server = (ServerPtr) arg;
	if (server) {
		list_free(server->connections);
		ipc_free(server->ipc);
		free(arg);
	}
}

int server_start(ServerPtr server) {
	if (server) {
		return pthread_create(&server->thread, NULL, server_loop, server);
	}
	return -1;
}

ConnectionPtr connection_new(ServerPtr server, int socket, uint32_t ident) {
	ConnectionPtr ret = malloc(sizeof(struct Connection));
	if (ret) {
		ret->socket = socket;
		memset(&ret->thread, 0, sizeof(ret->thread));
		ret->ident = ident;
		ret->ipc = ipc_new();
		ret->server = server;
		ret->waiting = 0;
	}
	return ret;
}

void connection_free(void *arg) {
	ConnectionPtr conn = (ConnectionPtr) arg;
	if (conn) {
		if (conn->socket != -1) {
			close(conn->socket);
		}
		ipc_free(conn->ipc);
		free(arg);
	}
}

int connection_is_unused(void *data, void *arg) {
	if (data && ((ConnectionPtr) data)->socket == -1) {
		return 1;
	}
	return 0;
}

int connection_has_id(void *data, void *arg) {
	if (data && arg) {
		ConnectionPtr carg = (ConnectionPtr) data;
		uint32_t *ident = (uint32_t *) arg;
		if (carg->socket != -1 && carg->ident == *ident) {
			return 1;
		}
	}
	return 0;
}

int connection_start(ConnectionPtr conn) {
	if (conn) {
		return pthread_create(&conn->thread, NULL, connection_loop, conn);
	}
	return -1;
}

UsbPtr usb_new(ServerPtr server) {
	UsbPtr ret = malloc(sizeof(struct Usb));
	if (ret) {
		memset(&ret->thread, 0, sizeof(ret->thread));
		ret->server = server;
#ifndef USB_OFF
		ret->dali = usbdali_open(NULL, dali_outband_callback, ret);
		if (ret->dali) {
			usbdali_set_handler_timeout(ret->dali, 0);
			usbdali_set_debug(ret->dali, 1);
		}
#else
		ret->dali = NULL;
#endif
	}
	return ret;
}

void usb_free(void *arg) {
	UsbPtr usb = (UsbPtr) arg;
	if (usb) {
#ifndef USB_OFF
		if (usb->dali) {
			usbdali_close(usb->dali);
		}
#endif
		free(arg);
	}
}

int usb_start(UsbPtr usb) {
	if (usb) {
		return pthread_create(&usb->thread, NULL, usb_loop, usb);
	}
	return 0;
}

void *connection_loop(void *arg) {
	printf("Entering handler thread\n");

	int ret = 0;
	
	ConnectionPtr conn = (ConnectionPtr) arg;
	if (conn) {
		int connected = 1;
		while (running && connected) {
			struct pollfd fds[2];
			fds[0].fd = conn->socket;
			fds[0].events = POLLIN;
			fds[0].revents = 0;
			fds[1].fd = conn->ipc->sockets[0];
			fds[1].events = POLLIN;
			fds[1].revents = 0;
			int rdy_fds = poll(fds, 2, WAIT_POLL);
			if (rdy_fds == -1) {
				fprintf(stderr, "Error waiting for data: %s\n", strerror(errno));
				connected = 0;
				ret = -1;
			} else if (rdy_fds > 0) {
				if (fds[1].revents & POLLIN) {
					char buffer[8];
					ssize_t rd = read(conn->ipc->sockets[0], buffer, sizeof(buffer));
					if (rd == -1) {
						if (errno != ECONNRESET) {
							fprintf(stderr, "Error reading from IPC: %s\n", strerror(errno));
							ret = -1;
						} else {
							printf("IPC closed, exiting handler\n");
						}
						connected = 0;
					} else if (rd == 8) {
						switch (buffer[0]) {
						case IPC_OUTBAND:
							if (!conn->waiting) {
								char buffer2[2];
								buffer2[0] = buffer[1];
								buffer2[1] = buffer[2];
								ssize_t wr = write(conn->socket, buffer2, sizeof(buffer2));
								if (wr == -1) {
									if (errno != ECONNRESET) {
										fprintf(stderr, "Error writing to socket: %s\n", strerror(errno));
										ret = -1;
									} else {
										printf("Socket closed, exiting handler\n");
									}
									connected = 0;
								}
							}
							break;
						case IPC_RESPONSE:
							if (conn->waiting) {
								char buffer2[2];
								buffer2[0] = buffer[1];
								buffer2[1] = 0;
								ssize_t wr = write(conn->socket, buffer2, sizeof(buffer2));
								if (wr == -1) {
									if (errno != ECONNRESET) {
										fprintf(stderr, "Error writing to socket: %s\n", strerror(errno));
										ret = -1;
									} else {
										printf("Socket closed, exiting handler\n");
									}
									connected = 0;
								}
								conn->waiting = 0;
							}
							break;
						case IPC_ERROR:
							printf("Error handling request: %d\n", buffer[1]);
							conn->waiting = 0;
							break;
						}
					} else {
						fprintf(stderr, "Short read on IPC, only got %ld bytes\n", rd);
						connected = 0;
					}
				}
				if (fds[0].revents & POLLIN) {
					char buffer[2];
					ssize_t rd = read(conn->socket, buffer, sizeof(buffer));
					if (rd == -1) {
						if (errno != ECONNRESET) {
							fprintf(stderr, "Error reading from socket: %s\n", strerror(errno));
							ret = -1;
						} else {
							printf("Connection closed, exiting handler\n");
						}
						connected = 0;
					} else if (rd == 2) {
						printf("Got packet(0x%02x, 0x%02x)\n", (uint8_t) buffer[0], (uint8_t) buffer[1]);
						if (!conn->waiting) {
							char buffer2[8];
							buffer2[0] = IPC_COMMAND;
							buffer2[1] = buffer[0];
							buffer2[2] = buffer[1];
							buffer2[3] = 0;
							buffer2[4] = conn->ident & 0xff;
							buffer2[5] = (conn->ident >> 8) & 0xff;
							buffer2[6] = (conn->ident >> 16) & 0xff;
							buffer2[7] = (conn->ident >> 24) & 0xff;
							conn->waiting = 1;
							ssize_t wr = write(conn->server->ipc->sockets[1], buffer2, sizeof(buffer2));
						} else {
							printf("Command already queued, ignoring\n");
						}
					} else {
						fprintf(stderr, "Short read on socket, only got %ld bytes\n", rd);
						connected = 0;
					}
				}
				if ((fds[1].revents & POLLHUP) || (fds[1].revents & POLLERR)) {
					printf("IPC closed\n");
					connected = 0;
				}
				if ((fds[0].revents & POLLHUP) || (fds[0].revents & POLLERR)) {
					printf("Connection closed\n");
					connected = 0;
				}
			}
		}
		
		close(conn->socket);
		conn->socket = -1;
	} else {
		fprintf(stderr, "Invalid handler thread argument\n");
		ret = -1;
	}

	printf("Exiting handler thread\n");
	return thread_return_new(ret);
}

void *server_loop(void *arg) {
	printf("Entering server thread\n");

	int ret = 0;
	uint32_t ident = 0;
	
	ServerPtr server = (ServerPtr) arg;
	if (server) {
		int listener = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listener != -1) {
			struct sockaddr_in all_if;
			memset(&all_if, 0, sizeof(all_if));
			all_if.sin_family = AF_INET;
			all_if.sin_port = htons(NET_PORT);
			if (inet_pton(AF_INET, NET_ADDRESS, &all_if.sin_addr) == 1) {
				if (bind(listener, (struct sockaddr *) &all_if, sizeof(all_if)) == 0) {
					if (listen(listener, MAX_CONNECTIONS) == 0) {
						int listening = 1;
						while (running && listening) {
							struct pollfd fds[1];
							fds[0].fd = listener;
							fds[0].events = POLLIN;
							fds[0].revents = 0;
							int rdy_fds = poll(fds, 1, WAIT_POLL);
							if (rdy_fds == -1) {
								fprintf(stderr, "Error waiting for incoming connections: %s\n", strerror(errno));
								ret = -1;
								listening = 0;
							} else if (rdy_fds == 1) {
								if ((fds[0].revents & POLLERR) || (fds[0].revents & POLLHUP)) {
									fprintf(stderr, "Listener closed\n");
									ret = -1;
									listening = 0;
								} else if (fds[0].revents & POLLIN) {
									ListNodePtr closed;
									do {
										closed = list_find(server->connections, connection_is_unused, NULL);
										if (closed) {
											printf("Removing stale handler thread %d\n", ((ConnectionPtr) list_data(closed))->ident);
											connection_free(list_remove(server->connections, closed));
										}
									} while (closed);
									if (list_length(server->connections) < MAX_CONNECTIONS) {
										struct sockaddr_in incoming;
										socklen_t incoming_size = sizeof(incoming);
										int socket = accept(listener, (struct sockaddr *) &incoming, &incoming_size);
										if (socket == -1) {
											fprintf(stderr, "Error accepting connection: %s\n", strerror(errno));
											ret = -1;
											listening = 0;
										} else {
											if (incoming.sin_family != AF_INET) {
												fprintf(stderr, "Invalid address family from incoming connection %d\n", incoming.sin_family);
											} else {
												// TODO: Check for ident availability before use
												char addr[16];
												printf("Got connection from %s:%u, creating handler %d\n", inet_ntop(incoming.sin_family, &incoming.sin_addr, addr, sizeof(addr)), incoming.sin_port, ident);
												ConnectionPtr conn = connection_new(server, socket, ident++);
												ListNodePtr node = list_enqueue(server->connections, conn);
												if (connection_start(conn) == -1) {
													fprintf(stderr, "Error creating connection thread: %s\n", strerror(errno));
													connection_free(list_remove(server->connections, node));
												}
											}
										}
									} else {
										usleep(WAIT_CONGEST);
									}
								}
							}
						}
					} else {
						fprintf(stderr, "Error listening on socket: %s\n", strerror(errno));
						ret = -1;
					}
				} else {
					fprintf(stderr, "Error binding socket: %s\n", strerror(errno));
					ret = -1;
				}
			} else {
				fprintf(stderr, "Error converting address: %s\n", strerror(errno));
				ret = -1;
			}
			close(listener);
		} else {
			fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
			ret = -1;
		}
	} else {
		fprintf(stderr, "Invalid server thread argument\n");
		ret = -1;
	}

	printf("Waiting for connections to close down\n");
	running = 0;
	ConnectionPtr conn;
	do {
		conn = list_dequeue(server->connections);
		if (conn) {
			ThreadReturnPtr tret = NULL;
			if (pthread_join(conn->thread, &tret) == -1) {
				fprintf(stderr, "Error joining connection %d: %s\n", conn->ident, strerror(errno));
			}
			connection_free(conn);
		}
	} while (conn);
	
	printf("Exiting server thread\n");
	return thread_return_new(ret);
}

void dali_outband_callback(UsbDaliError err, DaliFramePtr frame, unsigned int response, void *arg) {
	UsbPtr usb = (UsbPtr) arg;
	if (usb) {
		char buffer[8];
		switch (err) {
		case USBDALI_SUCCESS:
			buffer[0] = IPC_OUTBAND;
			buffer[1] = frame->address;
			buffer[2] = frame->command;
			break;
		case USBDALI_SEND_TIMEOUT:
		case USBDALI_RECEIVE_TIMEOUT:
		case USBDALI_SEND_ERROR:
		case USBDALI_RECEIVE_ERROR:
			buffer[0] = IPC_ERROR;
			buffer[1] = IPC_EUSB;
			buffer[2] = 0;
			break;
        case USBDALI_QUEUE_FULL:
			buffer[0] = IPC_ERROR;
			buffer[1] = IPC_EQUEUE;
			buffer[2] = 0;
			break;
		case USBDALI_INVALID_ARG:
			buffer[0] = IPC_ERROR;
			buffer[1] = IPC_EFORMAT;
			buffer[2] = 0;
			break;
		default:
			buffer[0] = IPC_ERROR;
			buffer[1] = IPC_EINTERNAL;
			buffer[2] = 0;
			break;
		}
		buffer[3] = buffer[4] = buffer[5] = buffer[6] = buffer[7] = 0;
		list_lock(usb->server->connections);
		ListNodePtr node;
		for (node = list_first(usb->server->connections); node; node = list_next(node)) {
			ConnectionPtr conn = list_data(node);
			int wr = write(conn->ipc->sockets[1], buffer, sizeof(buffer));
			if (wr == -1) {
				fprintf(stderr, "Error writing to IPC: %s\n", strerror(errno));
			}
		}
		list_unlock(usb->server->connections);
	}
}

void dali_inband_callback(UsbDaliError err, DaliFramePtr frame, unsigned int response, void *arg) {
	ConnectionPtr conn = (ConnectionPtr) arg;
	if (conn) {
		// TODO: Check if the connection identifier matches
		// We might need an intermediate object for that...
		char buffer[8];
		switch (err) {
		case USBDALI_SUCCESS:
			buffer[0] = IPC_RESPONSE;
			buffer[1] = (uint8_t) response;
			buffer[2] = 0;
			break;
		case USBDALI_SEND_TIMEOUT:
		case USBDALI_RECEIVE_TIMEOUT:
		case USBDALI_SEND_ERROR:
		case USBDALI_RECEIVE_ERROR:
			buffer[0] = IPC_ERROR;
			buffer[1] = IPC_EUSB;
			buffer[2] = 0;
			break;
        case USBDALI_QUEUE_FULL:
			buffer[0] = IPC_ERROR;
			buffer[1] = IPC_EQUEUE;
			buffer[2] = 0;
			break;
		case USBDALI_INVALID_ARG:
			buffer[0] = IPC_ERROR;
			buffer[1] = IPC_EFORMAT;
			buffer[2] = 0;
			break;
		default:
			buffer[0] = IPC_ERROR;
			buffer[1] = IPC_EINTERNAL;
			buffer[2] = 0;
			break;
		}
		buffer[3] = buffer[4] = buffer[5] = buffer[6] = buffer[7] = 0;
		int wr = write(conn->ipc->sockets[1], buffer, sizeof(buffer));
		if (wr == -1) {
			fprintf(stderr, "Error writing to IPC: %s\n", strerror(errno));
		}
	}
}

void *usb_loop(void *arg) {
	printf("Entering USB thread\n");
	
	int ret = 0;
	
	UsbPtr usb = (UsbPtr) arg;
	if (usb) {
		struct pollfd *fds = NULL;
		size_t nfds = 0;
		if (usb->dali) {
			UsbDaliError err = usbdali_pollfds(usb->dali, 1, &fds, &nfds);
			if (err != USBDALI_SUCCESS) {
				fprintf(stderr, "Error getting USB polling descriptors: %s\n", usbdali_error_string(err));
				ret = -1;
				running = 0;
			}
		} else {
			fds = malloc(sizeof(struct pollfd));
			nfds = 1;
		}
		if (fds) {
			fds[0].fd = usb->server->ipc->sockets[0];
			fds[0].events = POLLIN;
			fds[0].revents = 0;
		}
		while (running) {
			// This just returns WAIT_POLL if usb->dali is NULL
			int timeout = usbdali_next_timeout(usb->dali, WAIT_POLL);
			int rdy_fds = poll(fds, nfds, timeout);
			if (rdy_fds == -1) {
				fprintf(stderr, "Error waiting for USB data or IPC: %s\n", strerror(errno));
				ret = -1;
				running = 0;
			} else if (rdy_fds > 0) {
				if ((fds[0].revents & POLLERR) || (fds[0].revents & POLLHUP)) {
					fprintf(stderr, "IPC closed\n");
					running = 0;
				} else if (fds[0].revents & POLLIN) {
					char buffer[8];
					ssize_t rd = read(usb->server->ipc->sockets[0], buffer, sizeof(buffer));
					if (rd == -1) {
						if (errno != ECONNRESET) {
							fprintf(stderr, "Error reading from IPC: %s\n", strerror(errno));
							ret = -1;
						} else {
							printf("IPC closed, exiting USB thread\n");
						}
						running = 0;
					}
					if (rd == 8) {
						switch (buffer[0]) {
						case IPC_COMMAND:
							printf("USB out address=0x%02x command=0x%02x\n", (uint8_t) buffer[1], (uint8_t) buffer[2]);
							if (usb->dali) {
								uint32_t ident = buffer[4] | (buffer[5] << 8) | (buffer[6] << 16) | (buffer[7] << 24);
								DaliFramePtr frame = daliframe_new(buffer[1], buffer[2]);
								// TODO: Do this in dali_inband_callback instead
								ListNodePtr node = list_find(usb->server->connections, connection_has_id, &ident);
								ConnectionPtr conn = NULL;
								if (node) {
									conn = list_data(node);
								} else {
									printf("Connection %d is gone, ignoring response\n", ident);
								}
								UsbDaliError err = usbdali_queue(usb->dali, frame, dali_inband_callback, conn);
								if (err != USBDALI_SUCCESS) {
									fprintf(stderr, "Error sending command through USB: %s\n", usbdali_error_string(err));
									ret = -1;
									running = 0;
								}
							}
							break;
						default:
							fprintf(stderr, "Invalid request: %d\n", buffer[0]);
							break;
						}
					}
				}
			}
			// Always handle libusb events, no harm done if there's nothing in the queue
			if (usb->dali) {
				UsbDaliError err = usbdali_handle(usb->dali);
				if (err != USBDALI_SUCCESS) {
					fprintf(stderr, "Error handling USB events: %s\n", usbdali_error_string(err));
					ret = -1;
					running = 0;
				}
			}
		}
	}
	
	printf("Exiting USB thread\n");
	return thread_return_new(ret);
}

void signal_handler(int sig) {
	if (running) {
		fprintf(stderr, "Signal received, shutting down\n");
		running = 0;
	} else {
		fprintf(stderr, "Another signal received, killing process\n");
		kill(getpid(), SIGKILL);
	}
}

int main(int argc, const char **argv) {
	printf("Starting up\n");
	ServerPtr server = server_new();
	UsbPtr usb = usb_new(server);
	
	running = 1;
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	
	printf("Starting server thread\n");
	if (server_start(server) == -1) {
		fprintf(stderr, "Error starting server thread: %s\n", strerror(errno));
		running = 0;
	}

	printf("Starting USB thread\n");
#ifdef USB_THREAD
	if (usb_start(usb) == -1) {
		fprintf(stderr, "Error starting USB thread: %s\n", strerror(errno));
		running = 0;
	}

	ThreadReturnPtr usb_ret = NULL;
	pthread_join(usb->thread, &usb_ret);
#else
	ThreadReturnPtr usb_ret = usb_loop(usb);
#endif
	if (usb_ret) {
		printf("USB thread returned: %d\n", usb_ret->error);
		free(usb_ret);
	}
	
	ThreadReturnPtr server_ret = NULL;
	pthread_join(server->thread, &server_ret);
	if (server_ret) {
		printf("Server thread returned: %d\n", server_ret->error);
		free(server_ret);
	}
	
	printf("Freeing resources\n");
	usb_free(usb);
	server_free(server);
	
	return 0;
}
