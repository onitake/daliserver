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

#include "dispatch.h"
#include <stdlib.h>
#include <poll.h>

static const int DISPATCH_DEFAULT_TIMEOUT = -1;
static const size_t DISPATCH_ALLOC_INCREASE = 4;

struct DispatchEntry {
	void *arg;
	DispatchReadyFunc readyfn;
	DispatchErrorFunc errorfn;
	DispatchIndexFunc indexfn;
};

struct Dispatch {
	size_t numentries;
	size_t allocentries;
	struct DispatchEntry *entries;
	struct pollfd *fds;
	int timeout;
};

DispatchPtr dispatch_new() {
	DispatchPtr ret = malloc(sizeof(struct Dispatch));
	if (ret) {
		ret->numentries = 0;
		ret->allocentries = 0;
		ret->entries = NULL;
		ret->fds = NULL;
		ret->timeout = DISPATCH_DEFAULT_TIMEOUT;
	}
	return ret;
}

void dispatch_free(DispatchPtr table) {
	if (table) {
		free(table->entries);
		free(table->fds);
		free(table);
	}
}

void dispatch_run(DispatchPtr table) {
	if (table && table->numentries > 0) {
		int ready = poll(table->fds, table->numentries, table->timeout);
		if (ready == -1) {
			// error
		} else if (ready == 0) {
			// timeout
		} else {
			size_t i;
			for (i = 0; i < table->numentries; i++) {
				if (table->fds[i].revents & POLLIN) {
					table->entries[i].readyfn(table->entries[i].arg);
				}
				// check for other events
			}
		}
	}
}

void dispatch_set_timeout(DispatchPtr table, int timeout) {
	if (table) {
		table->timeout = timeout;
	}
}

void dispatch_add(DispatchPtr table, int fd, DispatchReadyFunc readyfn, DispatchErrorFunc errorfn, DispatchIndexFunc indexfn, void *arg) {
	if (table && readyfn && errorfn && indexfn && fd >= 0) {
		if (table->allocentries < table->numentries + 1) {
			table->allocentries += DISPATCH_ALLOC_INCREASE;
			table->entries = realloc(table->entries, sizeof(struct DispatchEntry) * table->allocentries);
			table->fds = realloc(table->fds, sizeof(struct pollfd) * table->allocentries);
		}
		if (table->entries && table->fds) {
			size_t index = table->numentries;
			table->entries[index].arg = arg;
			table->entries[index].readyfn = readyfn;
			table->entries[index].errorfn = errorfn;
			table->entries[index].indexfn = indexfn;
			table->fds[index].fd = fd;
			table->fds[index].events = POLLIN;
			table->fds[index].revents = 0;
			table->entries[index].indexfn(table->entries[index].arg, index);
			table->numentries++;
		}
	}
}

void dispatch_remove(DispatchPtr table, size_t index) {
	if (table && index < table->numentries) {
		if (index != table->numentries - 1) {
			table->entries[index].arg = table->entries[table->numentries - 1].arg;
			table->entries[index].readyfn = table->entries[table->numentries - 1].readyfn;
			table->entries[index].errorfn = table->entries[table->numentries - 1].errorfn;
			table->entries[index].indexfn = table->entries[table->numentries - 1].indexfn;
			table->fds[index].fd = table->fds[table->numentries - 1].fd;
			table->fds[index].events = table->fds[table->numentries - 1].events;
			table->fds[index].revents = table->fds[table->numentries - 1].revents;
			table->entries[index].indexfn(table->entries[index].arg, index);
		}
		table->numentries--;
	}
}


