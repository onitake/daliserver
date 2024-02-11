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

#include "config.h"
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "list.h"
#include "util.h"
#include "usb.h"
#include "ipc.h"
#include "dispatch.h"
#include "net.h"
#include "log.h"
#include "frame.h"

// Network protocol:
// struct BusMessage {
//     address:uint8_t
//     command:uint8_t
// }
// struct Request {
//     address:uint8_t
//     command:uint8_t
// }
// struct Response {
//     response:uint8_t
//     status:Status
// }
// enum Status:uint8_t {
//     0:ok
//     1:error
// }

// Listen on this port
const unsigned short DEFAULT_NET_PORT = 55825;
// Bind to this address
const char *DEFAULT_NET_ADDRESS = "127.0.0.1";
// Network frame size
const size_t DEFAULT_NET_FRAMESIZE = 4;
// Network protocol number
const size_t DEFAULT_NET_PROTOCOL = 2;
// Default log level 
const unsigned int DEFAULT_LOG_LEVEL = LOG_LEVEL_INFO;
// PID file
const char *DEFAULT_PID_FILE = "/var/run/daliserver.pid";

typedef enum {
	NET_STATUS_SUCCESS = 0,
	NET_STATUS_RESPONSE = 1,
	NET_STATUS_BROADCAST = 2,
	NET_STATUS_ERROR = 255,
} NetStatus;

typedef enum {
	NET_TYPE_SEND,
} NetCommand;

typedef struct {
	unsigned short port;
	char *address;
	unsigned int loglevel;
	int dryrun;
	int syslog;
	char *logfile;
	int background;
	char *pidfile;
	int usbbus;
	int usbdev;
} Options;

static IpcPtr killsocket;
static int running;

static void signal_handler(int sig);
static void dali_outband_handler(UsbDaliError err, DaliFramePtr frame, unsigned int status, void *arg);
static void dali_inband_handler(UsbDaliError err, DaliFramePtr frame, unsigned int response, unsigned int status, void *arg);
static void net_frame_handler(void *arg, const char *buffer, size_t bufsize, ConnectionPtr conn);
static void net_dequeue_connection(void *arg, ConnectionPtr conn);
static Options *parse_opt(int argc, char *const argv[]);
static void free_opt(Options *opts);
static int split_usbdev(const char *arg, int *usbbus, int *usbdev);
static void show_help();
static void show_banner();

int main(int argc, char *const argv[]) {
	int error = 0;
	
	log_debug("Parsing options");
	Options *opts = parse_opt(argc, argv);
	if (!opts) {
		show_banner();
		show_help();
		return -1;
	}
	log_set_level(opts->loglevel);
	if (opts->logfile) {
		log_set_logfile(opts->logfile);
		log_set_logfile_level(LOG_LEVEL_MAX);
	}
#ifdef HAVE_VSYSLOG
	if (opts->syslog) {
		log_set_syslog("daliserver");
	}
#endif
	if (opts->background) {
		daemonize(opts->pidfile);
	} else {
		show_banner();
	}

	log_info("Starting daliserver");

	log_debug("Initializing dispatch queue");
	DispatchPtr dispatch = dispatch_new();
	if (!dispatch) {
		error = -1;
	} else {
		//dispatch_set_timeout(dispatch, 100);

		UsbDaliPtr usb = NULL;
		if (!opts->dryrun) {
			log_debug("Initializing USB connection");
			usb = usbdali_open(NULL, dispatch, opts->usbbus, opts->usbdev);
			if (!usb) {
				error = -1;
			}
		}

		if (opts->dryrun || usb) {
			log_debug("Initializing server");
			ServerPtr server = server_open(dispatch, opts->address, opts->port, DEFAULT_NET_FRAMESIZE, net_frame_handler, usb);

			if (!server) {
				error = -1;
			} else {
				server_set_connection_destroy_callback(server, net_dequeue_connection, usb);
				
				if (usb) {
					usbdali_set_outband_callback(usb, dali_outband_handler, server);
					usbdali_set_inband_callback(usb, dali_inband_handler);
				}

				log_debug("Creating shutdown notifier");
				killsocket = ipc_new();
				
				if (!killsocket) {
					error = -1;
				} else {
					ipc_register(killsocket, dispatch);

					log_info("Server ready, waiting for events");
					running = 1;
					signal(SIGTERM, signal_handler);
					signal(SIGINT, signal_handler);
					signal(SIGHUP, signal_handler);
					while (running && dispatch_run(dispatch, usbdali_get_timeout(usb)));

					log_info("Shutting daliserver down");
					ipc_free(killsocket);
				}
				
				server_close(server);
			}
			
			if (usb) {
				usbdali_close(usb);
			}
		}

		dispatch_free(dispatch);
	}

	free_opt(opts);
	
	log_info("Exiting");
	return error;
}

static void signal_handler(int sig) {
	if (sig == SIGHUP) {
		log_info("Signal received, reopening log file");
		log_reopen_file();
		return;
	}
	if (running) {
		log_info("Signal received, shutting down");
		running = 0;
		ipc_notify(killsocket);
	} else {
		log_fatal("Another signal received, killing process");
		kill(getpid(), SIGKILL);
		ipc_notify(killsocket);
	}
}

static void dali_outband_handler(UsbDaliError err, DaliFramePtr frame, unsigned int status, void *arg) {
	log_debug("Outband message received");
	if (err == USBDALI_SUCCESS) {
		log_info("Broadcast (0x%02x 0x%02x) [0x%04x]", frame->address, frame->command, status);
		ServerPtr server = (ServerPtr) arg;
		if (server) {
			char rbuffer[DEFAULT_NET_FRAMESIZE];
			rbuffer[0] = DEFAULT_NET_PROTOCOL;
			rbuffer[1] = NET_STATUS_BROADCAST;
			rbuffer[2] = frame->address;
			rbuffer[3] = frame->command;
			server_broadcast(server, rbuffer, sizeof(rbuffer));
		}
	}
}

static void dali_inband_handler(UsbDaliError err, DaliFramePtr frame, unsigned int response, unsigned int status, void *arg) {
	log_debug("Inband message received");
	if (err == USBDALI_SUCCESS || err == USBDALI_RESPONSE) {
		log_info("Response to (0x%02x 0x%02x): 0x%02x [0x%04x]", frame->address, frame->command, response, status);
		ConnectionPtr conn = (ConnectionPtr) arg;
		if (conn) {
			char rbuffer[DEFAULT_NET_FRAMESIZE];
			rbuffer[0] = DEFAULT_NET_PROTOCOL;
			if (err == USBDALI_RESPONSE) {
				rbuffer[1] = NET_STATUS_RESPONSE;
				rbuffer[2] = (uint8_t) response;
			} else {
				rbuffer[1] = NET_STATUS_SUCCESS;
				rbuffer[2] = 0;
			}
			rbuffer[3] = 0;
			connection_reply(conn, rbuffer, sizeof(rbuffer));
		}
	} else {
		log_error("Error sending DALI message: %s", usbdali_error_string(err));
		ConnectionPtr conn = (ConnectionPtr) arg;
		if (conn) {
			char rbuffer[DEFAULT_NET_FRAMESIZE];
			rbuffer[0] = DEFAULT_NET_PROTOCOL;
			rbuffer[1] = NET_STATUS_ERROR;
			rbuffer[2] = 0;
			rbuffer[3] = 0;
			connection_reply(conn, rbuffer, sizeof(rbuffer));
		}
	}
}

static void net_frame_handler(void *arg, const char *buffer, size_t bufsize, ConnectionPtr conn) {
	if (buffer && bufsize >= DEFAULT_NET_FRAMESIZE) {
		log_info("Got frame: 0x%02x 0x%02x 0x%02x 0x%02x", (uint8_t) buffer[0], (uint8_t) buffer[1], (uint8_t) buffer[2], (uint8_t) buffer[3]);
		if ((uint8_t) buffer[0] == DEFAULT_NET_PROTOCOL) {
			if ((uint8_t) buffer[1] == NET_TYPE_SEND) {
				UsbDaliPtr dali = (UsbDaliPtr) arg;
				if (dali) {
					DaliFramePtr frame = daliframe_new((uint8_t) buffer[2], (uint8_t) buffer[3]);
					usbdali_queue(dali, frame, conn);
				} else {
					uint8_t response = 0;
					log_info("Faking response: 0x%02x", response);
					char rbuffer[DEFAULT_NET_FRAMESIZE];
					rbuffer[0] = DEFAULT_NET_PROTOCOL;
					rbuffer[1] = NET_STATUS_RESPONSE;
					rbuffer[2] = (uint8_t) response;
					rbuffer[3] = 0;
					connection_reply(conn, rbuffer, sizeof(rbuffer));
				}
			} else {
				log_warn("Frame with unsupported command received: %u", (uint8_t) buffer[1]);
			}
		} else {
			log_warn("Frame with invalid protocol version received: %u", (uint8_t) buffer[0]);
		}
	}
}

void net_dequeue_connection(void *arg, ConnectionPtr conn) {
	if (arg && conn) {
		log_debug("Dequeueing connection %p", conn);
		UsbDaliPtr usb = (UsbDaliPtr) arg;
		usbdali_cancel(usb, conn);
	}
}


static Options *parse_opt(int argc, char *const argv[]) {
	Options *opts = malloc(sizeof(Options));
	opts->address = strdup(DEFAULT_NET_ADDRESS);
	opts->port = DEFAULT_NET_PORT;
	opts->dryrun = 0;
	opts->loglevel = DEFAULT_LOG_LEVEL;
	opts->syslog = 0;
	opts->logfile = NULL;
	opts->background = 0;
	opts->pidfile = NULL;
	opts->usbbus = -1;
	opts->usbdev = -1;

	int opt;
	opterr = 0;
	while ((opt = getopt(argc, argv, "d:l:p:nsf:br:u:")) != -1) {
		switch (opt) {
		case 'd':
			if (strcmp(optarg, "fatal") == 0) {
				opts->loglevel = LOG_LEVEL_FATAL;
			} else if (strcmp(optarg, "error") == 0) {
				opts->loglevel = LOG_LEVEL_ERROR;
			} else if (strcmp(optarg, "warn") == 0) {
				opts->loglevel = LOG_LEVEL_WARN;
			} else if (strcmp(optarg, "info") == 0) {
				opts->loglevel = LOG_LEVEL_INFO;
			} else if (strcmp(optarg, "debug") == 0) {
				opts->loglevel = LOG_LEVEL_DEBUG;
			} else {
				free_opt(opts);
				return NULL;
			}
			break;
		case 'l':
			free(opts->address);
			opts->address = strdup(optarg);
			break;
		case 'p':
			opts->port = (unsigned short) (strtol(optarg, NULL, 0) & 0xffff);
			break;
		case 'n':
			opts->dryrun = 1;
			break;
#ifdef HAVE_VSYSLOG
		case 's':
			opts->syslog = 1;
			break;
#endif
		case 'f':
			opts->logfile = strdup(optarg);
			break;
		case 'b':
			opts->background = 1;
			if (!opts->pidfile) {
				opts->pidfile = strdup(DEFAULT_PID_FILE);
			}
			break;
		case 'r':
			free(opts->pidfile);
			opts->pidfile = strdup(optarg);
			break;
		case 'u':
			if (!split_usbdev(optarg, &opts->usbbus, &opts->usbdev)) {
				free_opt(opts);
				return NULL;
			}
			break;
		default:
			free_opt(opts);
			return NULL;
		}
	}

	return opts;
}

static void free_opt(Options *opts) {
	if (opts) {
		free(opts->address);
		free(opts->logfile);
		free(opts->pidfile);
		free(opts);
	}
}

static int split_usbdev(const char *arg, int *usbbus, int *usbdev) {
	if (arg && usbbus && usbdev) {
		char *colon = strchr(arg, ':');
		if (colon) {
			long bus = strtol(arg, NULL, 0);
			long dev = strtol(colon + 1, NULL, 0);
			if (bus >= 0 && bus <= INT_MAX && dev >= 0 && dev <= INT_MAX) {
				*usbbus = (int) bus;
				*usbdev = (int) dev;
				return 1;
			}
		}
	}
	return 0;
}

static void show_help() {
	fprintf(stderr, "Usage: daliserver [-d <loglevel>] [-l <address>] [-p <port>] [-n]\n");
	fprintf(stderr, "\n");
	if (log_debug_enabled()) {
		fprintf(stderr, "-d <loglevel> Set the logging level (fatal, error, warn, info, debug, default=info)\n");
	} else {
		fprintf(stderr, "-d <loglevel> Set the logging level (fatal, error, warn, info, default=info)\n");
	}
	fprintf(stderr, "-l <address>  Set the IP address to listen on (default=127.0.0.1)\n");
	fprintf(stderr, "-p <port>     Set the port to listen on (default=55825)\n");
	fprintf(stderr, "-n            Enable dry-run mode for debugging (USB port won't be opened)\n");
#ifdef HAVE_VSYSLOG
	fprintf(stderr, "-s            Enable syslog (errors only)\n");
#endif
	if (log_debug_enabled()) {
		fprintf(stderr, "-f <logfile>  Write debug messages to logfile\n");
	} else {
		fprintf(stderr, "-f <logfile>  Write info messages to logfile\n");
	}
	fprintf(stderr, "-b            Fork into background (implies -r)\n");
	fprintf(stderr, "-r <file>     Save PID to file (default=/var/run/daliserver.pid)\n");
	fprintf(stderr, "-u <bus:dev>  Only drive the USB device at bus:dev\n");
	fprintf(stderr, "\n");
}

static void show_banner() {
	fprintf(stderr, "DALI USB multiplexer (daliserver)\n");
	fprintf(stderr, "Copyright (c) 2011 onitake All rights reserved.\n");
	fprintf(stderr, "\n");
}
