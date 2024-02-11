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
#include <stdlib.h>
#include <errno.h>
#include "log.h"

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
};

DispatchPtr dispatch_new() {
	DispatchPtr ret = malloc(sizeof(struct Dispatch));
	if (ret) {
		ret->numentries = 0;
		ret->allocentries = 0;
		ret->entries = NULL;
		ret->fds = NULL;
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

DispatchStatus dispatch_run(DispatchPtr table, int timeout) {
	if (table) {
		int ready = poll(table->fds, table->numentries, timeout);
		if (ready == -1) {
			// error or signal
			if (errno != EINTR) {
				log_error("Error waiting for I/O events");
			}
			return 1;
		} else if (ready == 0) {
			// timeout
			log_debug("poll() timeout");
			return 1;
		} else {
			log_debug("poll() events waiting: %d", ready);
			size_t i;
			for (i = 0; i < table->numentries; i++) {
				if (table->fds[i].revents != 0) {
					log_debug("Events on fd %d: 0x%x", table->fds[i].fd, table->fds[i].revents);
				}
				// If any of these handlers remove a file descriptor,
				// the loop will be out of sync and events might be skipped
				// Not a big deal though, if everyting else works correctly
				if (table->fds[i].revents & POLLNVAL) {
					log_debug("Invalid fd %d", table->fds[i].fd);
					if (table->entries[i].errorfn) {
						table->entries[i].errorfn(table->entries[i].arg, DISPATCH_FD_INVALID);
					}
				} else if (table->fds[i].revents & POLLERR) {
					log_debug("Error event on fd %d", table->fds[i].fd);
					if (table->entries[i].errorfn) {
						table->entries[i].errorfn(table->entries[i].arg, DISPATCH_POLL_ERROR);
					}
				} else if (table->fds[i].revents & POLLHUP) {
					log_debug("Hangup event on fd %d", table->fds[i].fd);
					if (table->entries[i].errorfn) {
						table->entries[i].errorfn(table->entries[i].arg, DISPATCH_FD_CLOSED);
					}
				} else if (table->fds[i].revents & table->fds[i].events) {
					log_debug("I/O event on fd %d", table->fds[i].fd);
					if (table->entries[i].readyfn) {
						table->entries[i].readyfn(table->entries[i].arg);
					}
				}
			}
			return 2;
		}
	}
	return 0;
}

void dispatch_add(DispatchPtr table, int fd, short events, DispatchReadyFunc readyfn, DispatchErrorFunc errorfn, DispatchIndexFunc indexfn, void *arg) {
	if (table && fd >= 0) {
		log_debug("Adding %d to dispatch queue", fd);
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
			if (events == -1) {
				table->fds[index].events = POLLIN;
			} else {
				table->fds[index].events = events;
			}
			log_debug("Poll events for fd %d are: 0x%x", fd, table->fds[index].events);
			table->fds[index].revents = 0;
			if (table->entries[index].indexfn) {
				table->entries[index].indexfn(table->entries[index].arg, index);
			}
			table->numentries++;
		}
	}
}

void dispatch_remove_fd(DispatchPtr table, int fd) {
	if (table) {
		ssize_t i;
		for (i = 0; i < table->numentries; i++) {
			if (table->fds[i].fd == fd) {
				dispatch_remove(table, i);
				i--;
			}
		}
	}
}

void dispatch_remove(DispatchPtr table, size_t index) {
	// TODO: There's something wrong here
	if (table && index < table->numentries) {
		if (index != table->numentries - 1) {
			log_debug("Removing %d from dispatch queue", table->fds[index].fd);
			table->entries[index].arg = table->entries[table->numentries - 1].arg;
			table->entries[index].readyfn = table->entries[table->numentries - 1].readyfn;
			table->entries[index].errorfn = table->entries[table->numentries - 1].errorfn;
			table->entries[index].indexfn = table->entries[table->numentries - 1].indexfn;
			table->fds[index].fd = table->fds[table->numentries - 1].fd;
			table->fds[index].events = table->fds[table->numentries - 1].events;
			table->fds[index].revents = table->fds[table->numentries - 1].revents;
			if (table->entries[index].indexfn) {
				table->entries[index].indexfn(table->entries[index].arg, index);
			}
		}
		table->numentries--;
	}
}


