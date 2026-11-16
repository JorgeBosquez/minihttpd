#ifndef MINIHTTPD_SERVER_H
#define MINIHTTPD_SERVER_H

/* Devuelve 0 al terminar normalmente, o -1 si falla el servidor. */
int server_run(unsigned short port);

#endif
