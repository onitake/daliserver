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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "usb.h"
#include "list.h"
#include "pack.h"
#include "array.h"

typedef struct {
	//struct libusb_transfer *transfer;
	unsigned int seq_num;
	DaliFramePtr request;
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
	// 0 is reserved it seems
	unsigned int seq_num;
	UsbDaliInBandCallback req_callback;
	UsbDaliOutBandCallback bcast_callback;
	UsbDaliEventCallback event_callback;
	void *bcast_arg;
	void *event_arg;
	int debug;
	ArrayPtr pollfds;
	ssize_t event_index;
};

typedef struct {
/*
dr tp ?? ec ad cm st st sn .. .. .. .. .. .. ..
11 73 00 00 ff 93 ff ff 00 00 00 00 00 00 00 00
12 73 00 00 ff 08 ff ff 1d 00 00 00 00 00 00 00
12 71 00 00 00 00 00 8a 1d 00 00 00 00 00 00 00
12 73 00 00 ff 00 ff ff 1c 00 00 00 00 00 00 00
12 71 00 00 00 00 00 8a 1c 00 00 00 00 00 00 00
11 74 00 04 81 6c ff ff 00 48 ff 00 df 80 fe 10
11 74 00 04 81 6c ff ff
12 74 00 03 01 68 00 91
12 72 00 00 00 00 00 3c
11 74 00 04 81 6c b3 46
11 73 00 00 ff 93 ff ff
11 77 00 00 00 03 00 53
*/
	// 11 = DALI side, 12 = USB side
	uint8_t direction;
	// 71 = 16bit transfer response (?), 72 = 24bit transfer response (?), 73 = 16bit transfer (?), 74 = 24bit transfer (?), 77 = ?
	uint8_t type;
	//uint8_t unknown_00;
	uint8_t ecommand;
	uint8_t address;
	uint8_t command;
	// type 71: 8a = NO (?)
	uint16_t status;
	uint8_t seqnum;
} UsbDaliIn;

typedef struct {
/*
dr sn ?? ty ?? ec ad cm .. .. .. .. .. .. .. ..
12 1d 00 03 00 00 ff 08 00 00 00 00 00 00 00 00
12 1c 00 03 00 00 ff 00 00 00 00 00 00 00 00 00
12 9a 00 03 00 00 02 96
12 a5 00 03 00 00 fe fe
PS2 status
12 ce 00 04 00 07 01 d5
12 d4 00 04 00 03 01 68
*/
	// 12 = USB side
	uint8_t direction;
	uint8_t seqnum;
	//uint8_t unknown_00;
	// 03 = 16bit, 04 = 24bit
	uint8_t type;
	//uint8_t unknown_00;
	uint8_t ecommand;
	uint8_t address;
	uint8_t command;
} UsbDaliOut;

enum {
	USBDALI_DIRECTION_DALI = 0x11,
	USBDALI_DIRECTION_USB = 0x12,
	USBDALI_TYPE_16BIT = 0x03,
	USBDALI_TYPE_24BIT = 0x04,
	USBDALI_TYPE_16BIT_COMPLETE = 0x71,
	USBDALI_TYPE_24BIT_COMPLETE = 0x72,
	USBDALI_TYPE_16BIT_TRANSFER = 0x73,
	USBDALI_TYPE_24BIT_TRANSFER = 0x74,
	//USBDALI_TYPE_UNKNOWN = 0x77,
};

const uint16_t VENDOR_ID = 0x17b5;
const uint16_t PRODUCT_ID = 0x0020;
const int CONFIGURATION_VALUE = 1;
const size_t USBDALI_LENGTH = 64;
const unsigned int DEFAULT_HANDLER_TIMEOUT = 100; //msec
const unsigned int DEFAULT_COMMAND_TIMEOUT = 1000; //msec
const unsigned int DEFAULT_QUEUESIZE = 50; //max. queued commands
const unsigned int MAX_LIBUSB_TIMEOUT = 1000; //sec

UsbDaliTransfer *usbdali_transfer_new(DaliFramePtr request, void *arg) {
	UsbDaliTransfer *ret = malloc(sizeof(UsbDaliTransfer));
	if (ret) {
		memset(ret, 0, sizeof(UsbDaliTransfer));
		ret->request = request;
		ret->arg = arg;
	}
	return ret;
}

void usbdali_transfer_free(UsbDaliTransfer *transfer) {
	if (transfer) {
		/*if (transfer->transfer) {
			libusb_cancel_transfer(transfer->transfer);
		}*/
		daliframe_free(transfer->request);
		free(transfer);
	}
}

const char *libusb_error_string(int error) {
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

const char *libusb_status_string(int code) {
	switch (code) {
		case LIBUSB_TRANSFER_COMPLETED:
			return "Completed";
		case LIBUSB_TRANSFER_TIMED_OUT:
			return "Timed out";
		case LIBUSB_TRANSFER_ERROR:
			return "Error";
		case LIBUSB_TRANSFER_CANCELLED:
			return "Cancelled";
		case LIBUSB_TRANSFER_STALL:
			return "Stalled";
		case LIBUSB_TRANSFER_NO_DEVICE:
			return "No device";
		case LIBUSB_TRANSFER_OVERFLOW:
			return "Overflow";
		default:
			return "";
	}
}

const char *usbdali_error_string(UsbDaliError error) {
	switch (error) {
		case USBDALI_SUCCESS:
			return "Success";
		case USBDALI_SEND_TIMEOUT:
			return "Send timeout";
		case USBDALI_RECEIVE_TIMEOUT:
			return "Receive timeout";
		case USBDALI_SEND_ERROR:
			return "Send error";
		case USBDALI_RECEIVE_ERROR:
			return "Receive error";
		case USBDALI_QUEUE_FULL:
			return "Queue full";
		case USBDALI_INVALID_ARG:
			return "Invalid argument";
		case USBDALI_NO_MEMORY:
			return "No memory";
		case USBDALI_SYSTEM_ERROR:
			return "System error";
		default:
			return "";
	}
}

void usbdali_print_in(uint8_t *buffer, size_t buflen) {
	if (buffer && buflen >= 9) {
		switch (buffer[0]) {
			case USBDALI_DIRECTION_DALI:
				printf("Direction: DALI<->DALI ");
				break;
			case USBDALI_DIRECTION_USB:
				printf("Direction: USB<->DALI ");
				break;
			default:
				printf("Direction: Unknown (%02x) ", buffer[0]);
				break;
		}
		switch (buffer[1]) {
			case USBDALI_TYPE_16BIT_COMPLETE:
				printf("Type: 16bit DALI Complete ");
				break;
			case USBDALI_TYPE_24BIT_COMPLETE:
				printf("Type: 24bit DALI Complete ");
				break;
			case USBDALI_TYPE_16BIT_TRANSFER:
				printf("Type: 16bit DALI Transfer ");
				break;
			case USBDALI_TYPE_24BIT_TRANSFER:
				printf("Type: 24bit DALI Transfer ");
				break;
				break;
			default:
				printf("Type: Unknown (%02x) ", buffer[1]);
				break;
		}
		switch (buffer[1]) {
			case USBDALI_TYPE_16BIT_COMPLETE:
			case USBDALI_TYPE_16BIT_TRANSFER:
				printf("Address: %02x ", buffer[4]);
				printf("Command: %02x ", buffer[5]);
				break;
			case USBDALI_TYPE_24BIT_COMPLETE:
			case USBDALI_TYPE_24BIT_TRANSFER:
				printf("Command: %02x ", buffer[3]);
				printf("Address: %02x ", buffer[4]);
				printf("Value: %02x ", buffer[5]);
				break;
			default:
				printf("Data: %02x %02x %02x ", buffer[3], buffer[4], buffer[5]);
				break;
		}
		printf("Status: %04x ", (buffer[6] << 8) | buffer[7]);
		switch (buffer[1]) {
			case USBDALI_TYPE_16BIT_COMPLETE:
			case USBDALI_TYPE_24BIT_COMPLETE:
				printf("Sequence number: %02x ", buffer[8]);
				break;
		}
	}
}

void usbdali_print_out(uint8_t *buffer, size_t buflen) {
	if (buffer && buflen >= 8) {
		switch (buffer[0]) {
			case USBDALI_DIRECTION_DALI:
				printf("Direction: DALI<->DALI ");
				break;
			case USBDALI_DIRECTION_USB:
				printf("Direction: USB<->DALI ");
				break;
			default:
				printf("Direction: Unknown (%02x) ", buffer[0]);
				break;
		}
		printf("Sequence number: %02x ", buffer[1]);
		switch (buffer[3]) {
			case USBDALI_TYPE_16BIT:
				printf("Type: 16bit DALI ");
				printf("Address: %02x ", buffer[6]);
				printf("Command: %02x ", buffer[7]);
				break;
			case USBDALI_TYPE_24BIT:
				printf("Type: 24bit DALI ");
				printf("Command: %02x ", buffer[5]);
				printf("Address: %02x ", buffer[6]);
				printf("Value: %02x ", buffer[7]);
				break;
			default:
				printf("Type: Unknown (%02x) ", buffer[3]);
				printf("Data: %02x %02x %02x %02x ", buffer[4], buffer[5], buffer[6], buffer[7]);
				break;
		}
	}
}

static void usbdali_add_pollfd(int fd, short events, void *user_data) {
	UsbDaliPtr dali = (UsbDaliPtr) user_data;
	if (dali) {
		struct pollfd pfd;
		pfd.fd = fd;
		pfd.events = events;
		pfd.revents = 0;
		array_append(dali->pollfds, &pfd);
	}
}

static void usbdali_remove_pollfd(int fd, void *user_data) {
	UsbDaliPtr dali = (UsbDaliPtr) user_data;
	if (dali) {
		size_t i;
		for (i = 0; i < array_length(dali->pollfds); i++) {
			if (((struct pollfd *) array_get(dali->pollfds, i))->fd == fd) {
				array_remove(dali->pollfds, i);
			}
		}
	}
}

UsbDaliPtr usbdali_open(libusb_context *context) {
	int free_context;
	if (!context) {
		free_context = 1;
		int err = libusb_init(&context);
		if (err != LIBUSB_SUCCESS) {
			fprintf(stderr, "Error initializing libusb: %s\n", libusb_error_string(err));
			return NULL;
		}
	} else {
		free_context = 0;
	}

	libusb_device_handle *handle = libusb_open_device_with_vid_pid(context, VENDOR_ID, PRODUCT_ID);
	if (handle) {
		libusb_device *device = libusb_get_device(handle);

		struct libusb_config_descriptor *config = NULL;
		int err = libusb_get_config_descriptor_by_value(device, CONFIGURATION_VALUE, &config);
		if (err == LIBUSB_SUCCESS) {
			if (config->bNumInterfaces == 1) {
				if (config->interface[0].num_altsetting == 1) {
					if (config->interface[0].altsetting[0].bNumEndpoints == 2) {
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

						err = libusb_kernel_driver_active(handle, 0);
						if (err >= LIBUSB_SUCCESS) {
							if (err == 1) {
								printf("Kernel driver is active, trying to detach\n");
								err = libusb_detach_kernel_driver(handle, 0);
								if (err != LIBUSB_SUCCESS) {
									fprintf(stderr, "Error detaching interface from kernel: %s\n", libusb_error_string(err));
								}
							}

							err = libusb_set_configuration(handle, CONFIGURATION_VALUE);
							if (err == LIBUSB_SUCCESS) {
								err = libusb_claim_interface(handle, 0);
								if (err == LIBUSB_SUCCESS) {
									err = libusb_set_interface_alt_setting(handle, 0, 0);
									if (err == LIBUSB_SUCCESS) {
										ArrayPtr pollfds = array_new(sizeof(struct pollfd));
										const struct libusb_pollfd **usbfds = libusb_get_pollfds(context);
										if (pollfds && usbfds) {
											UsbDaliPtr dali = malloc(sizeof(struct UsbDali));
											if (dali) {
												dali->context = context;
												dali->free_context = free_context;
												dali->handle = handle;
												dali->endpoint_in = endpoint_in;
												dali->endpoint_out = endpoint_out;
												dali->cmd_timeout = DEFAULT_COMMAND_TIMEOUT;
												dali->handle_timeout = DEFAULT_HANDLER_TIMEOUT;
												dali->recv_transfer = NULL;
												dali->send_transfer = NULL;
												dali->queue_size = DEFAULT_QUEUESIZE;
												dali->queue = list_new((ListDataFreeFunc) usbdali_transfer_free);
												dali->seq_num = 1;
												dali->bcast_callback = NULL;
												dali->req_callback = NULL;
												dali->event_callback = NULL;
												dali->bcast_arg = NULL;
												dali->event_arg = NULL;
												dali->debug = 0;
												dali->pollfds = pollfds;
												dali->event_index = -1;

												size_t i;
												for (i = 0; usbfds[i]; i++) {
													usbdali_add_pollfd(usbfds[i]->fd, usbfds[i]->events, dali);
												}
												free(usbfds);
												libusb_set_pollfd_notifiers(context, usbdali_add_pollfd, usbdali_remove_pollfd, dali);

												return dali;
											} else {
												fprintf(stderr, "Can't allocate device structure\n");
											}
										} else {
											fprintf(stderr, "Error creating dynamic array for poll fds, possibly out of memory\n");
										}
										if (pollfds) {
											array_free(pollfds);
										}
										if (usbfds) {
											free(usbfds);
										}
									} else {
										fprintf(stderr, "Error assigning altsetting: %s\n", libusb_error_string(err));
									}
									libusb_release_interface(handle, 0);
								} else {
									fprintf(stderr, "Error claiming interface: %s\n", libusb_error_string(err));
								}
							} else {
								fprintf(stderr, "Error setting configuration: %s\n", libusb_error_string(err));
							}
							err = libusb_attach_kernel_driver(handle, 0);
							if (err != LIBUSB_SUCCESS) {
								fprintf(stderr, "Error reattaching interface: %s\n", libusb_error_string(err));
							}
						} else {
							fprintf(stderr, "Error getting interface active state: %s\n", libusb_error_string(err));
						}
					} else {
						fprintf(stderr, "Need exactly two endpoints, got %d\n", config->interface[0].altsetting[0].bNumEndpoints);
					}
				} else {
					fprintf(stderr, "Need exactly one altsetting, got %d\n", config->interface[0].num_altsetting);
				}
			} else {
				fprintf(stderr, "Need exactly one interface, got %d\n", config->bNumInterfaces);
			}

			libusb_free_config_descriptor(config);
		} else {
			fprintf(stderr, "Error getting configuration descriptor: %s\n", libusb_error_string(err));
		}

		libusb_close(handle);
	} else {
		fprintf(stderr, "Can't find USB device\n");
	}

	if (free_context) {
		libusb_exit(context);
	}
	return NULL;
}

void usbdali_close(UsbDaliPtr dali) {
	if (dali) {
		array_free(dali->pollfds);
		
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
			fprintf(stderr, "Error reattaching interface: %s\n", libusb_error_string(err));
		}

		libusb_close(dali->handle);

		if (dali->free_context) {
			if (dali->debug) {
				printf("Freeing libusb context\n");
			}
			libusb_exit(dali->context);
		}
		
		free(dali);
	}
}

static void usbdali_receive_callback(struct libusb_transfer *transfer) {
	UsbDaliPtr dali = (UsbDaliPtr) transfer->user_data;

	if (dali->debug) {
		printf("Received data from device (status=0x%x - %s):\n", transfer->status, libusb_status_string(transfer->status));
		hexdump(transfer->buffer, transfer->actual_length);
		usbdali_print_in(transfer->buffer, transfer->actual_length);
		printf("\n");
	}

	dali->recv_transfer = NULL;

	switch (transfer->status) {
		case LIBUSB_TRANSFER_COMPLETED: {
			UsbDaliIn in;
			size_t length = transfer->actual_length;
			if (unpack(">CC CCCSC", transfer->buffer, &length, &in.direction, &in.type, &in.ecommand, &in.address, &in.command, &in.status, &in.seqnum) == -1) {
				fprintf(stderr, "Invalid packet received\n");
			} else {
				switch (in.direction) {
					case USBDALI_DIRECTION_DALI:
						switch (in.type) {
							case USBDALI_TYPE_16BIT_TRANSFER: {
								DaliFramePtr frame = daliframe_new(in.address, in.command);
								dali->bcast_callback(USBDALI_SUCCESS, frame, in.status, dali->bcast_arg);
								daliframe_free(frame);
							} break;
							case USBDALI_TYPE_24BIT_TRANSFER: {
								DaliFramePtr frame = daliframe_enew(in.ecommand, in.address, in.command);
								dali->bcast_callback(USBDALI_SUCCESS, frame, in.status, dali->bcast_arg);
								daliframe_free(frame);
							} break;
							default:
								if (dali->debug) {
									printf("Not handling unknown message type 0x%02x\n", in.type);
								}
								break;
						}
						break;
					case USBDALI_DIRECTION_USB:
						if (dali->send_transfer) {
							if (dali->send_transfer->seq_num == in.seqnum) {
								switch (in.type) {
									case USBDALI_TYPE_16BIT_COMPLETE:
										if (dali->debug) {
											printf("Transfer completed with status 0x%04x\n", in.status);
										}
										// Transfer completed, do not tail another cancel
										//dali->send_transfer->transfer = NULL;
										usbdali_transfer_free(dali->send_transfer);
										dali->send_transfer = NULL;
										break;
									case USBDALI_TYPE_24BIT_COMPLETE:
										if (dali->debug) {
											printf("Transfer completed with status 0x%04x\n", in.status);
										}
										//dali->send_transfer->transfer = NULL;
										usbdali_transfer_free(dali->send_transfer);
										dali->send_transfer = NULL;
										break;
									case USBDALI_TYPE_16BIT_TRANSFER: {
										DaliFramePtr frame = daliframe_new(in.address, in.command);
										dali->req_callback(USBDALI_SUCCESS, frame, in.status, dali->send_transfer->arg);
										daliframe_free(frame);
									} break;
									case USBDALI_TYPE_24BIT_TRANSFER: {
										DaliFramePtr frame = daliframe_enew(in.ecommand, in.address, in.command);
										dali->req_callback(USBDALI_SUCCESS, frame, in.status, dali->send_transfer->arg);
										daliframe_free(frame);
									} break;
									default:
										if (dali->debug) {
											printf("Not handling unknown message type 0x%02x\n", in.type);
										}
										break;
								}
							} else {
								fprintf(stderr, "Got response with sequence number different from transfer\n");
							}
						} else {
							fprintf(stderr, "Got response while no send transfer was active\n");
						}
						break;
				}
			}
		} break;
		case LIBUSB_TRANSFER_TIMED_OUT:
			if (dali->send_transfer) {
				dali->req_callback(USBDALI_RECEIVE_TIMEOUT, dali->send_transfer->request, 0xffff, dali->send_transfer->arg);
				//dali->send_transfer->transfer = NULL;
				usbdali_transfer_free(dali->send_transfer);
				dali->send_transfer = NULL;
			}
			// Do nothing for out of band receives - a new one will be sent from the next handle call
			break;
		case LIBUSB_TRANSFER_ERROR:
		case LIBUSB_TRANSFER_CANCELLED:
		case LIBUSB_TRANSFER_STALL:
		case LIBUSB_TRANSFER_NO_DEVICE:
		case LIBUSB_TRANSFER_OVERFLOW:
			if (dali->send_transfer) {
				dali->req_callback(USBDALI_RECEIVE_ERROR, dali->send_transfer->request, 0xffff, dali->send_transfer->arg);
				//dali->send_transfer->transfer = NULL;
				usbdali_transfer_free(dali->send_transfer);
				dali->send_transfer = NULL;
			} else {
				dali->bcast_callback(USBDALI_RECEIVE_ERROR, NULL, 0xffff, dali->bcast_arg);
			}
			break;
	}

	free(transfer->buffer);
	libusb_free_transfer(transfer);
}

static int usbdali_receive(UsbDaliPtr dali) {
	if (dali && !dali->recv_transfer) {
		unsigned char *buffer = malloc(USBDALI_LENGTH);
		memset(buffer, 0, USBDALI_LENGTH);
		
		if (dali->debug) {
			printf("Receiving data from device\n");
		}
		dali->recv_transfer = libusb_alloc_transfer(0);
		libusb_fill_interrupt_transfer(dali->recv_transfer, dali->handle, dali->endpoint_in, buffer, USBDALI_LENGTH, usbdali_receive_callback, dali, dali->cmd_timeout);
		return libusb_submit_transfer(dali->recv_transfer);
	}
	return -1;
}

static void usbdali_send_callback(struct libusb_transfer *transfer) {
	UsbDaliPtr dali = transfer->user_data;

	if (dali->debug) {
		printf("Sent data to device (status=0x%x - %s):\n", transfer->status, libusb_status_string(transfer->status));
	}
	
	free(transfer->buffer);

	switch (transfer->status) {
		case LIBUSB_TRANSFER_COMPLETED:
			usbdali_receive(dali);
			break;
		case LIBUSB_TRANSFER_TIMED_OUT:
			dali->req_callback(USBDALI_SEND_TIMEOUT, dali->send_transfer->request, 0xffff, dali->send_transfer->arg);
			//dali->send_transfer->transfer = NULL;
			usbdali_transfer_free(dali->send_transfer);
			dali->send_transfer = NULL;
			break;
		case LIBUSB_TRANSFER_ERROR:
		case LIBUSB_TRANSFER_CANCELLED:
		case LIBUSB_TRANSFER_STALL:
		case LIBUSB_TRANSFER_NO_DEVICE:
		case LIBUSB_TRANSFER_OVERFLOW:
			dali->req_callback(USBDALI_SEND_ERROR, dali->send_transfer->request, 0xffff, dali->send_transfer->arg);
			//dali->send_transfer->transfer = NULL;
			usbdali_transfer_free(dali->send_transfer);
			dali->send_transfer = NULL;
			break;
	}
	
	libusb_free_transfer(transfer);
}

static int usbdali_send(UsbDaliPtr dali, UsbDaliTransfer *transfer) {
	if (dali && transfer && !dali->send_transfer) {
		if (dali->recv_transfer) {
			libusb_cancel_transfer(dali->recv_transfer);
		}

		unsigned char *buffer = malloc(USBDALI_LENGTH);
		memset(buffer, 0, USBDALI_LENGTH);
		size_t length = USBDALI_LENGTH;
		if (transfer->request->ecommand == 0) {
			if (!pack("CCCCCCCC", buffer, &length, USBDALI_DIRECTION_USB, dali->seq_num, 0x00, USBDALI_TYPE_16BIT, 0x00, 0x00, transfer->request->address, transfer->request->command)) {
				free(buffer);
				return -1;
			}
		} else {
			if (!pack("CCCCCCCC", buffer, &length, USBDALI_DIRECTION_USB, dali->seq_num, 0x00, USBDALI_TYPE_24BIT, 0x00, transfer->request->ecommand, transfer->request->address, transfer->request->command)) {
				free(buffer);
				return -1;
			}
		}
		
		if (dali->debug) {
			printf("Sending data to device:\n");
			hexdump(buffer, USBDALI_LENGTH);
			usbdali_print_out(buffer, USBDALI_LENGTH);
			printf("\n");
		}
		dali->send_transfer = transfer;
		dali->send_transfer->seq_num = dali->seq_num;
		if (dali->seq_num == 0xff) {
			dali->seq_num = 1;
		} else {
			dali->seq_num++;
		}
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		libusb_fill_interrupt_transfer(transfer, dali->handle, dali->endpoint_out, buffer, USBDALI_LENGTH, usbdali_send_callback, dali, dali->cmd_timeout);
		return libusb_submit_transfer(transfer);
	}
	return -1;
}

UsbDaliError usbdali_queue(UsbDaliPtr dali, DaliFramePtr frame, void *cbarg) {
	if (dali) {
		if (dali->debug) {
			printf("dali=%p frame=%p arg=%p\n", dali, frame, cbarg);
		}
		if (list_length(dali->queue) < dali->queue_size) {
			UsbDaliTransfer *transfer = usbdali_transfer_new(frame, cbarg);
			if (transfer) {
				if (dali->debug) {
					printf("Enqueued transfer (%p,%p)\n", transfer->request, transfer->arg);
				}
				list_enqueue(dali->queue, transfer);
				return usbdali_handle(dali);
				//return USBDALI_SUCCESS;
			}
		}
		return USBDALI_QUEUE_FULL;
	}
	return USBDALI_INVALID_ARG;
}

UsbDaliError usbdali_handle(UsbDaliPtr dali) {
	if (dali) {
		if (dali->debug) {
			//printf("Handling requests\n");
		}
		if (!dali->send_transfer) {
			if (dali->debug) {
				//printf("No send transfer active\n");
			}
			UsbDaliTransfer *transfer = list_dequeue(dali->queue);
			if (transfer) {
				if (dali->debug) {
					printf("Dequeued transfer\n");
				}
				usbdali_send(dali, transfer);
			} else {
				if (!dali->recv_transfer) {
					if (dali->debug) {
						//printf("No receive transfer active\n");
					}
					usbdali_receive(dali);
				}
			}
		}
		
		struct pollfd *fds = array_get(dali->pollfds, 0);
		int rdy = poll(fds, array_length(dali->pollfds), dali->handle_timeout);
		if (rdy == -1) {
			if (dali->debug) {
				printf("Error waiting for events: %s\n", strerror(errno));
			}
			return USBDALI_SYSTEM_ERROR;
		} else if (rdy > 0) {
			int multi = 0;
			if (dali->event_index >= 0) {
				if ((fds[dali->event_index].revents & POLLERR) || (fds[dali->event_index].revents & POLLHUP)) {
					if (dali->debug) {
						printf("Event file descriptor closed\n");
					}
					if (dali->event_callback) {
						dali->event_callback(1, dali->event_arg);
					}
				} else if (fds[dali->event_index].revents & POLLIN) {
					if (dali->event_callback) {
						dali->event_callback(0, dali->event_arg);
					}
				}
				multi = fds[dali->event_index].revents != 0;
			}
			if (!multi || rdy > 1) {
				struct timeval tv = { 0, 0 };
				int err = libusb_handle_events_timeout(dali->context, &tv);
				if (err == LIBUSB_SUCCESS) {
					return USBDALI_SUCCESS;
				} else {
					return USBDALI_SEND_ERROR;
				}
			}
		}
		return USBDALI_SUCCESS;
	}
	return USBDALI_INVALID_ARG;
}

static void usbdali_set_cmd_timeout(UsbDaliPtr dali, unsigned int timeout) {
	if (dali) {
		dali->cmd_timeout = timeout;
	}
}

void usbdali_set_handler_timeout(UsbDaliPtr dali, unsigned int timeout) {
	if (dali) {
		dali->handle_timeout = timeout;
	}
}

void usbdali_set_debug(UsbDaliPtr dali, int enable) {
	if (dali) {
		dali->debug = enable ? 1 : 0;
		libusb_set_debug(dali->context, enable ? 3 : 0);
	}
}

void usbdali_set_outband_callback(UsbDaliPtr dali, UsbDaliOutBandCallback callback, void *arg) {
	if (dali) {
		dali->bcast_callback = callback;
		dali->bcast_arg = arg;
	}
}

void usbdali_set_inband_callback(UsbDaliPtr dali, UsbDaliInBandCallback callback) {
	if (dali) {
		dali->req_callback = callback;
	}
}

void usbdali_set_event_callback(UsbDaliPtr dali, int fd, UsbDaliEventCallback callback, void *arg) {
	if (dali) {
		if (dali->event_index >= 0) {
			array_remove(dali->pollfds, dali->event_index);
		}
		dali->event_callback = callback;
		dali->event_arg = arg;
		if (fd >= 0) {
			usbdali_add_pollfd(fd, POLLIN, dali);
			dali->event_index = array_length(dali->pollfds) - 1;
		} else {
			dali->event_index = -1;
		}
	}
}

DaliFramePtr daliframe_new(uint8_t address, uint8_t command) {
	DaliFramePtr frame = malloc(sizeof(struct DaliFrame));
	memset(frame, 0, sizeof(struct DaliFrame));
	frame->address = address;
	frame->command = command;
	return frame;
}

DaliFramePtr daliframe_enew(uint8_t ecommand, uint8_t address, uint8_t command) {
	DaliFramePtr frame = malloc(sizeof(struct DaliFrame));
	memset(frame, 0, sizeof(struct DaliFrame));
	frame->ecommand = ecommand;
	frame->address = address;
	frame->command = command;
	return frame;
}

DaliFramePtr daliframe_clone(DaliFramePtr frame) {
	DaliFramePtr ret = malloc(sizeof(struct DaliFrame));
	memset(ret, 0, sizeof(struct DaliFrame));
	ret->ecommand = frame->ecommand;
	ret->address = frame->address;
	ret->command = frame->command;
	return ret;
}

void daliframe_free(DaliFramePtr frame) {
	if (frame) {
		free(frame);
	}
}
