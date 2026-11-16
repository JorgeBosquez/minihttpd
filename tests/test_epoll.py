"""Pruebas HTTP por red. Opcional: ruta a otro ejecutable como argumento."""
import select
import socket
import struct
import subprocess
import sys

binary = sys.argv[1] if len(sys.argv) > 1 else "./minihttpd"
with socket.socket() as probe:
    probe.bind(("127.0.0.1", 0))
    port = probe.getsockname()[1]
process = subprocess.Popen([binary, str(port)], stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE)
clients = []

def connect():
    client = socket.create_connection(("127.0.0.1", port), timeout=3)
    clients.append(client)
    return client

class Reader:
    def __init__(self, sock):
        self.sock, self.pending = sock, b""

    def response(self, expected, close=False):
        while b"\r\n\r\n" not in self.pending:
            data = self.sock.recv(4096)
            assert data, "EOF antes de los encabezados"
            self.pending += data
        head, self.pending = self.pending.split(b"\r\n\r\n", 1)
        lines = head.split(b"\r\n")
        assert lines[0] == f"HTTP/1.1 {expected}".encode(), lines[0]
        headers = dict(line.split(b": ", 1) for line in lines[1:])
        size = int(headers[b"Content-Length"])
        assert headers[b"Connection"] == (b"close" if close else b"keep-alive")
        assert headers[b"Content-Type"] == b"text/plain; charset=utf-8"
        if expected.startswith("405"):
            assert headers[b"Allow"] == b"GET"
        while len(self.pending) < size:
            data = self.sock.recv(4096)
            assert data, "EOF antes del cuerpo"
            self.pending += data
        body, self.pending = self.pending[:size], self.pending[size:]
        expected_body = (b"MiniHTTPd: respuesta HTTP de prueba.\n" if expected.startswith("200")
                         else expected.encode() + b"\n")
        assert body == expected_body, body
        if close:
            assert not self.pending and self.sock.recv(1) == b""

request = b"GET /index.html HTTP/1.1\r\nHost: local\r\n\r\n"
try:
    assert select.select([process.stdout], [], [], 3)[0]
    assert b"escuchando" in process.stdout.readline()
    idle = [connect() for _ in range(8)]
    active = idle[-1]
    reader = Reader(active)
    active.sendall(request[:-2])
    assert not select.select([active], [], [], 0.1)[0], "Respuesta a solicitud incompleta"
    other = idle[0]
    other.sendall(request)
    Reader(other).response("200 OK")
    active.sendall(request[-2:])
    reader.response("200 OK")
    print("OK: respuesta real y progreso con otros clientes inactivos")
    active.sendall(request * 3)
    for _ in range(3):
        reader.response("200 OK")
    active.sendall(request)
    reader.response("200 OK")
    print("OK: pipeline en orden y reutilizacion posterior de conexion")
    cases = [
        (b"GET / HTTP/1.1\r\n\r\n", "400 Bad Request"),
        (b"POST / HTTP/1.1\r\nHost: local\r\n\r\n", "405 Method Not Allowed"),
        (b"x" * 4097, "400 Bad Request"),
        (b"GET / HTTP/1.1\r\nHost: local\r\nConnection: close\r\n\r\n", "200 OK"),
    ]
    for payload, status in cases:
        client = connect()
        client.sendall(payload)
        Reader(client).response(status, close=True)
    print("OK: 400, 405 con Allow y cierre despues de respuesta completa")
    client = connect()
    client.sendall(request[:-2])
    client.shutdown(socket.SHUT_WR)
    Reader(client).response("400 Bad Request", close=True)
    client = connect()
    client.sendall(request)
    client.shutdown(socket.SHUT_WR)
    Reader(client).response("200 OK")
    assert client.recv(1) == b""
    print("OK: EOF con solicitud incompleta y media conexion cerrada")
    broken = connect()
    broken.sendall(request)
    broken.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
    broken.close()
    client = connect()
    client.sendall(request)
    Reader(client).response("200 OK")
    assert process.poll() is None
    print("OK: servidor sigue atendiendo tras desconexion abrupta")
finally:
    for client in clients:
        client.close()
    process.terminate()
    process.communicate(timeout=3)
