# MiniHTTPd

Servidor HTTP/1.1 en C para Linux. Sirve archivos de `www` usando un solo hilo,
`epoll`, sockets no bloqueantes y conexiones persistentes.

## Compilar y ejecutar

Requisitos: Linux o Ubuntu/WSL, GCC y Make. Para las pruebas manuales: curl.
Ejecutar desde la raíz del proyecto:

```bash
make
./minihttpd 8080
```

Abrir http://localhost:8080/ en el navegador. `/` entrega `www/index.html`.
Puerto predeterminado: 8080. Detener con Ctrl+C. `make clean` elimina la compilación.

## Solicitudes

Acepta GET sobre HTTP/1.1. Por requisito de clase, exige `Host`, `Connection`
y `User-Agent`, con valores no vacíos. Si falta alguna, responde 400.
`Connection: keep-alive` permite reutilizar la conexión; `close` la cierra
tras enviar la respuesta. Los errores también cierran la conexión.

## Pruebas

Con el servidor activo, ejecutar en otra terminal. Curl añade `Host` y
`User-Agent`; estos comandos agregan `Connection` cuando corresponde.

```bash
# 200: página HTML
curl -i -H 'Connection: keep-alive' http://localhost:8080/

# 404: archivo inexistente
curl -i -H 'Connection: keep-alive' http://localhost:8080/no-existe.html

# 405: método no permitido
curl -i -H 'Connection: keep-alive' -X POST http://localhost:8080/

# 400: falta Connection
curl -i http://localhost:8080/

# 403: acceso fuera de www
curl --path-as-is -i -H 'Connection: keep-alive' http://localhost:8080/../../etc/passwd

# Persistencia: buscar "Reusing existing" en la salida
curl -v -H 'Connection: keep-alive' http://localhost:8080/ http://localhost:8080/style.css
```

El estado 500 indica un fallo interno, por ejemplo, si falta `www`.

## Alcance

- MIME: HTML, CSS, JS, PNG y JPG/JPEG; otros: `application/octet-stream`.
- Límites: 16 KiB de línea inicial y cabeceras, URI de 2048 bytes, línea de
  4096 bytes contando CR, Host y User-Agent de 1024 bytes. Excederlos produce 400.
- Rechaza rutas fuera de `www`, segmentos `..` y enlaces simbólicos.
- Sin HTTP/1.0, cuerpos de solicitud, TLS ni listado de directorios.
- Sin límite global de conexiones ni tiempo de inactividad. Lecturas de disco síncronas.

## Archivos

`main.c`: bucle epoll y señales. `server.c`: conexiones y envíos.
`http.c`: solicitudes y respuestas. `files.c`: acceso a archivos.
`mime.c`: tipos MIME. Código en `src/`, cabeceras en `include/`, contenido en `www/`.

Referencia de estudio: ejemplos Linux/POSIX de Comer y Stevens,
*Internetworking with TCP/IP*, volumen III (`passivesock.c`, `passiveTCP.c` y `TCPmechod.c`).
