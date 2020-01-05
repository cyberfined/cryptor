AS=as
CC=gcc
CFLAGS=-std=c11 -Wall -O2
LDFLAGS=-O2
SRCS=main.c huff.c bheap.c aes.c
OBJS=$(patsubst %.c, %.o, $(SRCS))
TARGET=cryptor
.PHONY: all clean
all: $(TARGET)
$(TARGET): $(OBJS) loader.o
	$(CC) $(OBJS) loader.o -o $(TARGET) $(LDFLAGS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
loader.o: loader.s loader
	$(AS) -c loader.s -o loader.o
loader:
	make -C cryload
	cp cryload/loader ./
clean:
	make -C cryload clean
	rm -f $(OBJS) loader loader.o $(TARGET)
