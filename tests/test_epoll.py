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
    for payload in (b"hola", b"segunda lectura"):
        active.sendall(payload)
        assert f"recibidos {len(payload)} bytes" in line()
    print("OK: ocho clientes abiertos; el ultimo progresa con los otros inactivos")
    active.sendall(b"x" * 20000)
    total = 0
    while total < 20000:
        entry = line()
        total += int(entry.split("recibidos ")[1].split()[0])
    assert total == 20000
    print("OK: datos mayores que el buffer se consumen en varios eventos")
    active.shutdown(socket.SHUT_WR)
    assert active.recv(1) == b""
    with socket.create_connection(("127.0.0.1", port), timeout=3) as new:
        assert "aceptada" in line()
        new.sendall(b"nuevo")
        assert "recibidos 5 bytes" in line()
    assert process.poll() is None
    print("OK: cierre por EOF y aceptacion posterior")
finally:
    for client in clients:
        client.close()
    process.terminate()
    process.communicate(timeout=3)
