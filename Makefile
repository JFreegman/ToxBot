LIBS = libtoxcore
CFLAGS = -std=gnu99 -Wall -ggdb -D_XOPEN_SOURCE_EXTENDED -D_XOPEN_SOURCE -D_FILE_OFFSET_BITS=64
OBJ = toxbot.o misc.o commands.o
LDFLAGS = $(shell pkg-config --libs $(LIBS))
SRC_DIR = ./src
CC=gcc

all: $(OBJ)
	@echo "  LD    $@"
	@$(CC) $(CFLAGS) -o toxbot $(OBJ) $(LDFLAGS)

%.o: $(SRC_DIR)/%.c
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) -o $*.o -c $(SRC_DIR)/$*.c
	@$(CC) -MM $(CFLAGS) $(SRC_DIR)/$*.c > $*.d

clean: 
	rm -f *.d *.o toxbot

.PHONY: clean all
