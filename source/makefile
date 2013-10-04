UNAME := $(shell uname -s)

CC=gcc
CFLAGS=-Wall -Os -std=gnu99 -I -fPIC -fms-extensions -DDEBUG
LDFLAGS=-lm -lpthread
LD_LIBRARY_PATH=.
SOURCES=vm.c struct.c serial.c compile.c util.c sys.c variable.c interpret.c node.c file.c
OBJECTS=$(SOURCES:.c=.o)
FRAMEWORKS=-framework Cocoa -framework OpenGL -framework OpenAL

osx: $(OBJECTS) hal_osx.o
	cd polarssl/library && $(MAKE) all && cd ../..
	$(CC) $(OBJECTS) hal_osx.o polarssl/library/libpolarssl.a $(FRAMEWORKS) -o filagree $(LDFLAGS)
	strip filagree

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

.m.o:
	$(CC) -c $(CFLAGS) $< -o $@

Javagree.class: Javagree.java
	javac Javagree.java

javagree: $(OBJECTS) Javagree.class hal_stub.o
	javah -jni Javagree
	cc -c -fPIC -DDEBUG -I/System/Library/Frameworks/JavaVM.framework/Headers javagree.c -o libjavagree.o
	libtool -dynamic -lSystem $(OBJECTS) hal_stub.o libjavagree.o -o libjavagree.dylib -macosx_version_min 10.8

clean:
	rm -f *.o *.class *.dylib filagree javagree
