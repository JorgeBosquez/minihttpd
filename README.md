# MiniHTTPd

Servidor HTTP/1.1 educativo en C para Linux, en desarrollo.

## Estado actual

Etapa 3: multiples clientes con epoll y sockets no bloqueantes.
Mantiene conexiones abiertas y lee sus bytes; todavia no sirve HTTP.

## Compilar en Ubuntu / WSL

Requisitos: GCC, Make y Git.

```bash
make
./minihttpd 8080
make clean
```

El puerto predeterminado es 8080; se admiten valores entre 1 y 65535.
Escucha en todas las interfaces IPv4 (`0.0.0.0`). Ctrl+C termina el proceso;
el sistema operativo libera sus sockets. Esta etapa usa epoll en modo level-triggered y sockets no bloqueantes.

En otra terminal, verifica la conexion con Python:

```bash
python3 -c 'import socket; s = socket.create_connection(("127.0.0.1", 8080)); s.sendall(b"hola"); input("Conexion abierta. Enter para cerrar..."); s.close()'
```

El servidor registra los bytes recibidos y mantiene la conexion abierta hasta
que el cliente cierre. Los datos se descartan en esta etapa: no hay parsing,
respuestas HTTP ni keep-alive HTTP implementados aun. El navegador todavia no
mostrara una pagina. `listen(..., 32)` solicita una cola de conexiones pendientes;
`MAX_EVENTS=64` limita eventos por ronda, no el total de clientes.
Si el puerto esta ocupado, el programa informa el error de bind y termina.

## Pruebas

Ejecutar desde la raiz del proyecto, sin iniciar manualmente otro servidor:

```bash
make
python3 tests/test_tcp.py
python3 tests/test_epoll.py
```

Las pruebas abren sus propios servidores en puertos disponibles. Verifican
argumentos invalidos, puerto ocupado, EOF, ocho conexiones simultaneas,
lecturas repetidas y recepcion de mas bytes que el buffer. El cliente activo
progresa aunque los otros no envien datos. No se han implementado aun limites
de conexiones ni plazos de inactividad.

## Organizacion

- `src/main.c`: entrada del programa y validacion del puerto.
- `src/server.c`, `include/server.h`: sockets, conexiones y bucle epoll.
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
2. Multiples clientes con epoll y sockets no bloqueantes: completado.
3. Parsing GET y encabezados Host, Connection y User-Agent.
4. Archivos estaticos y MIME: HTML, CSS, JS, PNG y JPG.
5. Estados 200, 400, 403, 404, 405 y 500; limites y rutas seguras con realpath.
6. Conexiones persistentes y manejo de lecturas y escrituras parciales.
7. Pruebas integrales y documentacion final.

El informe final se redactara a partir de las pruebas y experiencias reales.
