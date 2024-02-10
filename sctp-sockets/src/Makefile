include ../Makefile.in

INCLUDES = -I. -I/usr/local/include/
SOURCE := $(wildcard *.c)
OBJECTS := $(SOURCE:.c=.o)

%.o: %.c %.h
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -c $< -o $@

all: $(OBJECTS)

clean:
	rm -rf *.o *.d *.gcno *.gcov
