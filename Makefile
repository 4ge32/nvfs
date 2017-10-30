#Makefile

PROGRAM = fs.o
CFLAGS += `pkg-config fuse3 -cflags -libs`

fuse-example: $(PROGRAM)
	$(CC) $(CFLAGS) -o $@ $(PROGRAM) -lfuse -lpmemobj

.c.o:
	$(CC) -c -o $@ -std=gnu99 -ggdb -Wall -Werror -Wmissing-prototypes  -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse $<

clean:
	$(RM) -v $(PROGRAM)
