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
} DispatchError;

typedef void (*DispatchReadyFunc)(void *arg);
typedef void (*DispatchErrorFunc)(void *arg, DispatchError err);
typedef void (*DispatchIndexFunc)(void *arg, size_t index);

struct Dispatch;
typedef struct Dispatch *DispatchPtr;

DispatchPtr dispatch_new();
void dispatch_free(DispatchPtr table);
void dispatch_run(DispatchPtr table);
void dispatch_set_timeout(DispatchPtr table, int timeout);

void dispatch_add(DispatchPtr table, int fd, DispatchReadyFunc readyfn, DispatchErrorFunc errorfn, DispatchIndexFunc indexfn, void *arg);
void dispatch_remove(DispatchPtr table, size_t index);

#endif /*_DISPATCH_H*/

