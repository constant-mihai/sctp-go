include ../Makefile.in

INCLUDES := -I.. -I. -I/usr/local/include/
SOURCES := $(wildcard ../src/*.c)
HEADERS := $(wildcard ../src/*.h)
OBJECTS := $(SOURCES:.c=.o)
TEST_SOURCE := $(wildcard *.c)
TEST_HEADERS := $(wildcard *.h)
TEST_OBJECTS := $(TEST_SOURCE:.c=.o)

-include $(TEST_SOURCE.c=.d) $(SOURCES.c=.d)

%.o: %.c
	@echo "$< -> $@"
	$(VERBOSE)$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -MMD -c $< -o $@

.PHONY: tests
tests: $(TEST_OBJECTS) $(OBJECTS)
	@echo "Linking $^ -> $@"
	$(VERBOSE)$(CC) $(CFLAGS) $(INCLUDES) $($LDFLAGS) $^ -o $@

clean:
	rm -rf *.o *.d
	rm -rf tests
