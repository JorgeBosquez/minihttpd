#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#define LISTEN_BACKLOG 32

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

int server_run(unsigned short port)
{
    int listener = create_listener(port);
    if (listener < 0)
        return -1;

    printf("MiniHTTPd: TCP escuchando en 0.0.0.0:%u (Ctrl+C para salir).\n",
           (unsigned int)port);
    fflush(stdout);

    for (;;) {
        /* accept crea otro descriptor: el listener sigue disponible. */
        int client = accept(listener, NULL, NULL);
        if (client < 0) {
            if (errno == EINTR)
                continue;
            perror("accept");
            close(listener);
            return -1;
        }
        printf("Conexion TCP aceptada; descriptor %d.\n", client);
        fflush(stdout);
        /* Etapa TCP: aun no leemos ni respondemos solicitudes HTTP. */
        close(client);
    }
}
