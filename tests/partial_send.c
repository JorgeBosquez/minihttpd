/* Solo para pruebas: fuerza send parciales y EAGAIN sin cambiar produccion. */
#include <errno.h>
#include <stddef.h>
#include <sys/socket.h>

ssize_t __real_send(int fd, const void *buffer, size_t size, int flags);

ssize_t __wrap_send(int fd, const void *buffer, size_t size, int flags)
{
    static unsigned int calls;
    if (++calls % 3 == 0) {
        errno = EAGAIN;
        return -1;
    }
    return __real_send(fd, buffer, size > 7 ? 7 : size, flags);
}
