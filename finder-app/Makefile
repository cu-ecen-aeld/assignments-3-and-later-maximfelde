ifeq ($(CROSS_COMPILE), aarch64-none-linux-gnu-)
	CC=/home/maxim/gcc-arm-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc
else
	CC=gcc
endif

writer: writer.c
	$(CC) writer.c -o writer
clean: writer
	rm writer
