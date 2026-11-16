# MiniHTTPd

Servidor HTTP/1.1 educativo en C para Linux, en desarrollo.

## Estado actual: etapa 6

Servidor con epoll, sockets no bloqueantes y buffers independientes por cliente.
Analiza solicitudes y envia respuestas HTTP reales. Todo GET valido recibe por
ahora el texto `MiniHTTPd: respuesta HTTP de prueba.`; aun no se consulta `www`.
Las rutas inexistentes tambien reciben ese texto durante esta etapa.

## Compilar y ejecutar en Ubuntu / WSL

Requisitos: GCC, Make, Git; Python 3 para las pruebas de red.

```bash
make
./minihttpd 8080
```

Puerto predeterminado 8080; admite 1-65535. Escucha en todas las interfaces IPv4.
Ctrl+C termina el proceso y Linux libera sus recursos. En otra terminal:

```bash
curl -v http://localhost:8080/index.html
curl -v http://localhost:8080/ http://localhost:8080/index.html
curl -v -X POST http://localhost:8080/
curl -v -H 'Connection: close' http://localhost:8080/
```

La segunda orden permite observar reutilizacion de la conexion. POST devuelve
405 con Allow: GET. Un GET mal formado devuelve 400. Los errores cierran despues
del envio; HTTP/1.1 mantiene la conexion por defecto y respeta Connection: close.
Content-Length delimita cada cuerpo. Las respuestas usan CRLF y text/plain.

## Organizacion

- `src/main.c`: entrada y validacion del puerto.
- `src/server.c`, `include/server.h`: sockets, epoll, buffers y envios parciales.
- `src/http.c`, `include/http.h`: parser y respuestas HTTP de texto.
- `src/files.c`, `include/files.h`: acceso a archivos pendiente.
- `src/mime.c`, `include/mime.h`: tipos MIME pendientes.
- `www/`: contenido estatico para una etapa posterior.
- `tests/`: pruebas durante el desarrollo; se retiraran al preparar la entrega.

## Ciclo de cada cliente

Se acumulan bytes hasta completar cabeceras. El parser consume una solicitud;
memmove conserva bytes de la siguiente. Se prepara una respuesta y se vigila
EPOLLOUT, pausando lecturas para no acumular respuestas sin limite. send usa
MSG_NOSIGNAL y guarda el desplazamiento para continuar tras un envio parcial
o EAGAIN. Al terminar se procesa el siguiente mensaje ya recibido o se vuelve
a EPOLLIN. Connection: close termina la sesion despues de enviar el cuerpo.
Una solicitud valida seguida de cierre de escritura del cliente recibe respuesta;
un EOF con cabeceras incompletas recibe 400 y cierre.

Se usa epoll level-triggered. MAX_EVENTS=64 limita eventos por ronda, no clientes.
listen(..., 32) solicita la cola de conexiones pendientes. No hay limite global
de conexiones ni plazos de inactividad todavia.

## Parser y limites

HTTP/1.1, GET y rutas que empiezan por `/`. Host obligatorio y unico; se leen
Connection y User-Agent. Los nombres de cabecera no distinguen mayusculas.
No admite HTTP/1.0, absolute-form, cuerpos ni Transfer-Encoding. Content-Length
solo puede indicar cero. La validacion completa de autoridad Host esta pendiente.

Limites: 16 KiB de cabeceras, 4096 bytes por linea contando CR, URI de 2048 bytes,
Host y User-Agent de 1024 bytes. Solicitudes que exceden los limites dan 400.
Referencia: https://www.rfc-editor.org/rfc/rfc9112.html

## Pruebas

Desde la raiz, sin iniciar manualmente otro servidor:

```bash
make
make test-http
python3 tests/test_tcp.py
python3 tests/test_epoll.py
make test-partial
```

Las pruebas de red arrancan y detienen sus propios servidores. Verifican clientes
inactivos, fragmentacion, pipeline, cuerpos y Content-Length, 400/405, cierres,
EOF y desconexion abrupta. test-partial compila un ejecutable separado que limita
send a siete bytes e inyecta EAGAIN; ejecuta las mismas pruebas para comprobar
que los encabezados y cuerpos llegan completos y en orden. El binario normal
no incluye esa instrumentacion.

## Referencias de los ejemplos

Comer y Stevens, Internetworking with TCP/IP, volumen III, distribucion Linux/POSIX,
carpeta local V3.linux/V3.linux/examples:

- passiveTCP.c y passivesock.c: secuencia socket/bind/listen.
- TCPmechod.c: multiplexacion con select como referencia para epoll.
- TCPechod.c: conexiones y transferencia de bytes.

Implementacion propia basada en el estudio de esas ideas. Al adaptar fragmentos
se documentara la procedencia y se conservaran los avisos pertinentes.

## Pendiente

Archivos estaticos, MIME, errores 403/404/500, proteccion de rutas con realpath,
revision final de limites y seguridad, documentacion e informe de una pagina.
El informe reflejara las pruebas y experiencias reales del proyecto.
