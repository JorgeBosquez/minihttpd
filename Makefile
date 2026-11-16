CC = gcc
CPPFLAGS = -Iinclude -D_POSIX_C_SOURCE=200809L
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g
SOURCES = src/main.c src/server.c src/http.c src/files.c src/mime.c
OBJECTS = $(SOURCES:src/%.c=build/%.o)

.PHONY: all clean
all: minihttpd

minihttpd: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

build/%.o: src/%.c $(wildcard include/*.h) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build minihttpd
