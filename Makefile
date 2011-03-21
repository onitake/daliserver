CC = gcc
# Uncomment to disable USB communication
#DEFINES += -DUSB_OFF
# Uncomment to enable USB communication in a separate thread
DEFINES += -DUSB_THREAD
CFLAGS = -O0 -g $(shell pkg-config --cflags libusb-1.0) $(DEFINES)
LIBS = $(shell pkg-config --libs libusb-1.0)

.PHONY: all clean

all: daliserver

clean:
	rm -f *.o daliserver testpack testsock

daliserver: daliserver.o list.o util.o usb.o pack.o ipc.o
	$(CC) $(LDFLAGS) $(LIBS) -o $@ $^

testpack: testpack.o pack.o util.o
	$(CC) -o $@ $^

testsock: testsock.o
	$(CC) -o $@ $^
