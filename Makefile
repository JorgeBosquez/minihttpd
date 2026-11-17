CC = gcc
CPPFLAGS = -Iinclude -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g
# Los modulos pendientes se agregaran cuando tengan implementacion.
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

.PHONY: test-http
test-http: build/test_http
	./build/test_http

build/test_http: tests/test_http.c src/http.c include/http.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_http.c src/http.c -o $@

.PHONY: test-partial
test-partial: build/minihttpd_partial
	python3 tests/test_epoll.py ./build/minihttpd_partial
	python3 tests/test_files.py ./build/minihttpd_partial

build/minihttpd_partial: $(SOURCES) $(wildcard include/*.h) tests/partial_send.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) tests/partial_send.c -Wl,--wrap=send -o $@
