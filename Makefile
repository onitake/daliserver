CC = gcc
# Uncomment to disable USB communication
DEFINES += -DUSB_OFF
# Uncomment to enable USB communication in a separate thread
#DEFINES += -DUSB_THREAD
CFLAGS = -O0 -g $(shell pkg-config --cflags libusb-1.0) $(DEFINES)
LDFLAGS = $(shell pkg-config --libs libusb-1.0)

.PHONY: all clean

all: usbdali

clean:
	rm -f *.o usbdali

usbdali: usbdali.o
