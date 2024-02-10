include Makefile.in

SUBDIRS := src tests

.PHONY: all $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(VERBOSE)$(MAKE) -C $@
	$(VERBOSE)$(MAKE) -C tests tests

clean:
	$(VERBOSE)for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
		done
