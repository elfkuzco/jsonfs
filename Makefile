objects = jsonfs.o

PREFIX = $(HOME)

CC = gcc

CFLAGS = -g -O -Wall -W -pedantic -ansi -std=c99 -I$(PREFIX)/include -D_FILE_OFFSET_BITS=64
LDFLAGS = -L$(PREFIX)/lib
RPATH = -Wl,-rpath,$(PREFIX)/lib
LDLIBS = -ljansson

jsonfs: $(objects)
	$(CC) $(CFLAGS) -o jsonfs $(objects) $(LDFLAGS) $(RPATH) $(LDLIBS) `pkg-config fuse --libs`

.PHONY: clean
clean:
	rm jsonfs $(objects)
