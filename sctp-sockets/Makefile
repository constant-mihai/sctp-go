SUBDIRS := src tests

.PHONY: all $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	$(VERBOSE)for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
		done
