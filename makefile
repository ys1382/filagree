UNAME := $(shell uname -s)

CC=gcc
CFLAGS=-Wall -Os -std=gnu99 -I -fnested-functions -fms-extensions -DDEBUG
LDFLAGS=-lm -lpthread
LD_LIBRARY_PATH=.
SOURCES=vm.c struct.c serial.c compile.c util.c sys.c variable.c interpret.c hal_stub.c node.c file.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=filagree

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	strip $(EXECUTABLE)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

java: $(OBJECTS) javagree.java javagree.c
	javac javagree.java 
	javah -jni javagree
	cc -c -fPIC -I/System/Library/Frameworks/JavaVM.framework/Headers javagree.c -o libjavagree.o
	libtool -dynamic -lSystem $(OBJECTS) libjavagree.o -o libjavagree.dylib -macosx_version_min 10.8

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) 
