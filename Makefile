SHELL = bash

CC = clang

CFLAGS += -std=c99
CFLAGS += -pipe
CFLAGS += -Wall
CFLAGS += -O3
CFLAGS += -D_XOPEN_SOURCE
CFLAGS += -D_BSD_SOURCE

LDFLAGS += -lconfig
LDFLAGS += -lm
LDFLAGS += -lpcre
LDFLAGS += -lrt
LDFLAGS += -luuid

PROGRAM = bin/smtp-server

HEADERS = $(wildcard src/*.h) src/fsm.h
SOURCES = $(wildcard src/*.c) src/fsm.c
OBJECTS = $(patsubst src/%.c, obj/%.o, $(SOURCES))

all: $(PROGRAM)

bin:
	mkdir bin

$(PROGRAM): bin $(OBJECTS)
	$(CC) -o $@ obj/*.o $(LDFLAGS)

obj:
	mkdir obj

obj/%.o: src/%.c $(HEADERS) obj
	$(CC) -c $(CFLAGS) -o $@ $<

src/fsm: src/fsm.def src/fsm.in
	cp -f src/fsm.in src/fsm-fsm.c && \
	cd src && \
	autogen fsm.def && \
	mv -f fsm-fsm.h fsm.h && \
	mv -f fsm-fsm.c fsm.c && \
	sed -i 's|^#include "fsm-fsm\.h"|#include "fsm.h"|' fsm.c && \
	if ! [ -f fsm ]; then touch fsm; fi

src/fsm.h src/fsm.c: src/fsm src/fsm.def src/fsm.in

tests: test_system

test_system: $(PROGRAM)
	test/test_system

test_memory: $(PROGRAM)
	valgrind --leak-check=full --show-leak-kinds=all --trace-children=yes $(PROGRAM)/smtp-server etc/test_system.cfg

clean:
	rm -f $(PROGRAM) obj/*.o src/fsm.h src/fsm.c src/fsm
