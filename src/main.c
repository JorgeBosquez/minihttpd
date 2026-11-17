#include "server.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EVENTS 64

static volatile sig_atomic_t stop_requested;

static void request_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

int main(int argc, char *argv[])
{
    unsigned short port = 8080;

    if (argc > 2) {
        fprintf(stderr, "Uso: %s [puerto 1-65535]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 2) {
        /* Validamos digitos antes de convertir: no aceptamos signos ni espacios. */
        if (argv[1][0] == '\0' || strlen(argv[1]) > 5 ||
            strspn(argv[1], "0123456789") != strlen(argv[1])) {
            fprintf(stderr, "Puerto invalido: use un numero entre 1 y 65535.\n");
            return EXIT_FAILURE;
        }
        unsigned long value = strtoul(argv[1], NULL, 10);
        if (value == 0 || value > 65535) {
            fprintf(stderr, "Puerto fuera del rango 1-65535.\n");
            return EXIT_FAILURE;
        }
        port = (unsigned short)value;
    }

    struct sigaction action = {0};
    action.sa_handler = request_stop;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) < 0 ||
        sigaction(SIGTERM, &action, NULL) < 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }
    struct server *server = server_start(port);
    if (!server) return EXIT_FAILURE;

    struct epoll_event events[MAX_EVENTS];
    int result = EXIT_SUCCESS;
    while (!stop_requested) {
        /* El plazo evita esperar indefinidamente si la senal llega justo antes
         * de epoll_wait. No es un plazo de inactividad para los clientes.
         */
        int ready = epoll_wait(server_epoll_fd(server), events, MAX_EVENTS, 1000);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            result = EXIT_FAILURE;
            break;
        }
        for (int i = 0; i < ready && !stop_requested; ++i) {
            if (server_handle_event(server, &events[i]) < 0) {
                result = EXIT_FAILURE;
                stop_requested = 1;
            }
        }
    }
    server_stop(server);
    return result;
}
