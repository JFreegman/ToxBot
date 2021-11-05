#Install directories
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

LIBS = toxcore
CFLAGS += -std=c11 -Wall -g -D_XOPEN_SOURCE_EXTENDED -D_XOPEN_SOURCE -D_FILE_OFFSET_BITS=64
OBJ = toxbot.o misc.o commands.o groupchats.o log.o
CFLAGS += $(shell pkg-config --cflags $(LIBS))
LDFLAGS += $(shell pkg-config --libs $(LIBS))
SRC_DIR = ./src

all: $(OBJ)
	@echo "  LD    $@"
	@$(CC) $(CFLAGS) -o toxbot $(OBJ) $(LDFLAGS)

%.o: $(SRC_DIR)/%.c
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) -o $*.o -c $(SRC_DIR)/$*.c
	@$(CC) -MM $(CFLAGS) $(SRC_DIR)/$*.c > $*.d

install: toxbot
	@echo "Installing toxbot"
	@mkdir -p $(abspath $(DESTDIR)/$(BINDIR))
	@install -m 0755 toxbot $(abspath $(DESTDIR)/$(BINDIR))

clean:
	rm -f *.d *.o toxbot

uninstall:
	@echo "Uninstalling toxbot"
	@rm -f $(abspath $(DESTDIR)/$(BINDIR)/toxbot)

.PHONY: clean all
