#include "files.h"
#include "mime.h"
#include "http.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int hex(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int file_error(void)
{
    if (errno == ENOENT) return 404;
    if (errno == EACCES || errno == EPERM || errno == ELOOP || errno == ENOTDIR)
        return 403;
    return 500;
}

int files_open(const char *uri, struct static_file *file)
{
    file->fd = -1;
    char path[HTTP_MAX_URI + 16];
    size_t used = 0;
    /* Quitamos query antes de decodificar; %3F puede pertenecer al nombre. */
    for (size_t i = 0; uri[i] && uri[i] != '?'; ++i) {
        unsigned char c = (unsigned char)uri[i];
        if (c == '%') {
            if (!uri[i + 1] || !uri[i + 2]) return 400;
            int high = hex((unsigned char)uri[i + 1]);
            int low = hex((unsigned char)uri[i + 2]);
            if (high < 0 || low < 0) return 400;
            c = (unsigned char)(high * 16 + low);
            i += 2;
        }
        if (c < 32 || c == 127 || c == '\\') return 400;
        if (used >= HTTP_MAX_URI) return 400;
        path[used++] = (char)c;
    }
    path[used] = '\0';
    if (path[0] != '/') return 400;
    if (strcmp(path, "/") == 0)
        memcpy(path, "/index.html", sizeof("/index.html"));

    /* Rechazamos segmentos .., incluso si estaban codificados en la URI. */
    for (char *segment = path; *segment;) {
        while (*segment == '/') ++segment;
        char *end = segment;
        while (*end && *end != '/') ++end;
        if (end - segment == 2 && segment[0] == '.' && segment[1] == '.')
            return 403;
        segment = end;
    }

    char root[PATH_MAX], candidate[PATH_MAX], resolved[PATH_MAX];
    if (!realpath("www", root)) return 500;
    int count = snprintf(candidate, sizeof(candidate), "%s%s", root, path);
    if (count < 0 || (size_t)count >= sizeof(candidate)) return 400;
    if (!realpath(candidate, resolved)) return file_error();
    size_t root_size = strlen(root);
    if (strncmp(resolved, root, root_size) != 0 || resolved[root_size] != '/')
        return 403;

    /* realpath verifica pertenencia. openat con O_NOFOLLOW evita que un
     * enlace sustituido entre verificacion y apertura escape de www.
     * Politica sencilla: no se sirven enlaces simbolicos, ni siquiera internos.
     */
    int directory = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (directory < 0) return file_error();
    char *segment = path + 1;
    int fd = -1;
    for (;;) {
        char *slash = strchr(segment, '/');
        if (slash) *slash = '\0';
        if (*segment) {
            int flags = O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK;
            if (slash) flags |= O_DIRECTORY;
            fd = openat(directory, segment, flags);
            if (fd < 0) {
                int status = file_error();
                close(directory);
                return status;
            }
            close(directory);
            directory = fd;
        }
        if (!slash) break;
        *slash = '/';
        segment = slash + 1;
    }
    struct stat info;
    if (fstat(directory, &info) < 0) { close(directory); return 500; }
    if (!S_ISREG(info.st_mode)) { close(directory); return 403; }
    if (info.st_size < 0 || (uintmax_t)info.st_size > SIZE_MAX) {
        close(directory);
        return 500;
    }
    file->fd = directory;
    file->size = (size_t)info.st_size;
    file->type = mime_type(path);
    return 200;
}
