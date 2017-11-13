#Makefile

PROGRAM = fs.o
CFLAGS += `pkg-config fuse3 -cflags -libs`

all: fs mkfs.fs

fs: $(PROGRAM)
	$(CC) $(CFLAGS) -o $@ $(PROGRAM) -lfuse -lpmemobj

mkfs.fs: $(PROGRAM)
	$(CC) $(CFLAGS) -o $@ $(PROGRAM) -lfuse -lpmemobj

.c.o:
	$(CC) -c -o $@ -std=gnu99 -ggdb -Wall -Wextra -Werror -Wmissing-prototypes  -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse $<

clean:
	$(RM) -v $(PROGRAM)
