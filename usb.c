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
#include "list.h"
#include "pack.h"

typedef struct {
	struct libusb_transfer *transfer;
	unsigned int seq_num;
	DaliFrame *request;
	UsbDaliResponseCallback callback;
	void *arg;
} UsbDaliTransfer;

struct UsbDali {
	libusb_context *context;
	int free_context;
	libusb_device_handle *handle;
	unsigned char endpoint_in;
	unsigned char endpoint_out;
	unsigned int cmd_timeout;
	unsigned int handle_timeout;
	struct libusb_transfer *recv_transfer;
	UsbDaliTransfer *send_transfer;
	unsigned int queue_size;
	ListPtr queue;
	unsigned int seq_num;
	UsbDaliResponseCallback bcast_callback;
	void *bcast_arg;
};

typedef struct {
/*
       cd sn ?? ?? ad cm ?? ?? sf ?? .. .. .. .. .. ..
recv
0000   11 73 00 00 ff 93 ff ff 00 00 00 00 00 00 00 00
recv
0000   12 73 00 00 ff 08 ff ff 1d 00 00 00 00 00 00 00
0000   12 71 00 00 00 00 00 8a 1d 00 00 00 00 00 00 00
recv
0000   12 73 00 00 ff 00 ff ff 1c 00 00 00 00 00 00 00
0000   12 71 00 00 00 00 00 8a 1c 00 00 00 00 00 00 00
*/
	uint8_t code;
	uint8_t seq_num;
	uint8_t address;
	uint8_t command;
	uint16_t type;
	uint8_t seq_num_from;
} UsbDaliIn;

typedef struct {
/*
       ?? sn ?? ?? ?? ?? ad cm .. .. .. .. .. .. .. ..
send
0000   12 1d 00 03 00 00 ff 08 00 00 00 00 00 00 00 00
send
0000   12 1c 00 03 00 00 ff 00 00 00 00 00 00 00 00 00
*/
	uint8_t code;
	uint8_t seq_num;
	uint8_t address;
	uint8_t command;
} UsbDaliOut;

const uint16_t VENDOR_ID = 0x17b5;
const uint16_t PRODUCT_ID = 0x0020;
const int CONFIGURATION_VALUE = 1;
const size_t USBDALI_LENGTH = 64;
const unsigned char USBDALI_HEADER[] = { 0x12, 0x1c, 0x00, 0x03, 0x00, 0x00, };
const unsigned int DEFAULT_HANDLER_TIMEOUT = 10; //msec
const unsigned int DEFAULT_COMMAND_TIMEOUT = 1000; //msec
const unsigned int DEFAULT_QUEUESIZE = 50; //max. queued commands
const unsigned int MAX_SEQUENCE_NUMBER = 0xff;

UsbDaliTransfer *usbdali_transfer_new(DaliFrame *request, UsbDaliResponseCallback callback, void *arg) {
	UsbDaliTransfer *ret = malloc(sizeof(UsbDaliTransfer));
	if (ret) {
		memset(ret, 0, sizeof(UsbDaliTransfer));
		ret->request = request;
		ret->callback = callback;
		ret->arg = arg;
	}
	return ret;
}

void usbdali_transfer_free(UsbDaliTransfer *transfer) {
	if (transfer) {
		if (transfer->transfer) {
			libusb_cancel_transfer(transfer->transfer);
		}
		daliframe_free(transfer->request);
		free(transfer);
	}
}

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

UsbDali *usbdali_open(libusb_context *context, UsbDaliResponseCallback bcast_callback, void *arg) {
	int free_context;
	if (!context) {
		free_context = 1;
		int err = libusb_init(&context);
		if (err != LIBUSB_SUCCESS) {
			fprintf(stderr, "Error initializing libusb: %s\n", libusb_errstring(err));
			return NULL;
		}
	} else {
		free_context = 0;
	}
	
	//libusb_set_debug(context, 3);

	libusb_device_handle *handle = libusb_open_device_with_vid_pid(context, VENDOR_ID, PRODUCT_ID);
	if (!handle) {
		fprintf(stderr, "Can't find USB device\n");
		return NULL;
	}
	
	libusb_device *device = libusb_get_device(handle);
	
	struct libusb_config_descriptor *config = NULL;
	int err = libusb_get_config_descriptor_by_value(device, CONFIGURATION_VALUE, &config);
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
	dali->free_context = free_context;
	dali->handle = handle;
	dali->endpoint_in = endpoint_in;
	dali->endpoint_out = endpoint_out;
	dali->cmd_timeout = DEFAULT_COMMAND_TIMEOUT * 1000;
	dali->handle_timeout = DEFAULT_HANDLER_TIMEOUT * 1000;
	dali->recv_transfer = NULL;
	dali->send_transfer = NULL;
	dali->queue_size = DEFAULT_QUEUESIZE;
	dali->queue = list_new((ListDataFreeFunc) usbdali_transfer_free);
	dali->seq_num = 0;
	dali->bcast_callback = bcast_callback;
	dali->bcast_arg = arg;
	
	return dali;
}

void usbdali_close(UsbDali *dali) {
	if (dali) {
		list_free(dali->queue);
		
		usbdali_transfer_free(dali->send_transfer);
		if (dali->recv_transfer) {
			libusb_cancel_transfer(dali->recv_transfer);
		}
		struct timeval tv = { 0, 0 };
		libusb_handle_events_timeout(dali->context, &tv);

		libusb_release_interface(dali->handle, 0);

		int err = libusb_attach_kernel_driver(dali->handle, 0);	
		if (err != LIBUSB_SUCCESS) {
			fprintf(stderr, "Error reattaching interface: %s\n", libusb_errstring(err));
		}

		libusb_close(dali->handle);

		if (dali->free_context) {
			libusb_exit(dali->context);
		}
		
		free(dali);
	}
}

static void usbdali_receive_callback(struct libusb_transfer *transfer) {
	printf("Received data from device (status=0x%x):\n", transfer->status);
	hexdump(transfer->buffer, transfer->actual_length);
	
	UsbDaliIn in;
	size_t length = transfer->actual_length;
	unpack("CC  CCSC", transfer->buffer, &length, &in.code, &in.seq_num, &in.address, &in.command, &in.type, &in.seq_num_from);
	
	UsbDali *dali = transfer->user_data;
	if (dali->send_transfer) {
		switch (transfer->status) {
			case LIBUSB_TRANSFER_COMPLETED: {
				DaliFrame *response = daliframe_new(dali->send_transfer->request->address, 0);
				dali->send_transfer->callback(USBDALI_SUCCESS, response, dali->send_transfer->arg);
				daliframe_free(response);
			} break;
			case LIBUSB_TRANSFER_TIMED_OUT:
				dali->send_transfer->callback(USBDALI_SEND_TIMEOUT, dali->send_transfer->request, dali->send_transfer->arg);
				break;
			case LIBUSB_TRANSFER_ERROR:
			case LIBUSB_TRANSFER_CANCELLED:
			case LIBUSB_TRANSFER_STALL:
			case LIBUSB_TRANSFER_NO_DEVICE:
			case LIBUSB_TRANSFER_OVERFLOW:
				dali->send_transfer->callback(USBDALI_SEND_ERROR, dali->send_transfer->request, dali->send_transfer->arg);
				break;
		}
		dali->send_transfer->transfer = NULL;
		usbdali_transfer_free(dali->send_transfer);
		dali->send_transfer = NULL;
	} else {
		if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
			DaliFrame *msg = daliframe_new(0, 0);
			dali->bcast_callback(USBDALI_SUCCESS, msg, dali->bcast_arg);
			daliframe_free(msg);
		}
	}
	
	free(transfer->buffer);
	libusb_free_transfer(transfer);
}

static int usbdali_receive(UsbDali *dali) {
	if (dali && !dali->recv_transfer) {
		unsigned char *buffer = malloc(USBDALI_LENGTH);
		memset(buffer, 0, USBDALI_LENGTH);
		
		printf("Receiving data from device\n");
		dali->recv_transfer = libusb_alloc_transfer(0);
		libusb_fill_interrupt_transfer(dali->recv_transfer, dali->handle, dali->endpoint_in, buffer, USBDALI_LENGTH, usbdali_receive_callback, dali, dali->cmd_timeout);
		return libusb_submit_transfer(dali->recv_transfer);
	}
	return -1;
}

static void usbdali_send_callback(struct libusb_transfer *transfer) {
	printf("Sent data to device (status=0x%x)\n", transfer->status);
	free(transfer->buffer);
	UsbDali *dali = transfer->user_data;
	switch (transfer->status) {
		case LIBUSB_TRANSFER_COMPLETED:
			usbdali_receive(dali);
			break;
		case LIBUSB_TRANSFER_TIMED_OUT:
			dali->send_transfer->callback(USBDALI_SEND_TIMEOUT, dali->send_transfer->request, dali->send_transfer->arg);
			break;
		case LIBUSB_TRANSFER_ERROR:
		case LIBUSB_TRANSFER_CANCELLED:
		case LIBUSB_TRANSFER_STALL:
		case LIBUSB_TRANSFER_NO_DEVICE:
		case LIBUSB_TRANSFER_OVERFLOW:
			dali->send_transfer->callback(USBDALI_SEND_ERROR, dali->send_transfer->request, dali->send_transfer->arg);
			break;
	}
	libusb_free_transfer(transfer);
}

static int usbdali_send(UsbDali *dali, UsbDaliTransfer *transfer) {
	if (dali && transfer && !dali->send_transfer) {
		if (dali->recv_transfer) {
			libusb_cancel_transfer(dali->recv_transfer);
		}

		unsigned char *buffer = malloc(USBDALI_LENGTH);
		memset(buffer, 0, USBDALI_LENGTH);
		size_t length = USBDALI_LENGTH;
		if (!pack("CCCCCCCC", buffer, &length, 0x12, dali->seq_num, 0x00, 0x03, 0x00, 0x00, transfer->request->address, transfer->request->command)) {
			free(buffer);
			return -1;
		}
		
		printf("Sending data to device:\n");
		hexdump(buffer, USBDALI_LENGTH);
		dali->send_transfer = transfer;
		dali->send_transfer->seq_num = dali->seq_num;
		if (dali->seq_num == MAX_SEQUENCE_NUMBER) {
			dali->seq_num = 0;
		} else {
			dali->seq_num++;
		}
		dali->send_transfer->transfer = libusb_alloc_transfer(0);
		libusb_fill_interrupt_transfer(dali->send_transfer->transfer, dali->handle, dali->endpoint_out, buffer, USBDALI_LENGTH, usbdali_send_callback, dali, dali->cmd_timeout);
		return libusb_submit_transfer(dali->send_transfer->transfer);
	}
	return -1;
}

UsbDaliError usbdali_queue(UsbDali *dali, DaliFrame *frame, UsbDaliResponseCallback callback, void *arg) {
	if (dali) {
		if (list_length(dali->queue) < dali->queue_size) {
			UsbDaliTransfer *transfer = usbdali_transfer_new(frame, callback, arg);
			if (transfer) {
				list_enqueue(dali->queue, transfer);
				return USBDALI_SUCCESS;
			}
		}
	}
	return USBDALI_QUEUE_FULL;
}

UsbDaliError usbdali_handle(UsbDali *dali) {
	if (dali) {
		if (!dali->send_transfer) {
			UsbDaliTransfer *transfer = list_dequeue(dali->queue);
			if (transfer) {
				usbdali_send(dali, transfer);
			} else {
				if (!dali->recv_transfer) {
					usbdali_receive(dali);
				}
			}
		}
		
		struct timeval tv = { 0, dali->handle_timeout };
		return libusb_handle_events_timeout(dali->context, &tv);
	}
	return -1;
}

static void usbdali_set_cmd_timeout(UsbDali *dali, unsigned int timeout) {
	if (dali) {
		dali->cmd_timeout = timeout * 1000;
	}
}

void usbdali_set_handler_timeout(UsbDali *dali, unsigned int timeout) {
	if (dali) {
		dali->handle_timeout = timeout * 1000;
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
	return ret;
}

void daliframe_free(DaliFrame *frame) {
	if (frame) {
		free(frame);
	}
}
