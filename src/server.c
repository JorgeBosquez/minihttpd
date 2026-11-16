#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>

#define LISTEN_BACKLOG 32
#define MAX_EVENTS 64

/* Referencia conceptual: passiveTCP.c y passivesock.c de Comer y Stevens,
 * Internetworking with TCP/IP, volumen III, distribucion Linux/POSIX.
 * Implementacion propia: IPv4, puerto numerico y errores mediante retorno.
 */
static int create_listener(unsigned short port)
{
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket");
        return -1;
    }

    int reuse = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt");
        close(listener);
        return -1;
    }

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        close(listener);
        return -1;
    }
    if (listen(listener, LISTEN_BACKLOG) < 0) {
        perror("listen");
        close(listener);
        return -1;
    }
    return listener;
}

/* O_NONBLOCK evita esperar dentro de accept o recv. */
static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl");
        return -1;
    }
    return 0;
}

static int watch_readable(int epoll_fd, int fd)
{
    /* Modo level-triggered: vuelve a avisar mientras queden datos por leer. */
    struct epoll_event event = {0};
    event.events = EPOLLIN;
    event.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0) {
        perror("epoll_ctl ADD");
        return -1;
    }
    return 0;
}

static void close_client(int epoll_fd, int client)
{
    /* close tambien retira el socket de epoll; DEL hace explicita la retirada. */
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client, NULL);
    close(client);
}

static int accept_clients(int epoll_fd, int listener)
{
    /* Un limite por ronda permite volver a atender clientes existentes. */
    for (int i = 0; i < MAX_EVENTS; ++i) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return 0;
            if (errno == EINTR || errno == ECONNABORTED)
                continue;
            perror("accept");
            return -1;
        }
        if (set_nonblocking(client) < 0 || watch_readable(epoll_fd, client) < 0) {
            close(client);
            continue;
        }
        printf("Conexion TCP aceptada; descriptor %d.\n", client);
        fflush(stdout);
    }
    return 0;
}

static void read_client(int epoll_fd, int client)
{
    char buffer[4096];
    /* Una lectura por evento limita el trabajo de cada cliente por ronda.
     * Al usar level-triggered, epoll avisara otra vez si aun quedan datos.
     */
    ssize_t count = recv(client, buffer, sizeof(buffer), 0);
    if (count > 0) {
        printf("Cliente %d: recibidos %zd bytes.\n", client, count);
        fflush(stdout);
        /* Etapa epoll: descartamos los bytes; HTTP vendra despues. */
        return;
    }
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        return;
    if (count < 0)
        perror("recv");
    close_client(epoll_fd, client);
}

int server_run(unsigned short port)
{
    int listener = create_listener(port);
    if (listener < 0)
        return -1;
    if (set_nonblocking(listener) < 0) {
        close(listener);
        return -1;
    }
    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        close(listener);
        return -1;
    }
    if (watch_readable(epoll_fd, listener) < 0) {
        close(epoll_fd);
        close(listener);
        return -1;
    }

    printf("MiniHTTPd: epoll escuchando en 0.0.0.0:%u (Ctrl+C para salir).\n",
           (unsigned int)port);
    fflush(stdout);
    struct epoll_event events[MAX_EVENTS];
    int result = 0;
    for (;;) {
        int ready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            perror("epoll_wait");
            result = -1;
            break;
        }
        for (int i = 0; i < ready; ++i) {
            int fd = events[i].data.fd;
            if (fd == listener) {
                if (accept_clients(epoll_fd, listener) < 0) {
                    result = -1;
                    goto done;
                }
            } else {
                /* recv procesa datos, EOF y errores, incluidos HUP/ERR.
                 * No cerramos antes de consumir datos pendientes de un HUP.
                 */
                read_client(epoll_fd, fd);
            }
        }
    }
done:
    close(epoll_fd);
    close(listener);
    /* main termina inmediatamente; Linux libera los clientes restantes. */
    return result;
}
