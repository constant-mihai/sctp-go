include ../Makefile.in

INCLUDES = -I. -I/usr/local/include/
SOURCE := $(wildcard ../src/*.c)
OBJECTS := $(SOURCE:.c=.o)
TEST_SOURCE := $(wildcard *.c)
TEST_OBJECTS := $(TEST_SOURCE:.c=.o)

%.o: %.c %.h
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -c $< -o $@

tests: $(OBJECTS) $(TEST_OBJECTS)
	$(CC) $(CFLAGS) $($LDFLAGS) $^ -o $@

clean:
	rm -rf *.o *.d
	rm -rf tests
