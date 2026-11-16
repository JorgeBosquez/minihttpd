# MiniHTTPd

Servidor HTTP/1.1 educativo en C para Linux, en desarrollo.

## Estado actual

Etapa 1: estructura y programa inicial. Todavia no abre sockets ni sirve HTTP.

## Compilar en Ubuntu / WSL

Requisitos: GCC, Make y Git.

```bash
make
./minihttpd
make clean
```

El programa inicial imprime un mensaje y termina.

## Organizacion

- `src/main.c`: entrada del programa; coordinara el bucle epoll.
- `src/server.c`, `include/server.h`: sockets y conexiones.
- `src/http.c`, `include/http.h`: solicitudes y respuestas HTTP.
- `src/files.c`, `include/files.h`: acceso seguro a archivos.
- `src/mime.c`, `include/mime.h`: tipos de contenido.
- `www/`: contenido estatico de prueba.

## Referencias de codigo

Se estudiaran los ejemplos de Comer y Stevens de la carpeta local
`V3.linux/V3.linux/examples` (distribucion Linux/POSIX de
Internetworking with TCP/IP, volumen III):

- `passiveTCP.c`, `passivesock.c`: socket de escucha.
- `TCPmechod.c`: multiplexacion con select, para entender la transicion a epoll.
- `TCPechod.c`: aceptar conexiones y transferir bytes.

Esta etapa no copia codigo de esos archivos. Al adaptar fragmentos se documentara
su procedencia y se conservaran los avisos pertinentes.

## Etapas pendientes

1. Servidor TCP base.
2. Multiples clientes con epoll y sockets no bloqueantes.
3. Parsing GET y encabezados Host, Connection y User-Agent.
4. Archivos estaticos y MIME: HTML, CSS, JS, PNG y JPG.
5. Estados 200, 400, 403, 404, 405 y 500; limites y rutas seguras con realpath.
6. Conexiones persistentes y manejo de lecturas y escrituras parciales.
7. Pruebas integrales y documentacion final.

El informe final se redactara a partir de las pruebas y experiencias reales.
