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

#ifndef _IPC_H
#define _IPC_H

#include "dispatch.h"

struct Ipc;
typedef struct Ipc *IpcPtr;

// Create an unnamed socket pair for interprocess or interthread communication
IpcPtr ipc_new();
// Close both sockets and free all resources
void ipc_free(IpcPtr ipc);
// Registers the read socket on a dispatch queue, unregisters the previous registration
// A dummy reader callback is attached that will read one byte from the read socket
// This is intended to be used in conjunction with ipc_notify
void ipc_register(IpcPtr ipc, DispatchPtr dispatch);
// Simple notification mechanism, just write()s a 0 byte to the write socket
void ipc_notify(IpcPtr ipc);
// Returns the output end of the pipe
int ipc_read_socket(IpcPtr ipc);
// Returns the input end of the pipe
int ipc_write_socket(IpcPtr ipc);
// No data handling routines are provided, just use send(2), recv(2), close(2), poll(2), etc.

#endif /*_IPC_H*/
