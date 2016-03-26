SHELL = bash

CC = clang

CFLAGS += -std=c99
CFLAGS += -pipe
CFLAGS += -Wall
CFLAGS += -O3
CFLAGS += -g
CFLAGS += -D_XOPEN_SOURCE
CFLAGS += -D_BSD_SOURCE

LDFLAGS += -lconfig
LDFLAGS += -lcunit
LDFLAGS += -lm
LDFLAGS += -lpcre
LDFLAGS += -lrt
LDFLAGS += -luuid

PROGRAM = bin/smtp-server
TEST_PARSE = bin/test-parse

HEADERS = $(wildcard src/*.h) src/fsm.h
SOURCES += src/buffer.c
SOURCES += src/buffer_tailq.c
SOURCES += src/context.c
SOURCES += src/fsm.c
SOURCES += src/handle.c
SOURCES += src/log.c
SOURCES += src/maildir.c
SOURCES += src/parse.c
SOURCES += src/protocol.c
SOURCES += src/server.c
SOURCES += src/settings.c
SOURCES += src/signal_handle.c
SOURCES += src/time.c
SOURCES += src/transaction.c
SOURCES += src/worker.c
OBJECTS = $(patsubst src/%.c, obj/%.o, $(SOURCES))

all: $(PROGRAM) $(TEST_PARSE)

$(PROGRAM): bin $(OBJECTS) obj/main.o
	$(CC) -o $@ $(OBJECTS) obj/main.o $(LDFLAGS)

$(TEST_PARSE): bin $(OBJECTS) obj/test_parse.o
	$(CC) -o $@ $(OBJECTS) obj/test_parse.o $(LDFLAGS)

bin:
	mkdir bin

_doc:
	cd doc && $(MAKE) -e

obj:
	mkdir obj

obj/%.o: src/%.c $(HEADERS) obj src/fsm
	$(CC) -c $(CFLAGS) -o $@ $<

src/fsm: src/fsm-fsm.c src/fsm-fsm.h
	if [ -f src/fsm ]; then rm src/fsm; fi
	mv -f src/fsm-fsm.h src/fsm.h && \
	mv -f src/fsm-fsm.c src/fsm.c && \
	sed -i 's|^#include "fsm-fsm\.h"|#include "fsm.h"|' src/fsm.c

src/fsm-fsm.h: src/fsm.def src/fsm-fsm.c
	cd src && autogen fsm.def

src/fsm-fsm.c: src/fsm.in
	cp -f src/fsm.in src/fsm-fsm.c

src/fsm-fsm.h: src/fsm-fsm.c

src/fsm.h src/fsm.c: src/fsm src/fsm.def src/fsm.in

test: test_module test_system test_memory

test_module: $(TEST_PARSE) var/log
	$(TEST_PARSE) | tee var/log/test_module_result.log

test_system: $(PROGRAM) var/log
	test/test_system.bash | tee var/log/test_system_result.log

test_memory: $(PROGRAM) var/log
	test/test_memory.bash | tee var/log/test_memory_result.log

var/log:
	mkdir -p var/log

clean:
	rm -rf $(PROGRAM) obj/*.o src/fsm.h src/fsm.c src/fsm src/fsm-fsm.* \
		var/log/*.log var/mail
	cd doc && $(MAKE) clean
