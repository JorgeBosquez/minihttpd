#ifndef MINIHTTPD_SERVER_H
#define MINIHTTPD_SERVER_H

#include <sys/epoll.h>

struct server;
/* Estado opaco: main coordina eventos, server administra sockets y clientes. */
struct server *server_start(unsigned short port);
int server_epoll_fd(const struct server *server);
int server_handle_event(struct server *server, const struct epoll_event *event);
void server_stop(struct server *server);

#endif
