#Makefile

PROGRAM = fs.o
CFLAGS += `pkg-config fuse3 -cflags -libs`
LINK  = -lfuse -lpmemobj

.PHONY: all clean

all: fs mkfs_fs

fs: $(PROGRAM)
	$(CC) $(CFLAGS) -o $@ $(PROGRAM) $(LINK)

mkfs_fs: $(PROGRAM)
	$(CC) $(CFLAGS) -o $@ $(PROGRAM) $(LINK)

.c.o:
	$(CC) -c -o $@ -std=gnu99 -ggdb -Wall -Wextra -Werror -Wmissing-prototypes  -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse $<

clean:
	$(RM) -v $(PROGRAM) mkfs_fs
