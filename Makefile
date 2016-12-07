WITH_PLUGINS=1

SHELL=/bin/sh
DESTDIR?=/usr/local/bin/
CC=gcc

eon_cflags:=$(CFLAGS) -O2 -D_GNU_SOURCE -Wall -Wno-missing-braces -g -I./mlbuf/ -I./termbox/src/
eon_ldlibs:=$(LDLIBS) -lm -ldl
eon_objects:=$(patsubst %.c,%.o,$(wildcard src/*.c))
eon_static:=

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
	eon_ldlibs+= -L /usr/local/Cellar/pcre/8.38/lib -lpcre
else
	eon_ldlibs+= -lrt -lpcre
endif

ifdef WITH_PLUGINS
	eon_cflags+=`-DWITH_PLUGINS pkg-config --cflags luajit`
	eon_ldlibs+=`pkg-config --libs luajit`
else
  # remove plugins.o from list of objects
	eon_objects:=$(subst src/plugins.o ,,$(eon_objects))
endif

all: eon

eon: ./mlbuf/libmlbuf.a ./termbox/build/libtermbox.a $(eon_objects)
	$(CC) $(eon_objects) $(eon_static) ./mlbuf/libmlbuf.a ./termbox/build/libtermbox.a $(eon_ldlibs) -o eon

eon_static: eon_static:=-static
eon_static: eon_ldlibs:=$(eon_ldlibs) -lpthread
eon_static: eon

$(eon_objects): %.o: %.c
	$(CC) -c $(eon_cflags) $< -o $@

./mlbuf/libmlbuf.a: ./mlbuf/patched
	$(MAKE) -C mlbuf

./mlbuf/patched: 01-mlbuf-makefile.patch
	@echo "Patching mlbuf..."
	if [ -e $@ ]; then cd mlbuf; git reset --hard HEAD; cd ..; fi
	cd mlbuf; patch -p1 < ../$<; cd ..
	cp $< $@

./termbox/build/libtermbox.a:
	@echo "Building termbox..."
	if [ ! -e termbox/build ]; then mkdir termbox/build; cd termbox/build; cmake ..; cd ..; fi
	cd termbox/build && make

test: eon test_eon
	$(MAKE) -C mlbuf test

test_eon: eon
	$(MAKE) -C tests && ./eon -v

sloc:
	find . -name '*.c' -or -name '*.h' | \
		grep -Pv '(termbox|test|ut)' | \
		xargs -rn1 cat | \
		wc -l

install: eon
	install -v -m 755 eon $(DESTDIR)

clean:
	rm -f *.o eon.bak.* gmon.out perf.data perf.data.old eon
	$(MAKE) -C mlbuf clean
	$(MAKE) -C tests clean
	rm -Rf termbox/build

.PHONY: all eon_static test test_eon sloc install clean
