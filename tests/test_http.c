#include "http.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void expect(const char *text, enum http_result expected)
{
    struct http_request request;
    size_t consumed = 99;
    assert(http_parse(text, strlen(text), &request, &consumed) == expected);
    if (expected != HTTP_OK)
        assert(consumed == 0);
}

int main(void)
{
    struct http_request request;
    size_t consumed;
    const char *valid = "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\n"
                        "User-Agent: prueba\r\n\r\n";
    size_t size = strlen(valid);
    for (size_t i = 0; i < size; ++i)
        assert(http_parse(valid, i, &request, &consumed) == HTTP_INCOMPLETE);
    assert(http_parse(valid, size, &request, &consumed) == HTTP_OK);
    assert(consumed == size && request.keep_alive);
    assert(strcmp(request.uri, "/index.html") == 0);
    assert(strcmp(request.host, "localhost:8080") == 0);
    assert(strcmp(request.user_agent, "prueba") == 0);
    puts("OK: solicitud fragmentada y campos GET/Host/User-Agent");

    const char *close_request = "GET / HTTP/1.1\r\nhOsT: local\r\n"
        "Connection: keep-alive, CLOSE\r\nConnection: keep-alive\r\n\r\n";
    assert(http_parse(close_request, strlen(close_request), &request, &consumed) == HTTP_OK);
    assert(!request.keep_alive);
    expect("POST / HTTP/1.1\r\nHost: local\r\n\r\n", HTTP_METHOD_NOT_ALLOWED);
    expect("HEAD / HTTP/1.1\r\nHost: local\r\n\r\n", HTTP_METHOD_NOT_ALLOWED);
    expect("GET / HTTP/1.1\r\n\r\n", HTTP_BAD_REQUEST);
    expect("GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n", HTTP_BAD_REQUEST);
    expect("GET / HTTP/1.1\r\nHost : a\r\n\r\n", HTTP_BAD_REQUEST);
    expect("GET / HTTP/1.1\r\nHost: \r\n\r\n", HTTP_BAD_REQUEST);
    expect("GET / HTTP/1.1\r\nHost: a b\r\n\r\n", HTTP_BAD_REQUEST);
    expect("GET / HTTP/1.1\r\nHost: a\r\n X: folded\r\n\r\n", HTTP_BAD_REQUEST);
    expect("GET / HTTP/1.1\r\nHost: a\r\nContent-Length: 1\r\n\r\n", HTTP_BAD_REQUEST);
    expect("GET / HTTP/1.1\r\nHost: a\r\nContent-Length: 0\r\n\r\n", HTTP_OK);
    expect("GET / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n", HTTP_BAD_REQUEST);
    expect("GET / HTTP/1.0\r\nHost: a\r\n\r\n", HTTP_BAD_REQUEST);
    expect("GET / HTTP/1.1\nHost: a\n\n", HTTP_BAD_REQUEST);
    expect("GET / HTTP/1.1\rX", HTTP_BAD_REQUEST);
    puts("OK: Connection, metodos y solicitudes mal formadas");

    char buffer[HTTP_MAX_HEADERS + 100];
    memcpy(buffer, valid, size);
    memcpy(buffer + size, valid, size);
    assert(http_parse(buffer, size * 2, &request, &consumed) == HTTP_OK);
    assert(consumed == size); /* No consumir la siguiente solicitud. */
    memcpy(buffer, valid, size);
    buffer[5] = '\0';
    assert(http_parse(buffer, size, &request, &consumed) == HTTP_BAD_REQUEST);
    memset(buffer, 'a', sizeof(buffer));
    assert(http_parse(buffer, sizeof(buffer), &request, &consumed) == HTTP_BAD_REQUEST);
    int count = snprintf(buffer, sizeof(buffer), "GET /%0*d HTTP/1.1\r\nHost: a\r\n\r\n",
                         HTTP_MAX_URI, 0);
    assert(count > 0);
    assert(http_parse(buffer, (size_t)count, &request, &consumed) == HTTP_BAD_REQUEST);
    /* Cabeceras totales excesivas, aunque cada linea es pequena. */
    size_t offset = (size_t)snprintf(buffer, sizeof(buffer), "GET / HTTP/1.1\r\nHost: a\r\n");
    while (offset + 6 < sizeof(buffer)) {
        memcpy(buffer + offset, "X: a\r\n", 6);
        offset += 6;
    }
    assert(http_parse(buffer, offset, &request, &consumed) == HTTP_BAD_REQUEST);
    puts("OK: limites, NUL y bytes sobrantes de otra solicitud");
    return 0;
}
