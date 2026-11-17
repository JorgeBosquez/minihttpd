# MiniHTTPd

Servidor HTTP/1.1 basico en C para Linux, sin bibliotecas HTTP externas.

## Funcionalidad

Servidor con epoll, sockets no bloqueantes y buffers independientes por cliente.
Analiza solicitudes y sirve archivos estaticos de www con tipos MIME.
La ruta / entrega www/index.html. Los archivos inexistentes devuelven 404.

## Compilar y ejecutar en Ubuntu / WSL

Requisitos: GCC y Make en Linux. Git para gestionar el repositorio y curl
para las comprobaciones manuales. Python no es necesario.

```bash
make
./minihttpd 8080
```

Puerto predeterminado 8080; admite 1-65535. Escucha en todas las interfaces IPv4.
Ctrl+C o SIGTERM solicitan una parada ordenada: se cierran clientes, archivos
y epoll y se libera la memoria. En otra terminal:

```bash
curl -v http://localhost:8080/index.html
curl -v http://localhost:8080/ http://localhost:8080/index.html
curl -v -X POST http://localhost:8080/
curl -v -H 'Connection: close' http://localhost:8080/
```

Abre http://localhost:8080/ en el navegador para ver HTML, CSS y la imagen.
La segunda orden permite observar reutilizacion de la conexion. POST devuelve
405 con Allow: GET. Un GET mal formado devuelve 400. Los errores cierran despues
del envio; HTTP/1.1 mantiene la conexion por defecto y respeta Connection: close.
Content-Length delimita cada cuerpo. Las respuestas usan CRLF, el tipo MIME del archivo y text/plain para errores.

## Organizacion

- `src/main.c`: argumentos, senales y bucle epoll_wait.
- `src/server.c`, `include/server.h`: sockets, registro y despacho de eventos,
  buffers y envios parciales.
- `src/http.c`, `include/http.h`: parser, encabezados y respuestas de error.
- `src/files.c`, `include/files.h`: apertura segura de archivos regulares.
- `src/mime.c`, `include/mime.h`: tipos MIME por extension.
- `www/`: contenido estatico servido por el programa.

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
solo puede indicar cero. Host admite nombres DNS/IPv4 y literales IPv6 entre corchetes, con puerto
numerico opcional de 0 a 65535. Se rechazan hosts vacios, signos no admitidos,
IPv6 mal formado y puertos invalidos; es un subconjunto de autoridad URI.

Limites: 16 KiB de cabeceras, 4096 bytes por linea contando CR, URI de 2048 bytes,
Host y User-Agent de 1024 bytes. Solicitudes que exceden los limites dan 400.
Referencia: https://www.rfc-editor.org/rfc/rfc9112.html

## Comprobacion manual

Inicia el servidor desde la raiz del proyecto con `./minihttpd 8080`.
En otra terminal ejecuta:

```bash
# 200 y el HTML de www/index.html
curl -i http://localhost:8080/
# 200 y Content-Type: text/css
curl -i http://localhost:8080/style.css
# 404
curl -i http://localhost:8080/no-existe.html
# 405 y Allow: GET
curl -i -X POST http://localhost:8080/
# 400 por falta de Host
curl -i -H 'Host:' http://localhost:8080/
# 403 sin normalizar la ruta en curl
curl --path-as-is -i http://localhost:8080/../../etc/passwd
# Reutilizacion de conexion: buscar Reusing existing en la salida
curl -v http://localhost:8080/ http://localhost:8080/style.css
```

El estado 500 se devuelve ante fallos internos, por ejemplo si falta www.
Para observar varios clientes, abre varias terminales con `curl` o ventanas del
navegador. Esa comprobacion simple no sustituye las pruebas de solicitudes
fragmentadas y envios parciales realizadas durante el desarrollo.

Las pruebas automatizadas usadas durante el desarrollo estan en el historial
hasta el commit 0a8f273. Se retiraron de la version de entrega. No son necesarias
para compilar o ejecutar el servidor. Para estudiarlas sin modificar tu carpeta:

```bash
git show 0a8f273:tests/test_epoll.py
```

## Referencias de los ejemplos

Comer y Stevens, Internetworking with TCP/IP, volumen III, distribucion Linux/POSIX,
carpeta local V3.linux/V3.linux/examples:

- passiveTCP.c y passivesock.c: secuencia socket/bind/listen.
- TCPmechod.c: multiplexacion con select como referencia para epoll.
- TCPechod.c: conexiones y transferencia de bytes.

Implementacion propia basada en el estudio de esas ideas. Al adaptar fragmentos
se documentara la procedencia y se conservaran los avisos pertinentes.

## Pendiente

Informe de una pagina con experiencias y conclusiones del estudiante y URL
del repositorio: https://github.com/JorgeBosquez/minihttpd.
El informe reflejara las pruebas y experiencias reales del proyecto.

## Archivos estaticos y MIME

Ejecuta el programa desde la raiz del proyecto: www se resuelve respecto al
directorio de trabajo. No publica otros directorios ni genera listados.
El query se ignora al buscar archivos; se decodifican escapes %XX una sola vez.
Se rechazan escapes invalidos, NUL, controles y barras invertidas (400),
segmentos .. y enlaces simbolicos (403). realpath comprueba que la ruta resuelta
pertenece a www usando el separador para evitar confundir www con www-other.
La apertura posterior usa openat y O_NOFOLLOW para los componentes de la ruta.
No se permite servir enlaces, aunque apunten dentro de www.

Solo se sirven archivos regulares. Cada archivo se abre para la solicitud y se
cierra al terminar o al desconectarse el cliente. fstat determina Content-Length;
se lee y envia por bloques de 16 KiB sin cargar todo el archivo en memoria.
Si el archivo se trunca despues de enviar encabezados, se cierra la conexion.
Se recomienda mantener www estable durante el servicio; no se admite un arbol
modificado de manera hostil por otros procesos. Las lecturas de disco son
sincronicas: epoll controla sockets y no elimina posibles esperas del disco.

MIME: .html text/html, .css text/css, .js application/javascript, .png image/png,
.jpg/.jpeg image/jpeg; otras extensiones application/octet-stream. La comparacion
de extensiones no distingue mayusculas. No se detecta el tipo por el contenido.

Estados: 200 archivo servido, 400 solicitud/ruta invalida, 403 acceso prohibido,
404 archivo inexistente, 405 metodo no admitido, 500 fallo interno. Los errores
cierran la conexion despues de enviar su cuerpo. Sin www se devuelve 500.

```bash
curl -i http://localhost:8080/style.css
curl -i http://localhost:8080/no-existe.html
curl --path-as-is -i http://localhost:8080/../../etc/passwd
```

Se espera 200 con text/css, 404 y 403 respectivamente. --path-as-is impide que
curl normalice la ruta antes de enviarla y permite probar el rechazo real.

## Revision de arquitectura

main coordina epoll_wait y delega cada evento a server_handle_event. server
mantiene su estado privado y ofrece inicio, descriptor epoll y parada. Los
manejadores SIGINT/SIGTERM solo marcan una bandera sig_atomic_t: la liberacion
se realiza en el flujo normal. epoll_wait espera como maximo un segundo para
que una senal recibida justo antes de esperar no retrase indefinidamente la
salida. Este plazo no limita el tiempo de inactividad de los clientes.
