# MiniHTTPd

Servidor HTTP/1.1 educativo en C para Linux, en desarrollo.

## Estado actual

Etapa 2: servidor TCP base. Acepta conexiones y las cierra; todavia no sirve HTTP.

## Compilar en Ubuntu / WSL

Requisitos: GCC, Make y Git.

```bash
make
./minihttpd 8080
make clean
```

El puerto predeterminado es 8080; se admiten valores entre 1 y 65535.
Escucha en todas las interfaces IPv4 (`0.0.0.0`). Ctrl+C termina el proceso;
el sistema operativo libera sus sockets. Esta etapa usa accept bloqueante.

En otra terminal, verifica la conexion con Python:

```bash
python3 -c 'import socket; s = socket.create_connection(("127.0.0.1", 8080)); print("Conectado; cierre del servidor:", s.recv(1)); s.close()'
```

El resultado esperado es `b''`, porque el servidor acepta y cierra sin enviar
contenido. El navegador todavia no mostrara una pagina. `listen(..., 32)` establece
la cola solicitada de conexiones pendientes; no limita el total de clientes.
Si el puerto esta ocupado, el programa informa el error de bind y termina.

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

El servidor TCP sigue la secuencia socket/bind/listen de esos ejemplos, con
una implementacion propia documentada en src/server.c. Al adaptar fragmentos se documentara
su procedencia y se conservaran los avisos pertinentes.

## Etapas pendientes

1. Servidor TCP base: completado; continuar con epoll.
2. Multiples clientes con epoll y sockets no bloqueantes.
3. Parsing GET y encabezados Host, Connection y User-Agent.
4. Archivos estaticos y MIME: HTML, CSS, JS, PNG y JPG.
5. Estados 200, 400, 403, 404, 405 y 500; limites y rutas seguras con realpath.
6. Conexiones persistentes y manejo de lecturas y escrituras parciales.
7. Pruebas integrales y documentacion final.

El informe final se redactara a partir de las pruebas y experiencias reales.
