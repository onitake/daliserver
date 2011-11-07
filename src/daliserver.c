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

#include <signal.h>
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
//     status:uint8_t
// }
// enum Status {
//     0:ok
//     1:error
// }

// Listen on this port
const unsigned short NET_PORT = 55825;
// Bind to this address
const char *NET_ADDRESS = "127.0.0.1";
// Network frame size
const size_t NET_FRAMESIZE = 2;

static IpcPtr killsocket;
static int running;

static void signal_handler(int sig);
static void dali_outband_handler(UsbDaliError err, DaliFramePtr frame, unsigned int response, void *arg);
static void dali_inband_handler(UsbDaliError err, DaliFramePtr frame, unsigned int response, void *arg);
static void net_frame_handler(void *arg, const char *buffer, size_t bufsize, ConnectionPtr conn);

int main(int argc, const char **argv) {
	log_info("Starting daliserver");

	log_debug("Initializing dispatch queue");
	DispatchPtr dispatch = dispatch_new();
	if (!dispatch) {
		return -1;
	}
	//dispatch_set_timeout(dispatch, 100);

	log_debug("Initializing USB connection");
	UsbDaliPtr usb = usbdali_open(NULL, dispatch);
	if (!usb) {
		//return -1;
	}

	log_debug("Initializing server");
	ServerPtr server = server_open(dispatch, NET_ADDRESS, NET_PORT, NET_FRAMESIZE, net_frame_handler, usb);
	if (!server) {
		return -1;
	}

	usbdali_set_outband_callback(usb, dali_outband_handler, server);
	usbdali_set_inband_callback(usb, dali_inband_handler);

	log_debug("Creating shutdown notifier");
	killsocket = ipc_new();
	if (!killsocket) {
		return -1;
	}
	ipc_register(killsocket, dispatch);

	log_info("Server ready, waiting for events");
	running = 1;
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	while (running && dispatch_run(dispatch, usbdali_get_timeout(usb)));

	log_info("Shutting daliserver down");
	ipc_free(killsocket);
	server_close(server);
	usbdali_close(usb);
	dispatch_free(dispatch);

	log_info("Exiting");
	return 0;
}

static void signal_handler(int sig) {
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

static void dali_outband_handler(UsbDaliError err, DaliFramePtr frame, unsigned int response, void *arg) {
	log_debug("Outband message received");
	if (err == USBDALI_SUCCESS) {
		log_info("Broadcast (0x%02x 0x%02x): 0x%02x", frame->address, frame->command, response & 0xff);
		ServerPtr server = (ServerPtr) arg;
		if (server) {
			char rbuffer[NET_FRAMESIZE];
			rbuffer[0] = frame->address;
			rbuffer[1] = frame->command;
			server_broadcast(server, rbuffer, sizeof(rbuffer));
		}
	}
}

static void dali_inband_handler(UsbDaliError err, DaliFramePtr frame, unsigned int response, void *arg) {
	log_debug("Inband message received");
	if (err == USBDALI_SUCCESS) {
		log_info("Response to (0x%02x 0x%02x): 0x%02x", frame->address, frame->command, response & 0xff);
		ConnectionPtr conn = (ConnectionPtr) arg;
		if (conn) {
			char rbuffer[NET_FRAMESIZE];
			rbuffer[0] = 0;
			rbuffer[1] = (uint8_t) response;
			connection_reply(conn, rbuffer, sizeof(rbuffer));
		}
	} else {
		log_error("Error sending DALI message: %s", usbdali_error_string(err));
		ConnectionPtr conn = (ConnectionPtr) arg;
		if (conn) {
			char rbuffer[NET_FRAMESIZE];
			rbuffer[0] = 1;
			rbuffer[1] = 0;
			connection_reply(conn, rbuffer, sizeof(rbuffer));
		}
	}
}

static void net_frame_handler(void *arg, const char *buffer, size_t bufsize, ConnectionPtr conn) {
	if (buffer && bufsize >= NET_FRAMESIZE) {
		DaliFramePtr frame = daliframe_new((uint8_t) buffer[0], (uint8_t) buffer[1]);
		log_info("Got frame: 0x%02x 0x%02x", frame->address, frame->command);
		UsbDaliPtr dali = (UsbDaliPtr) arg;
		if (dali) {
			usbdali_queue(dali, frame, conn);
		}
	}
}
