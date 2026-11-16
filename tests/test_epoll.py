"""Comprueba progreso con clientes inactivos y lecturas repetidas."""
import os
import select
import socket
import subprocess
import time

with socket.socket() as probe:
    probe.bind(("127.0.0.1", 0))
    port = probe.getsockname()[1]
process = subprocess.Popen(["./minihttpd", str(port)], stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE)
clients = []
pending = b""

def line():
    global pending
    deadline = time.monotonic() + 3
    while b"\n" not in pending:
        remaining = deadline - time.monotonic()
        assert remaining > 0 and select.select([process.stdout], [], [], remaining)[0], "Servidor no progresa"
        data = os.read(process.stdout.fileno(), 4096)
        assert data, "Servidor termino inesperadamente"
        pending += data
    result, pending = pending.split(b"\n", 1)
    return result.decode()

try:
    assert "epoll escuchando" in line()
    for _ in range(8):
        clients.append(socket.create_connection(("127.0.0.1", port), timeout=3))
        assert "aceptada" in line()
    # Todos siguen abiertos sin datos, incluido el primero.
    for client in clients:
        client.settimeout(0.1)
        try:
            client.recv(1)
            raise AssertionError("El servidor cerro un cliente inactivo")
        except socket.timeout:
            pass
        client.settimeout(3)
    active = clients[-1]

    def until(text):
        # Una lectura TCP puede contener varios fragmentos; buscamos el evento.
        for _ in range(1000):
            entry = line()
            if text in entry:
                return entry
        raise AssertionError("Evento no encontrado: " + text)

    active.sendall(b"GET /fragmentada HTTP/1.1\r\nHost: local\r\n")
    until("solicitud incompleta")
    # Otro cliente puede completar una solicitud mientras active esta incompleto.
    clients[0].sendall(b"GET /otro HTTP/1.1\r\nHost: local\r\n\r\n")
    until("HTTP 200 GET /otro;")
    active.sendall(b"\r\n")
    until("HTTP 200 GET /fragmentada;")
    print("OK: buffers independientes y progreso con solicitud fragmentada")

    active.sendall(b"GET /uno HTTP/1.1\r\nHost: local\r\n\r\n"
                   b"GET /dos HTTP/1.1\r\nHost: local\r\n\r\n")
    until("HTTP 200 GET /uno;")
    until("HTTP 200 GET /dos;")
    print("OK: dos solicitudes en la misma conexion sin perder sobrantes")

    clients[1].sendall(b"GET / HTTP/1.1\r\n\r\n")
    until("HTTP 400")
    assert clients[1].recv(1) == b""
    clients[2].sendall(b"POST / HTTP/1.1\r\nHost: local\r\n\r\n")
    until("HTTP 405")
    assert clients[2].recv(1) == b""
    clients[3].sendall(b"x" * 4097)
    until("HTTP 400")
    assert clients[3].recv(1) == b""
    clients[4].sendall(b"GET /cierre HTTP/1.1\r\nHost: local\r\nConnection: close\r\n\r\n")
    until("HTTP 200 GET /cierre;")
    assert clients[4].recv(1) == b""
    print("OK: errores, limites y Connection close")

    # Cabeceras validas mayores que el antiguo buffer de lectura de 4096 bytes.
    active.sendall(b"GET /grande HTTP/1.1\r\nHost: local\r\n" + b"X: a\r\n" * 800 + b"\r\n")
    until("HTTP 200 GET /grande;")
    active.shutdown(socket.SHUT_WR)
    assert active.recv(1) == b""
    with socket.create_connection(("127.0.0.1", port), timeout=3) as new:
        until("aceptada")
        new.sendall(b"GET /nuevo HTTP/1.1\r\nHost: local\r\n\r\n")
        until("HTTP 200 GET /nuevo;")
    assert process.poll() is None
    print("OK: cabeceras grandes, EOF y aceptacion posterior")
finally:
    for client in clients:
        client.close()
    process.terminate()
    process.communicate(timeout=3)
