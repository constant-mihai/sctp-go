include Makefile.in

SUBDIRS := src tests

SOURCES := $(wildcard ./src/*.c)
HEADERS := $(wildcard ./src/*.h)
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all test shared $(SUBDIRS)

all: $(SUBDIRS)

shared: test $(SUBDIRS)
	rm -rf ./lib/libsctpcore.so
	gcc -shared -o ./lib/libsctpcore.so $(OBJECTS) $(LDFLAGS)

install: ./lib/libsctpcore.so
	cp ./lib/libsctpcore.so /usr/local/lib
	ldconfig

test: $(SUBDIRS)
	./tests/tests

$(SUBDIRS):
	$(VERBOSE)$(MAKE) -C $@
	$(VERBOSE)$(MAKE) -C tests tests

clean:
	$(VERBOSE)for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
		done
