include ../Makefile.in

INCLUDES := -I. -I/usr/local/include/
SOURCES := $(wildcard *.c)
HEADERS := $(wildcard *.h)
OBJECTS := $(SOURCES:.c=.o)

-include $(SOURCES.c=.d)

%.o: %.c %.h
	$(VERBOSE)$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -MMD -c $< -o $@

all: $(OBJECTS)

clean:
	rm -rf *.o *.d *.gcno *.gcov
