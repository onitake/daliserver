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
#include "ipc.h"

IpcPtr ipc_new() {
	IpcPtr ret = malloc(sizeof(struct Ipc));
	if (!ret) {
		fprintf(stderr, "Error allocating memory for socket pair: %s\n", strerror(errno));
		return NULL;
	}
	// SOCK_STREAM delivers data in the same chunks as they were sent, at least on Mach.
	// If this is not the case in your OS, SOCK_DGRAM must be used, but this has
	// the implication that sockets can't be closed. Termination must be signalled some other way.
	if (socketpair(PF_LOCAL, SOCK_STREAM, 0, ret->sockets) == -1) {
		fprintf(stderr, "Error creating socket pair: %s\n", strerror(errno));
		return NULL;
	}
	return ret;
}

void ipc_free(void *ipc) {
	IpcPtr ipcptr = (IpcPtr) ipc;
	if (ipcptr) {
		close(ipcptr->sockets[0]);
		close(ipcptr->sockets[1]);
		free(ipc);
	}
}
