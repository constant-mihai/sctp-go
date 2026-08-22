CORE := core
LIB_DIR := $(CORE)/lib
LIB := $(LIB_DIR)/libsctpcore.so

.PHONY: all core install test ctest clean

all: core

# Compiles the C sources and links core/lib/libsctpcore.so.
# Does not run the C test suite; use `make ctest` for that.
core:
	$(MAKE) -C $(CORE) shared

# Installs to /usr/local/lib; needed only if you want the .so on the default
# loader path. `make test` does not require it.
install: core
	$(MAKE) -C $(CORE) install

# LD_LIBRARY_PATH lets the test binary find the .so straight out of core/lib,
# so no sudo install is needed.
test: core
	LD_LIBRARY_PATH=$(CURDIR)/$(LIB_DIR) go test -v ./sctp

ctest:
	$(MAKE) -C $(CORE) test

clean:
	$(MAKE) -C $(CORE) clean
	go clean -testcache
