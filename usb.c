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

#include "usb.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct UsbDali {
	libusb_context *context;
	libusb_device_handle *handle;
	unsigned char ep_in;
	unsigned char ep_out;
	unsigned int timeout;
	struct libusb_transfer *rtransfer;
	struct libusb_transfer *stransfer;
};

const uint16_t VENDOR_ID = 0x17b5;
const uint16_t PRODUCT_ID = 0x0020;
const int CONFIGURATION_VALUE = 1;
const size_t USBDALI_LENGTH = 64;
const unsigned char USBDALI_HEADER[] = { 0x12, 0x1c, 0x00, 0x03, 0x00, 0x00, };
const unsigned int TIMEOUT = 100; //msec

const char *libusb_errstring(int error) {
	switch (error) {
		case LIBUSB_SUCCESS:
			return "Success";
		case LIBUSB_ERROR_IO:
			return "I/O error";
		case LIBUSB_ERROR_INVALID_PARAM:
			return "Invalid parameter";
		case LIBUSB_ERROR_ACCESS:
			return "Access error";
		case LIBUSB_ERROR_NO_DEVICE:
			return "No device";
		case LIBUSB_ERROR_NOT_FOUND:
			return "Not found";
		case LIBUSB_ERROR_BUSY:
			return "Busy";
		case LIBUSB_ERROR_TIMEOUT:
			return "Timeout";
		case LIBUSB_ERROR_OVERFLOW:
			return "Overflow";
		case LIBUSB_ERROR_PIPE:
			return "Pipe error";
		case LIBUSB_ERROR_INTERRUPTED:
			return "Interrupted";
		case LIBUSB_ERROR_NO_MEM:
			return "No memory";
		case LIBUSB_ERROR_NOT_SUPPORTED:
			return "Not supported";
		case LIBUSB_ERROR_OTHER:
			return "Other error";
		default:
			return "";
	}
}

UsbDali *usbdali_open() {
	libusb_context *context = NULL;
	int err = libusb_init(&context);
	if (err != LIBUSB_SUCCESS) {
		fprintf(stderr, "Error initializing libusb: %s\n", libusb_errstring(err));
		return NULL;
	}

	//libusb_set_debug(context, 3);

	libusb_device_handle *handle = libusb_open_device_with_vid_pid(context, VENDOR_ID, PRODUCT_ID);
	if (!handle) {
		fprintf(stderr, "Can't find USB device\n");
		return NULL;
	}
	
	libusb_device *device = libusb_get_device(handle);
	
	struct libusb_config_descriptor *config = NULL;
	err = libusb_get_config_descriptor_by_value(device, CONFIGURATION_VALUE, &config);
	if (err != LIBUSB_SUCCESS) {
		fprintf(stderr, "Error getting configuration descriptor: %s\n", libusb_errstring(err));
		libusb_close(handle);
		return NULL;
	}
	if (config->bNumInterfaces != 1) {
		fprintf(stderr, "Need exactly one interface, got %d\n", config->bNumInterfaces);
		libusb_free_config_descriptor(config);
		libusb_close(handle);
		return NULL;
	}
	if (config->interface[0].num_altsetting != 1) {
		fprintf(stderr, "Need exactly one altsetting, got %d\n", config->interface[0].num_altsetting);
		libusb_free_config_descriptor(config);
		libusb_close(handle);
		return NULL;
	}
	if (config->interface[0].altsetting[0].bNumEndpoints != 2) {
		fprintf(stderr, "Need exactly two endpoints, got %d\n", config->interface[0].altsetting[0].bNumEndpoints);
		libusb_free_config_descriptor(config);
		libusb_close(handle);
		return NULL;
	}

	unsigned char endpoint_in;
	unsigned char endpoint_out;
	if ((config->interface[0].altsetting[0].endpoint[0].bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
		endpoint_in = config->interface[0].altsetting[0].endpoint[0].bEndpointAddress;
		endpoint_out = config->interface[0].altsetting[0].endpoint[1].bEndpointAddress;
	} else {
		endpoint_out = config->interface[0].altsetting[0].endpoint[0].bEndpointAddress;
		endpoint_in = config->interface[0].altsetting[0].endpoint[1].bEndpointAddress;
	}
	printf("Input endpoint: 0x%02x\n", endpoint_in);
	printf("Output endpoint: 0x%02x\n", endpoint_out);

	libusb_free_config_descriptor(config);
	
	err = libusb_set_configuration(handle, CONFIGURATION_VALUE);
	if (err != LIBUSB_SUCCESS) {
		fprintf(stderr, "Error setting configuration: %s\n", libusb_errstring(err));
		libusb_close(handle);
		return NULL;
	}

	err = libusb_kernel_driver_active(handle, 0);
	if (err < LIBUSB_SUCCESS) {
		fprintf(stderr, "Error getting interface active state: %s\n", libusb_errstring(err));
		return NULL;
	}
	if (err == 1) {
		printf("Kernel driver is active, trying to detach\n");
		err = libusb_detach_kernel_driver(handle, 0);
		if (err != LIBUSB_SUCCESS) {
			fprintf(stderr, "Error detaching interface from kernel: %s\n", libusb_errstring(err));
		}
	}
	
	err = libusb_claim_interface(handle, 0);
	if (err != LIBUSB_SUCCESS) {
		fprintf(stderr, "Error claiming interface: %s\n", libusb_errstring(err));
		err = libusb_attach_kernel_driver(handle, 0);	
		if (err != LIBUSB_SUCCESS) {
			fprintf(stderr, "Error reattaching interface: %s\n", libusb_errstring(err));
		}
		libusb_close(handle);
		return NULL;
	}
	
	err = libusb_set_interface_alt_setting(handle, 0, 0);
	if (err != LIBUSB_SUCCESS) {
		fprintf(stderr, "Error assigning altsetting: %s\n", libusb_errstring(err));
		libusb_release_interface(handle, 0);
		err = libusb_attach_kernel_driver(handle, 0);	
		if (err != LIBUSB_SUCCESS) {
			fprintf(stderr, "Error reattaching interface: %s\n", libusb_errstring(err));
		}
		libusb_close(handle);
		return NULL;
	}
	
	UsbDali *dali = malloc(sizeof(UsbDali));
	if (!dali) {
		fprintf(stderr, "Can't allocate device structure\n");
		libusb_release_interface(handle, 0);
		err = libusb_attach_kernel_driver(handle, 0);	
		if (err != LIBUSB_SUCCESS) {
			fprintf(stderr, "Error reattaching interface: %s\n", libusb_errstring(err));
		}
		libusb_close(handle);
		return NULL;
	}
	
	dali->context = context;
	dali->handle = handle;
	dali->ep_in = endpoint_in;
	dali->ep_out = endpoint_out;
	dali->timeout = TIMEOUT * 1000;
	dali->rtransfer = NULL;
	dali->stransfer = NULL;
	
	return dali;
}

void usbdali_close(UsbDali *dali) {
	if (dali) {
		if (dali->rtransfer) {
			libusb_cancel_transfer(dali->rtransfer);
		}
		if (dali->stransfer) {
			libusb_cancel_transfer(dali->stransfer);
		}
		struct timeval tv = { 0, 0 };
		libusb_handle_events_timeout(dali->context, &tv);

		libusb_release_interface(dali->handle, 0);

		int err = libusb_attach_kernel_driver(dali->handle, 0);	
		if (err != LIBUSB_SUCCESS) {
			fprintf(stderr, "Error reattaching interface: %s\n", libusb_errstring(err));
		}

		libusb_close(dali->handle);

		libusb_exit(dali->context);
		
		free(dali);
	}
}

static void usbdali_send_callback(struct libusb_transfer *transfer) {
	printf("Got USB response to send (status=0x%x)\n", transfer->status);
	free(transfer->buffer);
	UsbDali *dali = transfer->user_data;
	if (dali) {
		dali->stransfer = NULL;
	}
	libusb_free_transfer(transfer);
}

int usbdali_send(UsbDali *dali, DaliFrame *frame) {
	if (dali) {
		if (dali->rtransfer) {
			libusb_cancel_transfer(dali->rtransfer);
		}

		unsigned char *buffer = malloc(USBDALI_LENGTH);
		memset(buffer, 0, USBDALI_LENGTH);
		memcpy(buffer, USBDALI_HEADER, sizeof(USBDALI_HEADER));
		size_t offset = sizeof(USBDALI_HEADER);
		buffer[offset++] = frame->address;
		buffer[offset++] = frame->command;
		//if (frame->length > 0 && frame->data) {
		//	unsigned char maxlen = frame->length > USBDALI_LENGTH - sizeof(USBDALI_HEADER) - 3 ? USBDALI_LENGTH - sizeof(USBDALI_HEADER) - 3 : frame->length;
		//	buffer[offset++] = maxlen;
		//	memcpy(&buffer[offset], frame->data, maxlen);
		//	offset += maxlen;
		//}
		
		printf("Sending data to device:\n");
		hexdump(buffer, USBDALI_LENGTH);
		dali->stransfer = libusb_alloc_transfer(0);
		libusb_fill_interrupt_transfer(dali->stransfer, dali->handle, dali->ep_out, buffer, USBDALI_LENGTH, usbdali_send_callback, dali, TIMEOUT);
		return libusb_submit_transfer(dali->stransfer);
	}
	return -1;
}

static void usbdali_receive_callback(struct libusb_transfer *transfer) {
	printf("Received data from device (status=0x%x):\n", transfer->status);
	hexdump(transfer->buffer, transfer->actual_length);
	free(transfer->buffer);
	UsbDali *dali = transfer->user_data;
	if (dali) {
		dali->rtransfer = NULL;
	}
	libusb_free_transfer(transfer);
}

static int usbdali_receive(UsbDali *dali) {
	if (dali && !dali->rtransfer && !dali->stransfer) {
		unsigned char *buffer = malloc(USBDALI_LENGTH);
		memset(buffer, 0, USBDALI_LENGTH);
		
		printf("Receiving data from device\n");
		dali->rtransfer = libusb_alloc_transfer(0);
		libusb_fill_interrupt_transfer(dali->rtransfer, dali->handle, dali->ep_in, buffer, USBDALI_LENGTH, usbdali_receive_callback, dali, TIMEOUT);
		return libusb_submit_transfer(dali->rtransfer);
	}
	return -1;
}

int usbdali_handle(UsbDali *dali) {
	if (dali) {
		if (!dali->rtransfer) {
			usbdali_receive(dali);
		}
		
		struct timeval tv = { 0, dali->timeout };
		return libusb_handle_events_timeout(dali->context, &tv);
	}
	return -1;
}

void usbdali_set_timeout(UsbDali *dali, unsigned int timeout) {
	if (dali) {
		dali->timeout = timeout * 1000;
	}
}

DaliFrame *daliframe_new(unsigned char address, unsigned char command) {
	DaliFrame *frame = malloc(sizeof(DaliFrame));
	memset(frame, 0, sizeof(DaliFrame));
	frame->address = address;
	frame->command = command;
	return frame;
}

DaliFrame *daliframe_clone(DaliFrame *frame) {
	DaliFrame *ret = malloc(sizeof(DaliFrame));
	memset(ret, 0, sizeof(DaliFrame));
	ret->address = frame->address;
	ret->command = frame->command;
	//ret->length = frame->length;
	//ret->data = malloc(frame->length);
	//memcpy(ret->data, frame->data, ret->length);
	return ret;
}

void daliframe_free(DaliFrame *frame) {
	if (frame) {
		//if (cmd->data) {
		//	free(cmd->data);
		//}
		free(frame);
	}
}
