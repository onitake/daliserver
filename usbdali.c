#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <libusb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct {
	unsigned char address;
	unsigned char command;
	unsigned char length;
	unsigned char *data;
} DaliCmd;

const uint16_t VENDOR_ID = 0x17b5;
const uint16_t PRODUCT_ID = 0x0020;
const int CONFIGURATION_VALUE = 1;
const DaliCmd ALL_OFF = { 0xff, 0x00, 0, NULL };
const DaliCmd ALL_ON = { 0xff, 0x08, 0, NULL };
const DaliCmd ALL_DIM = { 0xfe, 0x60, 0, NULL };
const size_t USBDALI_LENGTH = 64;
const unsigned char USBDALI_HEADER[] = { 0x12, 0x1c, 0x00, 0x03, 0x00, 0x00, };

// Listen on this port
const unsigned short NET_PORT = 55825;
// Bind to this address
const char *NET_ADDRESS = "0.0.0.0";
// Network protocol:
// Packet {
//     address:uint8_t
//     command:uint8_t
//     length:uint8_t
//     data:uint8_t[MIN(length, 64)]
// }

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
			// Assume ASCII compatible charset
			if (character >= 0x20 && character <= 0x7e) {
				printf("%c", character);
			} else {
				printf(".");
			}
		}
		printf("\n");
	}
}

int usbdali_open(libusb_context *context, libusb_device_handle **handle_out, unsigned char *ep_in_out, unsigned char *ep_out_out) {
	if (!handle_out) {
		fprintf(stderr, "handle is NULL, can't continue\n");
		return -1;
	}
	
	libusb_device_handle *handle = libusb_open_device_with_vid_pid(context, VENDOR_ID, PRODUCT_ID);
	if (!handle) {
		fprintf(stderr, "Can't find USB device\n");
		return -2;
	}
	
	libusb_device *device = libusb_get_device(handle);
	
	struct libusb_config_descriptor *config = NULL;
	int err = libusb_get_config_descriptor_by_value(device, CONFIGURATION_VALUE, &config);
	if (err != LIBUSB_SUCCESS) {
		fprintf(stderr, "Error getting configuration descriptor: %s\n", libusb_errstring(err));
		return -4;
	}
	if (config->bNumInterfaces != 1) {
		fprintf(stderr, "Need exactly one interface, got %d\n", config->bNumInterfaces);
		return -5;
	}
	if (config->interface[0].num_altsetting != 1) {
		fprintf(stderr, "Need exactly one altsetting, got %d\n", config->interface[0].num_altsetting);
		return -6;
	}
	if (config->interface[0].altsetting[0].bNumEndpoints != 2) {
		fprintf(stderr, "Need exactly two endpoints, got %d\n", config->interface[0].altsetting[0].bNumEndpoints);
		return -7;
	}

	if (ep_in_out || ep_out_out) {
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
		if (ep_in_out) {
			*ep_in_out = endpoint_in;
		}
		if (ep_out_out) {
			*ep_out_out = endpoint_out;
		}
	}

	libusb_free_config_descriptor(config);
	
	err = libusb_set_configuration(handle, CONFIGURATION_VALUE);
	if (err != LIBUSB_SUCCESS) {
		fprintf(stderr, "Error setting configuration: %s\n", libusb_errstring(err));
		return -8;
	}

	err = libusb_kernel_driver_active(handle, 0);
	if (err < LIBUSB_SUCCESS) {
		fprintf(stderr, "Error getting interface active state: %s\n", libusb_errstring(err));
		return -10;
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
		return -11;
	}
	
	err = libusb_set_interface_alt_setting(handle, 0, 0);
	if (err != LIBUSB_SUCCESS) {
		fprintf(stderr, "Error assigning altsetting: %s\n", libusb_errstring(err));
		return -12;
	}
	
	*handle_out = handle;
	return 0;
}

void usbdali_close(libusb_device_handle *handle) {
	libusb_release_interface(handle, 0);

	int err = libusb_attach_kernel_driver(handle, 0);	
	if (err != LIBUSB_SUCCESS) {
		fprintf(stderr, "Error reattaching interface: %s\n", libusb_errstring(err));
	}

	libusb_close(handle);
}

DaliCmd *dali_cmd_new(unsigned char address, unsigned char command) {
	DaliCmd *ret = malloc(sizeof(DaliCmd));
	memset(ret, 0, sizeof(DaliCmd));
	ret->address = address;
	ret->command = command;
	return ret;
}

DaliCmd *dali_cmd_clone(DaliCmd *cmd) {
	DaliCmd *ret = malloc(sizeof(DaliCmd));
	memset(ret, 0, sizeof(DaliCmd));
	ret->address = cmd->address;
	ret->command = cmd->command;
	ret->length = cmd->length;
	ret->data = malloc(ret->length);
	memcpy(ret->data, cmd->data, ret->length);
	return ret;
}

void dali_cmd_free(DaliCmd *cmd) {
	if (cmd) {
		if (cmd->data) {
			free(cmd->data);
		}
		free(cmd);
	}
}

int usbdali_send(libusb_device_handle *handle, unsigned char endpoint, const DaliCmd *cmd) {
	unsigned char *buffer = malloc(USBDALI_LENGTH);
	size_t offset = USBDALI_LENGTH;
	memset(buffer, 0, USBDALI_LENGTH);
	memcpy(buffer, USBDALI_HEADER, sizeof(USBDALI_HEADER));
	buffer[offset++] = cmd->address;
	buffer[offset++] = cmd->command;
	if (cmd->length > 0 && cmd->data) {
		unsigned char maxlen = cmd->length > USBDALI_LENGTH - sizeof(USBDALI_HEADER) - 3 ? USBDALI_LENGTH - sizeof(USBDALI_HEADER) - 3 : cmd->length;
		buffer[offset++] = maxlen;
		memcpy(&buffer[offset], cmd->data, maxlen);
		offset += maxlen;
	}
	int transferred = 0;
	int err = libusb_interrupt_transfer(handle, endpoint, buffer, USBDALI_LENGTH, &transferred, 0);
	if (err != LIBUSB_SUCCESS) {
		fprintf(stderr, "Error sending data to device: %s\n", libusb_errstring(err));
		return -1;
	}
	free(buffer);
	return 0;
}

DaliCmd *usbdali_receive(libusb_device_handle *handle, unsigned char endpoint) {
	unsigned char *buffer = malloc(USBDALI_LENGTH);
	memset(buffer, 0, USBDALI_LENGTH);
	int transferred = 0;
	int err = libusb_interrupt_transfer(handle, endpoint, buffer, USBDALI_LENGTH, &transferred, 0);
	if (err != LIBUSB_SUCCESS) {
		fprintf(stderr, "Error reading data from device: %s\n", libusb_errstring(err));
		return NULL;
	}
	printf("Got data from device:\n");
	hexdump(buffer, transferred);
	DaliCmd *cmd = NULL;
	if (cmd) {
		cmd = dali_cmd_new(buffer[6], buffer[7]);
		cmd->length = buffer[8];
		if (cmd->length > 0) {
			cmd->data = malloc(cmd->length);
			memcpy(cmd->data, &buffer[9], cmd->length);
		}
	}
	free(buffer);
	return cmd;
}

typedef void (*QueueDataFreeFunc)(void *);
struct Node;
typedef struct {
	struct Node *head;
	struct Node *tail;
	QueueDataFreeFunc free;
	pthread_mutex_t mutex;
} Queue;
struct Node {
	struct Node *prev;
	struct Node *next;
	void *data;
};
typedef struct Node Node;

Queue *queue_new(QueueDataFreeFunc free_func) {
	Queue *queue = malloc(sizeof(Queue));
	if (queue) {
		queue->head = NULL;
		queue->tail = NULL;
		queue->free = free_func;
		if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
			free(queue);
			queue = NULL;
		}
	}
	return queue;
}

void queue_append(Queue *queue, void *data) {
	if (queue) {
		Node *node = malloc(sizeof(Node));
		node->next = NULL;
		node->data = data;
		pthread_mutex_lock(&queue->mutex);
		node->prev = queue->tail;
		if (queue->tail) {
			queue->tail->next = node;
		}
		queue->tail = node;
		if (!queue->head) {
			queue->head = node;
		}
		pthread_mutex_unlock(&queue->mutex);
	}
}

void *queue_remove(Queue *queue) {
	void *data = NULL;
	if (queue) {
		Node *temp = NULL;
		pthread_mutex_lock(&queue->mutex);
		if (queue->head) {
			data = queue->head->data;
			if (queue->head->next) {
				queue->head->next->prev = NULL;
			}
			temp = queue->head;
			queue->head = queue->head->next;
			if (queue->tail == temp) {
				queue->tail = NULL;
			}
		}
		pthread_mutex_unlock(&queue->mutex);
		free(temp);
	}
	return data;
}

void queue_free(Queue *queue) {
	if (queue) {
		while (queue->head) {
			void *data = queue_remove(queue);
			if (data) {
				if (queue->free) {
					queue->free(data);
				} else {
					free(data);
				}
			}
		}
		pthread_mutex_destroy(&queue->mutex);
		free(queue);
	}
}

static int running;

typedef struct {
	int error;
} LoopReturn;

void *loop_return_new(int error) {
	LoopReturn *ret = malloc(sizeof(LoopReturn));
	ret->error = error;
	if (error != 0) {
		running = 0;
	}
	return ret;
}

typedef struct {
	Queue *oqueue;
	Queue *iqueue;
	int socket;
} HandlerLoopArg;

void *handler_loop(void *arg) {
	printf("Entering handler thread\n");
	HandlerLoopArg *handler_loop_arg = (HandlerLoopArg *) arg;
	int connected = 1;
	while (running && connected) {
		struct pollfd fds[1];
		fds[0].fd = handler_loop_arg->socket;
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		int rdy_fds = poll(fds, 1, 100);
		if (rdy_fds == -1) {
			fprintf(stderr, "Error waiting for data: %s\n", strerror(errno));
			connected = 0;
		}
		if (rdy_fds == 1) {
			if (fds[0].revents & POLLIN) {
				char buffer[3];
				ssize_t rd_bytes = read(handler_loop_arg->socket, buffer, sizeof(buffer));
				if (rd_bytes == -1) {
					if (errno != ECONNRESET) {
						fprintf(stderr, "Error reading from socket: %s\n", strerror(errno));
					} else {
						printf("Connection closed, exiting handler\n");
					}
					connected = 0;
				}
				if (rd_bytes == 3) {
					printf("Got packet\n");
					char *data = malloc(buffer[2]);
					ssize_t rd_bytes = read(handler_loop_arg->socket, data, buffer[2]);
					if (rd_bytes == -1) {
						if (errno != ECONNRESET) {
							fprintf(stderr, "Error reading from socket: %s\n", strerror(errno));
						} else {
							printf("Connection closed, exiting handler\n");
						}
						connected = 0;
					} else {
						DaliCmd *cmd = dali_cmd_new(buffer[0], buffer[1]);
						cmd->length = buffer[2];
						cmd->data = data;
						queue_append(handler_loop_arg->oqueue, cmd);
					}
				}
			}
			if ((fds[0].revents & POLLHUP) || (fds[0].revents & POLLERR)) {
				printf("Connection closed\n");
				connected = 0;
			}
		}
		DaliCmd *cmd = queue_remove(handler_loop_arg->iqueue);
		if (cmd) {
			printf("Sending packet\n");
			char *buffer = malloc(3 + cmd->length);
			buffer[0] = cmd->address;
			buffer[1] = cmd->command;
			if (cmd->data && cmd->length > 0) {
				buffer[2] = cmd->length;
				memcpy(&buffer[3], cmd->data, cmd->length);
			} else {
				buffer[2] = 0;
			}
			ssize_t wr_bytes = write(handler_loop_arg->socket, buffer, buffer[2] + 3);
			if (wr_bytes == -1) {
				if (errno != ECONNRESET) {
					fprintf(stderr, "Error writing to socket: %s\n", strerror(errno));
				} else {
					printf("Connection closed, exiting handler\n");
				}
				connected = 0;
			}
			printf("Wrote %ld bytes\n", wr_bytes);
			dali_cmd_free(cmd);
		}
	}
	close(handler_loop_arg->socket);
	printf("Exiting handler thread\n");
	return NULL;
}

typedef struct {
	Queue *oqueue;
	Queue *qlist;
} ServerLoopArg;

void *server_loop(void *arg) {
	printf("Entering server thread\n");
	ServerLoopArg *server_loop_arg = (ServerLoopArg *) arg;
	int listener = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener == -1) {
		fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
		return loop_return_new(-1);
	}
	struct sockaddr_in all_if;
	memset(&all_if, 0, sizeof(all_if));
	all_if.sin_family = AF_INET;
	all_if.sin_port = htons(NET_PORT);
	if (inet_pton(AF_INET, NET_ADDRESS, &all_if.sin_addr) == -1) {
		fprintf(stderr, "Error converting address: %s\n", strerror(errno));
		close(listener);
		return loop_return_new(-1);
	}
	if (bind(listener, (struct sockaddr *) &all_if, sizeof(all_if)) == -1) {
		fprintf(stderr, "Error binding socket: %s\n", strerror(errno));
		close(listener);
		return loop_return_new(-1);
	}
	if (listen(listener, 10) == -1) {
		fprintf(stderr, "Error listening on socket: %s\n", strerror(errno));
		close(listener);
		return loop_return_new(-1);
	}
	int connected = 1;
	while (running && connected) {
		struct pollfd fds[1];
		fds[0].fd = listener;
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		int rdy_fds = poll(fds, 1, 100);
		if (rdy_fds == -1) {
			fprintf(stderr, "Error waiting for incoming connections: %s\n", strerror(errno));
			close(listener);
			return loop_return_new(-1);
		}
		if (rdy_fds == 1) {
			if ((fds[0].revents & POLLERR) || (fds[0].revents & POLLHUP)) {
				printf("Connection closed\n");
				connected = 0;
			} else if (fds[0].revents & POLLIN) {
				struct sockaddr_in incoming;
				socklen_t incoming_size = sizeof(incoming);
				int socket = accept(listener, (struct sockaddr *) &incoming, &incoming_size);
				if (socket == -1) {
					fprintf(stderr, "Error accepting connection: %s\n", strerror(errno));
					connected = 0;
				} else {
					if (incoming.sin_family != AF_INET) {
						fprintf(stderr, "Invalid address family from incoming connection %d\n", incoming.sin_family);
					} else {
						char addr[16];
						printf("Got connection from %s:%u\n", inet_ntop(incoming.sin_family, &incoming.sin_addr, addr, sizeof(addr)), incoming.sin_port);
						HandlerLoopArg handler_loop_arg;
						handler_loop_arg.oqueue = server_loop_arg->oqueue;
						handler_loop_arg.iqueue = queue_new((QueueDataFreeFunc) dali_cmd_free);
						handler_loop_arg.socket = socket;
						queue_append(server_loop_arg->qlist, handler_loop_arg.iqueue);
						pthread_t handler;
						pthread_create(&handler, NULL, handler_loop, &handler_loop_arg);
					}
				}
			}
		}
	}
	close(listener);
	printf("Exiting server thread\n");
	return loop_return_new(0);
}

typedef struct {
	Queue *oqueue;
	Queue *qlist;
	libusb_device_handle *handle;
	unsigned char oendp;
	unsigned char iendp;
} UsbLoopArg;

void *usb_loop(void *arg) {
	printf("Entering USB thread\n");
	UsbLoopArg *usb_loop_arg = (UsbLoopArg *) arg;
	unsigned int c = 0;
	while (running) {
		DaliCmd *cmd_in = NULL;
#ifndef USB_OFF
		cmd_in = usbdali_receive(usb_loop_arg->handle, usb_loop_arg->iendp);
#endif
		if (cmd_in) {
			printf("USB in address=0x%02x command=0x%02x length=%u\n", cmd_in->address, cmd_in->command, cmd_in->length);
			pthread_mutex_lock(&usb_loop_arg->qlist->mutex);
			Node *node;
			for (node = usb_loop_arg->qlist->head; node; node = node->next) {
				queue_append((Queue *) node->data, dali_cmd_clone(cmd_in));
			}
			pthread_mutex_unlock(&usb_loop_arg->qlist->mutex);
			dali_cmd_free(cmd_in);
		}
		
		DaliCmd *cmd_out = queue_remove(usb_loop_arg->oqueue);
		if (cmd_out) {
			printf("USB out address=0x%02x command=0x%02x length=%u\n", cmd_out->address, cmd_out->command, cmd_out->length);
#ifndef USB_OFF
			usbdali_send(usb_loop_arg->handle, usb_loop_arg->oendp, cmd_out);
#endif
			dali_cmd_free(cmd_out);
		}
		
		if (running) {
			struct timespec usecs = { 0, 100000000 };
			nanosleep(&usecs, NULL);
		}
	}
	printf("Exiting USB thread\n");
	return loop_return_new(0);
}

void signal_handler(int sig) {
	if (running) {
		printf("Signal received, shutting down\n");
		running = 0;
	} else {
		printf("Another signal received, killing process\n");
		kill(getpid(), SIGKILL);
	}
}

int main(int argc, const char **argv) {
	libusb_context *context = NULL;
	int err = libusb_init(&context);
	if (err != LIBUSB_SUCCESS) {
		fprintf(stderr, "Error initializing libusb: %s\n", libusb_errstring(err));
		return -1;
	}
	//libusb_set_debug(context, 3);
	libusb_device_handle *handle = NULL;
	unsigned char endpoint_out = 0;
	unsigned char endpoint_in = 0;
#ifndef USB_OFF
	if (usbdali_open(context, &handle, &endpoint_in, &endpoint_out) != 0) {
		return -1;
	}
#endif
	
	// Output command queue
	Queue *out_queue = queue_new((QueueDataFreeFunc) dali_cmd_free);
	// List the queues for all connections
	Queue *queue_list = queue_new((QueueDataFreeFunc) queue_free);
	
	running = 1;
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	
	ServerLoopArg server_loop_arg;
	server_loop_arg.oqueue = out_queue;
	server_loop_arg.qlist = queue_list;
	pthread_t server;
	pthread_create(&server, NULL, server_loop, &server_loop_arg);

	UsbLoopArg usb_loop_arg;
	usb_loop_arg.oqueue = out_queue;
	usb_loop_arg.qlist = queue_list;
	usb_loop_arg.handle = handle;
	usb_loop_arg.oendp = endpoint_out;
	usb_loop_arg.iendp = endpoint_in;
#ifdef USB_THREAD
	pthread_t usb;
	pthread_create(&usb, NULL, usb_loop, &usb_loop_arg);

	LoopReturn *usb_ret = NULL;
	pthread_join(usb, (void **) &usb_ret);
#else
	LoopReturn *usb_ret = usb_loop(&usb_loop_arg);
#endif
	if (usb_ret) {
		printf("USB thread returned: %d\n", usb_ret->error);
		free(usb_ret);
	}
	
	LoopReturn *server_ret = NULL;
	pthread_join(server, (void **) &server_ret);
	if (server_ret) {
		printf("Server thread returned: %d\n", server_ret->error);
		free(server_ret);
	}
	
	queue_free(queue_list);
	queue_free(out_queue);
	
#ifndef USB_OFF
	usbdali_close(handle);
#endif
	libusb_exit(context);
	
	return 0;
}
