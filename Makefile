LIBS=libpng

INSTALL_DIR=$(HOME)/.local/bin

CFLAGS=-Wall -Werror -Wextra -Wpedantic -ansi -pedantic \
	   $(shell for lib in $(LIBS); do pkg-config --cflags $$lib; done)

CLIBS=$(shell for lib in $(LIBS); do pkg-config --libs $$lib; done)

OPT=-O3 -s -flto -march=native -mtune=native

DEBUG=-Og -g -DDEBUG

CSOURCE=main.c

.PHONY: all
all: stack-png debug

debug: $(CSOURCE)
	$(CC) $(CFLAGS) $(CLIBS) $(DEBUG) $^ -o $@

stack-png: $(CSOURCE)
	$(CC) $(CFLAGS) $(CLIBS) $(OPT) $^ -o $@

.PHONY: install
install: stack-png
	install ./stack-png $(INSTALL_DIR)/stack-png

.PHONY: clean
clean:
	rm stack-png debug
