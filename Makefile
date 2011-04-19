CC = gcc
AR = ar
RANLIB = ranlib
LD = gcc
# Uncomment to disable USB communication
#DEFINES += -DUSB_OFF
# Uncomment to enable USB communication in a separate thread
DEFINES += -DUSB_THREAD
CFLAGS = -O0 -g $(shell pkg-config --cflags libusb-1.0) $(DEFINES)
LIBS = $(shell pkg-config --libs libusb-1.0) -lpthread

.PHONY: all clean

all: daliserver

clean:
	rm -f *.o daliserver testpack testsock

libdaliusb.a: list.o util.o usb.o pack.o ipc.o array.o
	$(AR) cru $@ $^
	$(RANLIB) $@

daliserver: daliserver.o libdaliusb.a
	$(LD) $(LDFLAGS) $(LIBS) -o $@ $^

testpack: testpack.o pack.o util.o
	$(LD) -o $@ $^

testsock: testsock.o
	$(LD) -o $@ $^
