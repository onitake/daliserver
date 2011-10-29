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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/socket.h>
#include "ipc.h"

struct Ipc {
	int sockets[2];
	DispatchPtr dispatch;
};

static void ipc_read_zero(void *arg);

IpcPtr ipc_new() {
	IpcPtr ret = malloc(sizeof(struct Ipc));
	if (!ret) {
		fprintf(stderr, "Error allocating memory for socket pair: %s\n", strerror(errno));
		return NULL;
	}
	ret->dispatch = NULL;
	// SOCK_STREAM delivers data in the same chunks as they were sent, at least on Mach.
	// If this is not the case in your OS, SOCK_DGRAM must be used, but this has
	// the implication that sockets can't be closed. Termination must be signalled some other way.
	if (socketpair(PF_LOCAL, SOCK_STREAM, 0, ret->sockets) == -1) {
		fprintf(stderr, "Error creating socket pair: %s\n", strerror(errno));
		return NULL;
	}
	return ret;
}

void ipc_free(IpcPtr ipc) {
	if (ipc) {
		if (ipc->dispatch) {
			dispatch_remove_fd(ipc->dispatch, ipc_read_socket(ipc));
		}
		close(ipc->sockets[0]);
		close(ipc->sockets[1]);
		free(ipc);
	}
}

void ipc_register(IpcPtr ipc, DispatchPtr dispatch) {
	if (ipc) {
		if (ipc->dispatch) {
			dispatch_remove_fd(ipc->dispatch, ipc_read_socket(ipc));
		}
		ipc->dispatch = dispatch;
		dispatch_add(ipc->dispatch, ipc_read_socket(ipc), POLLIN, ipc_read_zero, NULL, NULL, ipc);
	}
}

void ipc_notify(IpcPtr ipc) {
	if (ipc) {
		uint8_t dummy = 0;
		ssize_t wrbytes = write(ipc_write_socket(ipc), &dummy, sizeof(dummy));
	}
}

int ipc_read_socket(IpcPtr ipc) {
	if (ipc) {
		return ipc->sockets[0];
	}
	return -1;
}

int ipc_write_socket(IpcPtr ipc) {
	if (ipc) {
		return ipc->sockets[0];
	}
	return -1;
}

static void ipc_read_zero(void *arg) {
	IpcPtr ipc = (IpcPtr) arg;
	if (ipc) {
		uint8_t dummy = 0;
		ssize_t rdbytes = read(ipc_read_socket(ipc), &dummy, sizeof(dummy));
	}
}
