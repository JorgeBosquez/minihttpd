#include "http.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static int token(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           (c != 0 && strchr("!#$%&'*+-.^_`|~", c) != NULL);
}

static int copy_value(char *out, size_t capacity, const char *first, const char *end)
{
    size_t size = (size_t)(end - first);
    if (size >= capacity)
        return -1;
    memcpy(out, first, size);
    out[size] = '\0';
    return 0;
}

static int named(const char *first, size_t size, const char *name)
{
    return strlen(name) == size && strncasecmp(first, name, size) == 0;
}

/* Aceptamos nombres DNS/IPv4 y literales IPv6 entre corchetes, con puerto
 * numerico opcional. Subconjunto intencional para este servidor educativo.
 */
static int valid_host(const char *host)
{
    const char *port = NULL;
    if (*host == '[') {
        const char *end = strchr(host, ']');
        char address[INET6_ADDRSTRLEN];
        struct in6_addr binary;
        if (!end || copy_value(address, sizeof(address), host + 1, end) < 0 ||
            inet_pton(AF_INET6, address, &binary) != 1)
            return 0;
        if (end[1]) {
            if (end[1] != ':') return 0;
            port = end + 2;
        }
    } else {
        const char *end = strchr(host, ':');
        if (!end) end = host + strlen(host);
        if (end == host) return 0;
        for (const char *h = host; h < end; ++h)
            if (!((*h >= 'a' && *h <= 'z') || (*h >= 'A' && *h <= 'Z') ||
                  (*h >= '0' && *h <= '9') || *h == '.' || *h == '-'))
                return 0;
        if (*end) port = end + 1;
    }
    if (port) {
        if (!*port) return 0;
        unsigned int number = 0;
        for (; *port; ++port) {
            if (*port < '0' || *port > '9') return 0;
            number = number * 10 + (unsigned int)(*port - '0');
            if (number > 65535) return 0;
        }
    }
    return 1;
}

/* Connection es una lista: close prevalece sobre keep-alive. */
static int connection_close(const char *first, const char *end, int *close_seen)
{
    int token_seen = 0;
    while (first < end) {
        while (first < end && (*first == ' ' || *first == '\t' || *first == ','))
            ++first;
        if (first == end)
            break;
        const char *start = first;
        while (first < end && token((unsigned char)*first))
            ++first;
        if (first == start)
            return -1;
        token_seen = 1;
        if (named(start, (size_t)(first - start), "close"))
            *close_seen = 1;
        while (first < end && (*first == ' ' || *first == '\t'))
            ++first;
        if (first < end && *first != ',')
            return -1;
    }
    return token_seen ? 0 : -1;
}

enum http_result http_parse(const char *data, size_t length,
                            struct http_request *request, size_t *consumed)
{
    *consumed = 0;
    memset(request, 0, sizeof(*request));
    size_t header_size = 0;
    size_t line_size = 0;
    /* Buscamos CRLF CRLF con limites, incluso si la solicitud llega por partes. */
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)data[i];
        if (i >= HTTP_MAX_HEADERS || c == 0 || c == 127 ||
            (c < 32 && c != '\r' && c != '\n' && c != '\t'))
            return HTTP_BAD_REQUEST;
        if (c == '\n') {
            if (i == 0 || data[i - 1] != '\r')
                return HTTP_BAD_REQUEST;
            line_size = 0;
        } else if (++line_size > HTTP_MAX_LINE) {
            return HTTP_BAD_REQUEST;
        }
        if (c == '\r' && i + 1 < length && data[i + 1] != '\n')
            return HTTP_BAD_REQUEST;
        if (i >= 3 && memcmp(data + i - 3, "\r\n\r\n", 4) == 0) {
            header_size = i + 1;
            break;
        }
    }
    if (header_size == 0)
        return length >= HTTP_MAX_HEADERS ? HTTP_BAD_REQUEST : HTTP_INCOMPLETE;

    /* Copia acotada para separar lineas sin modificar el buffer del cliente. */
    char headers[HTTP_MAX_HEADERS + 1];
    memcpy(headers, data, header_size);
    headers[header_size] = '\0';
    char *end = strstr(headers, "\r\n");
    *end = '\0';
    char *space = strchr(headers, ' ');
    if (space == NULL || space == headers)
        return HTTP_BAD_REQUEST;
    *space++ = '\0';
    for (const char *m = headers; *m; ++m)
        if (!token((unsigned char)*m))
            return HTTP_BAD_REQUEST;
    int get = strcmp(headers, "GET") == 0;
    char *version = strchr(space, ' ');
    if (version == NULL)
        return HTTP_BAD_REQUEST;
    *version++ = '\0';
    if (strcmp(version, "HTTP/1.1") != 0 || space[0] != '/' ||
        copy_value(request->uri, sizeof(request->uri), space, version - 1) < 0)
        return HTTP_BAD_REQUEST;
    for (const unsigned char *u = (const unsigned char *)space; *u; ++u)
        if (*u <= 32 || *u >= 127 || *u == '#')
            return HTTP_BAD_REQUEST;

    int host_seen = 0, agent_seen = 0, connection_seen = 0;
    int length_seen = 0, close_seen = 0;
    char *line = end + 2;
    while (*line != '\r') {
        end = strstr(line, "\r\n");
        *end = '\0';
        char *colon = strchr(line, ':');
        if (colon == NULL || colon == line)
            return HTTP_BAD_REQUEST;
        for (char *n = line; n < colon; ++n)
            if (!token((unsigned char)*n))
                return HTTP_BAD_REQUEST;
        size_t name_size = (size_t)(colon - line);
        char *value = colon + 1;
        while (*value == ' ' || *value == '\t')
            ++value;
        char *value_end = end;
        while (value_end > value && (value_end[-1] == ' ' || value_end[-1] == '\t'))
            --value_end;
        if (named(line, name_size, "Host")) {
            if (host_seen++ || value == value_end ||
                copy_value(request->host, sizeof(request->host), value, value_end) < 0)
                return HTTP_BAD_REQUEST;
            /* Subconjunto local: sin espacios, listas ni userinfo en Host. */
            for (char *h = value; h < value_end; ++h)
                if ((unsigned char)*h <= 32 || (unsigned char)*h >= 127 ||
                    strchr("/,?@#", *h))
                    return HTTP_BAD_REQUEST;
        } else if (named(line, name_size, "User-Agent")) {
            if (agent_seen++ || value == value_end || copy_value(request->user_agent,
                    sizeof(request->user_agent), value, value_end) < 0)
                return HTTP_BAD_REQUEST;
        } else if (named(line, name_size, "Connection")) {
            connection_seen = 1;
            if (connection_close(value, value_end, &close_seen) < 0)
                return HTTP_BAD_REQUEST;
        } else if (named(line, name_size, "Transfer-Encoding")) {
            return HTTP_BAD_REQUEST;
        } else if (named(line, name_size, "Content-Length")) {
            /* Este servidor GET no admite cuerpos; solo acepta longitud cero. */
            if (length_seen++ || value == value_end)
                return HTTP_BAD_REQUEST;
            for (char *n = value; n < value_end; ++n)
                if (*n != '0')
                    return HTTP_BAD_REQUEST;
        }
        line = end + 2;
    }
    /* Condicion academica: las tres cabeceras son obligatorias.
     * Connection y User-Agent no son obligatorias en HTTP/1.1 general.
     */
    if (!host_seen || !agent_seen || !connection_seen || !valid_host(request->host))
        return HTTP_BAD_REQUEST;
    if (!get)
        return HTTP_METHOD_NOT_ALLOWED;
    request->keep_alive = !close_seen;
    *consumed = header_size;
    return HTTP_OK;
}

static const char *reason_phrase(int status)
{
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    default: return NULL;
    }
}

int http_headers(char *out, size_t capacity, int status, int keep_alive,
                 const char *type, size_t length)
{
    const char *reason = reason_phrase(status);
    if (!reason) return -1;
    int count = snprintf(out, capacity,
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Connection: %s\r\n%s\r\n", status, reason, type, length,
        keep_alive ? "keep-alive" : "close", status == 405 ? "Allow: GET\r\n" : "");
    return count < 0 || (size_t)count >= capacity ? -1 : count;
}

int http_response(char *out, size_t capacity, int status, int keep_alive)
{
    const char *reason = reason_phrase(status);
    if (!reason) return -1;
    char body[128];
    int length = snprintf(body, sizeof(body), "%d %s\n", status, reason);
    if (length < 0 || (size_t)length >= sizeof(body)) return -1;
    int headers = http_headers(out, capacity, status, keep_alive,
                               "text/plain; charset=utf-8", (size_t)length);
    if (headers < 0 || (size_t)headers + (size_t)length >= capacity) return -1;
    memcpy(out + headers, body, (size_t)length);
    return headers + length;
}
