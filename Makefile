objects = jsonfs.o

CC = gcc

CFLAGS = -O -Wall -W -pedantic -ansi -std=c99 \
	 `pkg-config --cflags fuse jansson`

LDFLAGS = `pkg-config --libs fuse jansson`

jsonfs: $(objects)
	$(CC) $(CFLAGS) -o jsonfs $(objects) $(LDFLAGS)

.PHONY: clean
clean:
	rm jsonfs $(objects)
