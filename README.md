# MiniHTTPd

Servidor HTTP/1.1 educativo en C para Linux, en desarrollo.

## Estado actual

Etapa 3: multiples clientes con epoll y sockets no bloqueantes.
Etapa 5: cada cliente acumula bytes y analiza solicitudes HTTP.
Los resultados se registran en terminal; todavia no se envian respuestas HTTP.

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
python3 -c 'import socket; s = socket.create_connection(("127.0.0.1", 8080)); s.sendall(b"GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"); input("Conexion abierta. Enter para cerrar..."); s.close()'
```

El servidor registra el resultado del parser. Conserva las solicitudes
incompletas por cliente y los bytes sobrantes de otra solicitud. Aun no envia
respuestas HTTP. Los errores y Connection: close cierran la conexion; en una
etapa posterior el cierre se hara despues de enviar la respuesta. El navegador todavia no
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

## Parser HTTP (etapa 4)

`http_parse` en `src/http.c` recibe bytes y longitud, sin requerir terminador NUL.
Se prueba por separado y tambien se invoca desde el bucle epoll con un buffer
independiente por cliente.
No se envian respuestas HTTP en esta etapa.

- Resultado 0: solicitud incompleta; el futuro llamador acumulara mas bytes.
- Resultado 200: GET valido; entrega URI, Host, User-Agent y keep_alive.
- Resultado 400: sintaxis invalida o limites excedidos.
- Resultado 405: metodo distinto de GET con estructura valida.

Estos resultados son valores internos, aun no respuestas enviadas por red.
Se admite HTTP/1.1 y rutas origin-form que empiezan por `/`. No se implementan
HTTP/1.0, absolute-form ni cuerpos de solicitudes. Se rechaza Transfer-Encoding y
Content-Length distinto de cero. Host es obligatorio y no se permiten duplicados;
la validacion completa de la autoridad Host no esta implementada. Los encabezados
no reconocidos se ignoran despues de validar su sintaxis. Connection: close
prevalece, incluso si aparecen varios encabezados Connection.

Limites: 16 KiB de cabeceras, 4096 bytes por linea contando CR, URI de 2048 bytes,
Host y User-Agent de 1024 bytes cada uno. Los limites producen resultado 400.
El parser devuelve cuantos bytes pertenecen a la primera solicitud para que el
servidor pueda conservar una segunda solicitud recibida en la misma lectura.
La acumulacion por cliente esta implementada; el envio no bloqueante sigue pendiente.

```bash
make test-http
```

Esta prueba esta escrita en C. Comprueba solicitudes fragmentadas, campos,
metodos, errores, limites y bytes sobrantes. Referencia de sintaxis:
https://www.rfc-editor.org/rfc/rfc9112.html

## Integracion del parser con epoll (etapa 5)

Cada cliente tiene descriptor, buffer de hasta 16 KiB y cantidad de bytes usados.
`epoll_event.data.ptr` identifica ese estado sin usar el descriptor como indice.
Una lista enlazada registra los clientes para liberar sus buffers al cerrar o
si falla el bucle de eventos. No hay limite global de conexiones aun.

Al recibir datos, el parser devuelve incompleto, valido o error. Una solicitud
valida consume solo sus bytes; memmove conserva el resto para la siguiente.
No se procesan solicitudes posteriores a Connection: close. Los estados HTTP
se muestran en stdout, sin enviarlos por red. Al finalizar esta etapa, curl y
el navegador aun no pueden obtener una respuesta HTTP.

`python3 tests/test_epoll.py` verifica buffers independientes, fragmentacion,
solicitudes consecutivas, errores 400/405, limites, cabeceras grandes y cierres.
