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

#ifndef _NET_H
#define _NET_H

#include <stddef.h>
#include "dispatch.h"

struct Server;
typedef struct Server *ServerPtr;
struct Connection;
typedef struct Connection *ConnectionPtr;

typedef void (*ConnectionReceivedFunc)(void *arg, const char *buffer, size_t bufsize, ConnectionPtr conn);
typedef void (*ConnectionDestroyFunc)(void *arg, ConnectionPtr conn);

// Creates a new server that listens on the specified address and port
// Use 0.0.0.0 to listen on all interfaces
ServerPtr server_open(DispatchPtr dispatch, const char *listenaddr, unsigned int port, size_t framesize, ConnectionReceivedFunc recvfn, void *arg);
// Shuts the server down and closes all connections
void server_close(ServerPtr server);
// Sends a message to all connections not waiting for a reply
void server_broadcast(ServerPtr server, const char *buffer, size_t bufsize);
// Assigns a handler to be called before a connection object is destroyed
void server_set_connection_destroy_callback(ServerPtr conn, ConnectionDestroyFunc destroy, void *arg);

// Sends a reply
void connection_reply(ConnectionPtr conn, const char *buffer, size_t bufsize);

#endif //_NET_H
