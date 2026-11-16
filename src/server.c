#include "server.h"
#include "http.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>

#define LISTEN_BACKLOG 32
#define MAX_EVENTS 64

/* epoll guarda un puntero al estado de cada conexion. */
struct client {
    int fd;
    size_t used;
    char input[HTTP_MAX_HEADERS];
    char output[1024];
    size_t output_size;
    size_t sent;
    int close_after_send;
    struct client *previous;
    struct client *next;
};

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

static int watch_readable(int epoll_fd, struct client *client)
{
    /* Modo level-triggered: vuelve a avisar mientras queden datos por leer. */
    struct epoll_event event = {0};
    event.events = EPOLLIN;
    event.data.ptr = client;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client->fd, &event) < 0) {
        perror("epoll_ctl ADD");
        return -1;
    }
    return 0;
}

static void close_client(int epoll_fd, struct client *client, struct client **head)
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
    close(client->fd);
    if (client->previous)
        client->previous->next = client->next;
    else
        *head = client->next;
    if (client->next)
        client->next->previous = client->previous;
    free(client);
}

static int accept_clients(int epoll_fd, int listener, struct client **head)
{
    for (int i = 0; i < MAX_EVENTS; ++i) {
        int fd = accept(listener, NULL, NULL);
        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return 0;
            if (errno == EINTR || errno == ECONNABORTED)
                continue;
            perror("accept");
            return -1;
        }
        struct client *client = calloc(1, sizeof(*client));
        if (!client) {
            perror("calloc");
            close(fd);
            continue;
        }
        client->fd = fd;
        if (set_nonblocking(fd) < 0 || watch_readable(epoll_fd, client) < 0) {
            close(fd);
            free(client);
            continue;
        }
        client->next = *head;
        if (*head)
            (*head)->previous = client;
        *head = client;
        printf("Conexion TCP aceptada; descriptor %d.\n", fd);
        fflush(stdout);
    }
    return 0;
}

/* Solo vigilamos escritura mientras hay una respuesta pendiente.
 * Durante ese envio pausamos lecturas para mantener acotada la memoria.
 */
static int change_interest(int epoll_fd, struct client *client, uint32_t events)
{
    struct epoll_event event = {0};
    event.events = events;
    event.data.ptr = client;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client->fd, &event) < 0) {
        perror("epoll_ctl MOD");
        return -1;
    }
    return 0;
}

static int queue_response(int epoll_fd, struct client *client, int status, int keep)
{
    int size = http_response(client->output, sizeof(client->output), status, keep);
    if (size < 0)
        return -1;
    client->output_size = (size_t)size;
    client->sent = 0;
    client->close_after_send = !keep;
    return change_interest(epoll_fd, client, EPOLLOUT);
}

static int process_input(int epoll_fd, struct client *client)
{
    if (!client->used)
        return change_interest(epoll_fd, client, EPOLLIN);
    struct http_request request;
    size_t consumed;
    enum http_result result = http_parse(client->input, client->used,
                                         &request, &consumed);
    if (result == HTTP_INCOMPLETE)
        return change_interest(epoll_fd, client, EPOLLIN);
    if (result != HTTP_OK) {
        client->used = 0;
        return queue_response(epoll_fd, client, result, 0);
    }
    client->used -= consumed;
    memmove(client->input, client->input + consumed, client->used);
    /* Por ahora todo GET valido recibe texto fijo, sin acceder a www. */
    return queue_response(epoll_fd, client, 200, request.keep_alive);
}

/* Devuelven -1 cuando el llamador debe cerrar y liberar el cliente. */
static int read_client(int epoll_fd, struct client *client)
{
    ssize_t count = recv(client->fd, client->input + client->used,
                         sizeof(client->input) - client->used, 0);
    if (count < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return 0;
        perror("recv");
        return -1;
    }
    if (count == 0) {
        if (client->used)
            return queue_response(epoll_fd, client, 400, 0);
        return -1;
    }
    client->used += (size_t)count;
    return process_input(epoll_fd, client);
}

static int write_client(int epoll_fd, struct client *client)
{
    ssize_t count = send(client->fd, client->output + client->sent,
                         client->output_size - client->sent, MSG_NOSIGNAL);
    if (count < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return 0; /* epoll avisara otra vez; no perdemos el desplazamiento. */
        perror("send");
        return -1;
    }
    if (count == 0)
        return -1;
    client->sent += (size_t)count;
    if (client->sent < client->output_size)
        return 0;
    client->output_size = client->sent = 0;
    if (client->close_after_send)
        return -1;
    /* Procesar sobrantes ahora evita esperar un EPOLLIN que podria no llegar. */
    return process_input(epoll_fd, client);
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
    struct client listener_state = {.fd = listener};
    struct client *clients = NULL;
    if (watch_readable(epoll_fd, &listener_state) < 0) {
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
            struct client *client = events[i].data.ptr;
            if (client == &listener_state) {
                if (accept_clients(epoll_fd, listener, &clients) < 0) {
                    result = -1;
                    goto done;
                }
            } else {
                int status;
                if (events[i].events & EPOLLERR) {
                    status = -1;
                } else if (client->output_size) {
                    status = write_client(epoll_fd, client);
                } else {
                    status = read_client(epoll_fd, client);
                }
                if (status < 0)
                    close_client(epoll_fd, client, &clients);
            }
        }
    }
done:
    while (clients)
        close_client(epoll_fd, clients, &clients);
    close(epoll_fd);
    close(listener);
    return result;
}
