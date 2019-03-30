UNAME := $(shell uname -s)

CC=gcc
DBGFLAG=#-DDEBUG
CFLAGS=-Wall -Os -I -fPIC -fms-extensions -DFG_MAIN $(DBGFLAG)
LDFLAGS=-lm -lpthread
LD_LIBRARY_PATH=.
SOURCES=vm.c struct.c serial.c compile.c util.c sys.c variable.c interpret.c node.c file.c
OBJECTS=$(SOURCES:.c=.o)

all: $(OBJECTS) 
	$(CC) $(OBJECTS) -o filagree $(LDFLAGS)
	strip filagree

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

.m.o:
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f *.o *.class *.dylib filagree
