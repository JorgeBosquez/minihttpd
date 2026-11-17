"""Archivos, MIME, traversal y transferencias por bloques en entorno temporal."""
import http.client
import os
from pathlib import Path
import select
import socket
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else "./minihttpd").resolve())
with tempfile.TemporaryDirectory(prefix="minihttpd-files-") as temporary:
    base = Path(temporary)
    root = base / "www"
    root.mkdir()
    fixtures = {
        "index.html": (b"<h1>MiniHTTPd</h1>", "text/html"),
        "style.css": (b"body { color: blue; }", "text/css"),
        "app.js": (b"console.log(1);", "application/javascript"),
        "image.png": (Path("www/image.png").read_bytes(), "image/png"),
        "photo.jpg": (b"\xff\xd8\x00\xff\xd9", "image/jpeg"),
        "upper.JPG": (b"jpg", "image/jpeg"),
        "empty.html": (b"", "text/html"),
        "a b.html": (b"space", "text/html"),
        "large.bin": (bytes(range(256)) * 2000, "application/octet-stream"),
    }
    for name, (data, _) in fixtures.items():
        (root / name).write_bytes(data)
    (base / "secret").write_bytes(b"SECRET")
    (base / "www-other").mkdir()
    (base / "www-other" / "secret").write_bytes(b"SECRET")
    (root / "outside").symlink_to(base / "secret")
    (root / "internal").symlink_to(root / "index.html")
    (root / "prefix").symlink_to(base / "www-other", target_is_directory=True)
    (root / "folder").mkdir()
    os.mkfifo(root / "pipe")
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    process = subprocess.Popen([binary, str(port)], cwd=base,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
    def get(path, status, expected=None, mime=None):
        connection.request("GET", path)
        response = connection.getresponse()
        body = response.read()
        assert response.status == status, (path, response.status)
        assert len(body) == int(response.getheader("Content-Length"))
        if expected is not None: assert body == expected, path
        if mime: assert response.getheader("Content-Type") == mime, path
        if status != 200: assert b"SECRET" not in body
    try:
        assert select.select([process.stdout], [], [], 3)[0]
        assert b"escuchando" in process.stdout.readline()
        for name, (data, mime) in fixtures.items():
            get("/" + name.replace(" ", "%20"), 200, data, mime)
        get("/?q=1", 200, fixtures["index.html"][0], "text/html")
        print("OK: archivos binarios, MIME, query, espacios, vacios y archivo de 512000 bytes")
        for path in ("/../secret", "/%2e%2e/secret", "/%2e%2e%2fsecret", "/outside",
                     "/internal", "/prefix/secret", "/folder", "/pipe"):
            get(path, 403)
        get("/missing.html", 404)
        for path in ("/%00.html", "/%GG", "/%", "/%5csecret"):
            get(path, 400)
        print("OK: 400/403/404, traversal, enlaces externos/internos y archivo no regular")
        root.rename(base / "saved-www")
        get("/", 500)
        print("OK: 500 si falta la raiz de contenido")
    finally:
        connection.close()
        process.terminate()
        process.communicate(timeout=3)
