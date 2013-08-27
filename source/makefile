UNAME := $(shell uname -s)

CC=gcc
CFLAGS=-Wall -Os -std=gnu99 -I -fPIC -fnested-functions -fms-extensions -DDEBUG
LDFLAGS=-lm -lpthread
LD_LIBRARY_PATH=.
SOURCES=vm.c struct.c serial.c compile.c util.c sys.c variable.c interpret.c hal_stub.c node.c file.c
OBJECTS=$(SOURCES:.c=.o)

all: filagree javagree

filagree: $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	strip $@

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

Javagree.class: Javagree.java
	javac Javagree.java

javagree: $(OBJECTS) Javagree.class
	javah -jni Javagree
	cc -c -fPIC -DDEBUG -I/System/Library/Frameworks/JavaVM.framework/Headers javagree.c -o libjavagree.o
	libtool -dynamic -lSystem $(OBJECTS) libjavagree.o -o libjavagree.dylib -macosx_version_min 10.8

clean:
	rm -f *.o *.class *.dylib filagree javagree
