ifeq ($(CROSS_COMPILE),)
	CC := gcc
else
	CC := $(CROSS_COMPILE)gcc
endif

default: writer

writer:
	$(CC) -o writer writer.c

clean:
	-rm -f writer *.o

