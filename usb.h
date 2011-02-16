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

struct UsbDali;
typedef struct UsbDali UsbDali;

typedef struct {
	unsigned char address;
	unsigned char command;
} DaliFrame;

// Print a libusb error description to stderr
const char *libusb_errstring(int error);

// Create libusb context and open the first attache USBDali adapter
UsbDali *usbdali_open();
// Stop running transfers and close the device, then finalize the libusb context
void usbdali_close(UsbDali *dali);
// Submit a Dali command
int usbdali_send(UsbDali *dali, DaliFrame *frame);
// Handle pending events, submit a receive request if no transfer is active
// Wait for timeout (default: 100msec)
int usbdali_handle(UsbDali *dali);
// Set the handler timeout (in msec)
void usbdali_set_timeout(UsbDali *dali, unsigned int timeout);

// Allocate a Dali frame
DaliFrame *daliframe_new(unsigned char address, unsigned char command);
// Duplicate a Dali frame
DaliFrame *daliframe_clone(DaliFrame *frame);
// Deallocate a Dali frame
void daliframe_free(DaliFrame *frame);

#endif _USB_H
