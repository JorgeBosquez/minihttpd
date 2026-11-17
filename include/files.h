#ifndef MINIHTTPD_FILES_H
#define MINIHTTPD_FILES_H
#include <stddef.h>
struct static_file {
    int fd;
    size_t size;
    const char *type;
};
/* Abre un archivo regular dentro de www. Devuelve estado HTTP.
 * En 200 el llamador es responsable de cerrar file->fd.
 */
int files_open(const char *uri, struct static_file *file);
#endif
