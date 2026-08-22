include Makefile.in

LIB_DIR := lib
LIB := $(LIB_DIR)/libsctpcore.so
PREFIX ?= /usr/local

SUBDIRS := src tests
SOURCES := $(wildcard ./src/*.c)
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all src tests shared test install clean

all: $(LIB)

src:
	$(VERBOSE)$(MAKE) -C src

tests: src
	$(VERBOSE)$(MAKE) -C tests tests

# The library is a real file target, so `install` can depend on it and still
# build it on a clean tree. `shared` is kept as an alias for the old name.
$(LIB): src | $(LIB_DIR)
	$(VERBOSE)$(CC) -shared -o $@ $(OBJECTS) $(LDFLAGS)

shared: $(LIB)

$(LIB_DIR):
	$(VERBOSE)mkdir -p $@

# Building the library no longer depends on the C tests passing; run them
# explicitly with `make test`.
test: tests
	$(VERBOSE)./tests/tests

install: $(LIB)
	cp $(LIB) $(PREFIX)/lib
	ldconfig

clean:
	$(VERBOSE)for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
		done
	$(VERBOSE)rm -rf $(LIB_DIR)
