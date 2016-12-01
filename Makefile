SHELL=/bin/sh
DESTDIR?=/usr/local/bin/
CC=gcc
eon_cflags:=$(CFLAGS) -D_GNU_SOURCE -Wall -Wno-missing-braces -g -I./mlbuf/ -I./termbox/src/
eon_ldlibs:=$(LDLIBS) -lm -L /usr/local/Cellar/pcre/8.36/lib -lpcre -lrt
eon_objects:=$(patsubst %.c,%.o,$(wildcard *.c))
eon_static:=

all: eon

eon: ./mlbuf/libmlbuf.a ./termbox/build/libtermbox.a $(eon_objects)
	$(CC) $(eon_objects) $(eon_static) ./mlbuf/libmlbuf.a ./termbox/build/libtermbox.a $(eon_ldlibs) -o eon

eon_static: eon_static:=-static
eon_static: eon_ldlibs:=$(eon_ldlibs) -lpthread
eon_static: eon

$(eon_objects): %.o: %.c
	$(CC) -c $(eon_cflags) $< -o $@

./mlbuf/libmlbuf.a:
	$(MAKE) -C mlbuf

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
