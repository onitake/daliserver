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

#ifndef _USB_H
#define _USB_H

#include <libusb.h>
#include <poll.h>
#include <sys/types.h>

struct UsbDali;
typedef struct UsbDali *UsbDaliPtr;

struct DaliFrame {
	uint8_t ecommand;
	uint8_t address;
	uint8_t command;
};
typedef struct DaliFrame *DaliFramePtr;

typedef enum {
	USBDALI_SUCCESS = 0,
	USBDALI_SEND_TIMEOUT = -1,
	USBDALI_RECEIVE_TIMEOUT = -2,
	USBDALI_SEND_ERROR = -3,
	USBDALI_RECEIVE_ERROR = -4,
	USBDALI_QUEUE_FULL = -5,
	USBDALI_INVALID_ARG = -6,
	USBDALI_NO_MEMORY = -7,
} UsbDaliError;

typedef void (*UsbDaliOutBandCallback)(UsbDaliError err, DaliFramePtr frame, unsigned int response, void *arg);
typedef void (*UsbDaliInBandCallback)(UsbDaliError err, DaliFramePtr frame, unsigned int response, void *arg);

// Return a human-readable error description of the libusb error
const char *libusb_error_string(int error);
// Return a human-readable error description of the UsbDali error
const char *usbdali_error_string(UsbDaliError error);

// Open the first attache USBDali adapter.
// Also creates a libusb context if context is NULL.
UsbDaliPtr usbdali_open(libusb_context *context, UsbDaliOutBandCallback bcast_callback, void *arg);
// Stop running transfers and close the device, then finalize the libusb context
// if it was created by usbdali_open.
void usbdali_close(UsbDaliPtr dali);
// Enqueue a Dali command
UsbDaliError usbdali_queue(UsbDaliPtr dali, DaliFramePtr frame, UsbDaliInBandCallback callback, void *arg);
// Handle pending events, submit a receive request if no transfer is active.
// Then wait for timeout
UsbDaliError usbdali_handle(UsbDaliPtr dali);
// Set the handler timeout (in msec, default 10, 0 is nonblocking)
void usbdali_set_handler_timeout(UsbDaliPtr dali, unsigned int timeout);
// Set the maximum queue size (default 50)
void usbdali_set_queue_size(UsbDaliPtr dali, unsigned int size);
// Prepare a struct pollfd array with the libusb polling file descriptors filled in.
// Reserves space at the front so you can fill in your own descriptors.
// The array must be free'd when done.
UsbDaliError usbdali_pollfds(UsbDaliPtr dali, size_t reserve, struct pollfd **fds, size_t *nfds);

// Allocate a Dali frame
DaliFramePtr daliframe_new(uint8_t address, uint8_t command);
DaliFramePtr daliframe_enew(uint8_t ecommand, uint8_t address, uint8_t command);
// Duplicate a Dali frame
DaliFramePtr daliframe_clone(DaliFramePtr frame);
// Deallocate a Dali frame
void daliframe_free(DaliFramePtr frame);

#endif /*_USB_H*/
