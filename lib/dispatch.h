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

#ifndef _DISPATCH_H
#define _DISPATCH_H

#include <sys/types.h>

typedef enum {
	DISPATCH_FD_CLOSED = -1,
	DISPATCH_POLL_ERROR = -2,
	DISPATCH_FD_INVALID = -3,
} DispatchError;

// Callback function pointer to input handler routine
typedef void (*DispatchReadyFunc)(void *arg);
// Callback function pointer to error handler routine
typedef void (*DispatchErrorFunc)(void *arg, DispatchError err);
// Callback function pointer to update queue entry index
typedef void (*DispatchIndexFunc)(void *arg, size_t index);

struct Dispatch;
typedef struct Dispatch *DispatchPtr;

// Create a new dispatch queue
DispatchPtr dispatch_new();
// Destroy a dispatch queue
void dispatch_free(DispatchPtr table);
// Wait for I/O events on all the file descriptors in the dispatch queue
// Pass -1 for the timeout to wait forever
// Returns:
// 0 on error
// 1 if a timeout occured
// 2 if events were handled
// 3 if no events were waiting
int dispatch_run(DispatchPtr table, int timeout);

// Add a file descriptor to the dispatch queue
// fd must be a valid file descriptor
// events is a flag mask for poll(), if events is -1, POLLIN will be used
// Any or all of the function pointers may be NULL, in which case no action will be taken upon receiving an event
// arg is the first argument passed to the callbacks
void dispatch_add(DispatchPtr table, int fd, short events, DispatchReadyFunc readyfn, DispatchErrorFunc errorfn, DispatchIndexFunc indexfn, void *arg);
// Remove a file descriptor from the queue
// This may take linear time as the queue is search first
void dispatch_remove_fd(DispatchPtr table, int fd);
// Remove entry number 'index' from the queue
// Takes constant time
void dispatch_remove(DispatchPtr table, size_t index);

#endif /*_DISPATCH_H*/

