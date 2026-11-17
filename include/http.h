#ifndef MINIHTTPD_HTTP_H
#define MINIHTTPD_HTTP_H

#include <stddef.h>

#define HTTP_MAX_HEADERS 16384
#define HTTP_MAX_LINE 4096
#define HTTP_MAX_URI 2048
#define HTTP_MAX_VALUE 1024

struct http_request {
    char uri[HTTP_MAX_URI + 1];
    char host[HTTP_MAX_VALUE + 1];
    char user_agent[HTTP_MAX_VALUE + 1];
    int keep_alive;
};

enum http_result {
    HTTP_INCOMPLETE = 0,
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_METHOD_NOT_ALLOWED = 405
};

/* No modifica data ni requiere terminador NUL. consumed solo es valido con OK.
 * El llamador acumula bytes y conserva los sobrantes para otra solicitud.
 */
enum http_result http_parse(const char *data, size_t length,
                            struct http_request *request, size_t *consumed);

int http_headers(char *out, size_t capacity, int status, int keep_alive,
                 const char *type, size_t length);

/* Construye una respuesta de texto; devuelve bytes escritos o -1. */
int http_response(char *out, size_t capacity, int status, int keep_alive);

#endif
