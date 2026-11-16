#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    return server_run(port) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
