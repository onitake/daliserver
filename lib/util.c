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

#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

void hexdump(const uint8_t *data, size_t length) {
	size_t line;
	for (line = 0; line < (length + 15) / 16; line++) {
		printf("0x%08lx ", line * 16);
		size_t column;
		for (column = 0; column < 16 && line * 16 + column < length; column++) {
			printf("%02x ", data[line * 16 + column]);
		}
		for (column = 0; column < 16 && line * 16 + column < length; column++) {
			uint8_t character = data[line * 16 + column];
			/* Assume ASCII compatible charset */
			if (character >= 0x20 && character <= 0x7e) {
				printf("%c", character);
			} else {
				printf(".");
			}
		}
		printf("\n");
	}
}

void daemonize(const char *pidfile) {
	// Check if we are a child of init
	if (getppid() == 1) {
		// Yes, no need to daemonize
		return;
	}

	// Prepare notification channel
	int notefd[2];
	if (pipe(notefd) < 0) {
		exit(1);
	}
	
	// First fork
	pid_t pid = fork();
	// Did it succeed?
	if (pid < 0) {
		// No, exit
		exit(1);
	}
	// Are we the parent?
	if (pid > 0) {
		// Yes, wait for the daemon to signal us
		char retval = 0;
		if (read(notefd[0], &retval, 1) != 1) {
			exit(1);
		}
		exit(retval);
	}
	
	// Prepare for the second fork
	close(notefd[0]);

	// Change the file mode mask
	umask(0);

	// Create a new session ID for the daemon process
	pid_t sid = setsid();
	if (sid < 0) {
		// Propagate the error to the main process
		char retval = 2;
		if (write(notefd[1], &retval, 1) != 1) { /* ignore */ }
		exit(1);
	}

	// Change to the directory root to prevent mount point locking
	if (chdir("/") < 0) {
		// Propagate the error to the main process
		char retval = 2;
		if (write(notefd[1], &retval, 1) != 1) { /* ignore */ }
		exit(1);
	}

	// Drop standard I/O descriptors
	if (!freopen("/dev/null", "r", stdin) || !freopen("/dev/null", "w", stdout) || !freopen("/dev/null", "w", stderr)) {
		// Propagate the error to the main process
		char retval = 2;
		if (write(notefd[1], &retval, 1) != 1) { /* ignore */ }
		exit(1);
	}
	
	// Fork again to cut all ties to the controlling terminal
	pid_t dpid = fork();
	if (dpid < 0) {
		// Propagate the error to the main process
		char retval = 2;
		if (write(notefd[1], &retval, 1) != 1) { /* ignore */ }
		exit(1);
	}
	if (dpid > 0) {
		// The fork has succeeded, we are not needed any more
		exit(0);
	}

	// Record the daemon's PID
	if (pidfile) {
		int pidfd = open(pidfile, O_WRONLY | O_CREAT, 0640);
		if (pidfd < 0) {
			// Propagate the error to the main process
			char retval = 2;
			if (write(notefd[1], &retval, 1) != 1) { /* ignore */ }
			exit(1);
		}
		if (lockf(pidfd, F_TLOCK, 0) < 0) {
			// Locking has failed - another daemon is running
			// Propagate the error to the main process
			char retval = 2;
			if (write(notefd[1], &retval, 1) != 1) { /* ignore */ }
			exit(1);
		}
		// Store the PID
		FILE *pidfp = fdopen(pidfd, "w");
		if (!pidfp) {
			// Propagate the error to the main process
			char retval = 2;
			if (write(notefd[1], &retval, 1) != 1) { /* ignore */ }
			exit(1);
		}
		fprintf(pidfp, "%d\n", getpid());
		fflush(pidfp);
		// And keep the file open and locked
	}

	// Signal the main process that daemonization has finished
	char retval = 0;
	if (write(notefd[1], &retval, 1) != 1) {
		// Nasty business. If we get here, try to do the best we can to avoid
		// blocking the master indefinitely.
		close(notefd[1]);
		exit(1);
	}
	// And close the notification channel
	close(notefd[1]);
	
	// Done
}

