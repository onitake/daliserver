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
#include "frame.h"
#include "dispatch.h"

struct UsbDali;
typedef struct UsbDali *UsbDaliPtr;

typedef enum {
	USBDALI_SUCCESS = 0,
	USBDALI_RESPONSE = 1,
	USBDALI_SEND_TIMEOUT = -1,
	USBDALI_RECEIVE_TIMEOUT = -2,
	USBDALI_SEND_ERROR = -3,
	USBDALI_RECEIVE_ERROR = -4,
	USBDALI_QUEUE_FULL = -5,
	USBDALI_INVALID_ARG = -6,
	USBDALI_NO_MEMORY = -7,
	USBDALI_SYSTEM_ERROR = -8,
} UsbDaliError;

typedef void (*UsbDaliOutBandCallback)(UsbDaliError err, DaliFramePtr frame, unsigned int status, void *arg);
typedef void (*UsbDaliInBandCallback)(UsbDaliError err, DaliFramePtr frame, unsigned int response, unsigned int status, void *arg);
typedef void (*UsbDaliEventCallback)(int closed, void *arg);

// Return a human-readable error description of the libusb error
const char *libusb_error_string(int error);
// Return a human-readable error description of the UsbDali error
const char *usbdali_error_string(UsbDaliError error);

// Open the first attached USBDali adapter.
// Also creates a libusb context if context is NULL.
// The dispatch queue is mandatory
UsbDaliPtr usbdali_open(libusb_context *context, DispatchPtr dispatch);
// Stop running transfers and close the device, then finalize the libusb context
// if it was created by usbdali_open.
void usbdali_close(UsbDaliPtr dali);
// Enqueue a Dali command
// cbarg is the arg argument that will be passed to the inband callback
UsbDaliError usbdali_queue(UsbDaliPtr dali, DaliFramePtr frame, void *cbarg);
// Set the handler timeout (in msec, default 100)
// 0 is supposed to mean 'forever', but this isn't implemented yet.
void usbdali_set_handler_timeout(UsbDaliPtr dali, unsigned int timeout);
// Set the maximum queue size (default and maximum 255)
void usbdali_set_queue_size(UsbDaliPtr dali, unsigned int size);
// Sets the out of band message callback
void usbdali_set_outband_callback(UsbDaliPtr dali, UsbDaliOutBandCallback callback, void *arg);
// Sets the in band message callback
void usbdali_set_inband_callback(UsbDaliPtr dali, UsbDaliInBandCallback callback);
// Returns the next timeout to use for polling in msecs, -1 if no timeout is active
int usbdali_get_timeout(UsbDaliPtr dali);

#endif /*_USB_H*/

