"""Pruebas de la etapa TCP. Ejecutar desde la raiz: python3 tests/test_tcp.py."""
import socket
import subprocess

for args in (["0"], ["65536"], ["-1"], ["abc"], [""], ["8080", "extra"]):
    result = subprocess.run(["./minihttpd", *args], capture_output=True, timeout=3)
    assert result.returncode != 0, args
print("OK: puertos y argumentos invalidos")

# Reservamos un puerto para comprobar el error de bind.
with socket.socket() as reserved:
    reserved.bind(("127.0.0.1", 0))
    reserved.listen()
    port = reserved.getsockname()[1]
    result = subprocess.run(["./minihttpd", str(port)], capture_output=True, timeout=3)
    assert result.returncode != 0 and b"bind" in result.stderr
print("OK: puerto ocupado")

# Elegimos un puerto disponible y comprobamos dos conexiones consecutivas.
with socket.socket() as probe:
    probe.bind(("127.0.0.1", 0))
    port = probe.getsockname()[1]
process = subprocess.Popen(["./minihttpd", str(port)], stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, text=True)
try:
    # readline tiene un plazo mediante select para detectar fallos al arrancar.
    import select
    assert select.select([process.stdout], [], [], 3)[0], "Sin mensaje de inicio"
    assert "escuchando" in process.stdout.readline()
    for _ in range(2):
        with socket.create_connection(("127.0.0.1", port), timeout=3) as client:
            assert client.recv(1) == b"", "Se esperaba cierre sin respuesta HTTP"
    assert process.poll() is None, "El servidor termino tras aceptar un cliente"
    print("OK: dos conexiones aceptadas y cerradas; servidor sigue activo")
finally:
    process.terminate()
    process.communicate(timeout=3)
