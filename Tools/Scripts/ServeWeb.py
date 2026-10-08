"""Serves a web build for local testing.

    python ServeWeb.py <binaries dir> <package dir> [port]

The binaries dir holds index.html and hyperion-sample.{js,wasm,data}; the package dir holds the cooked Cache/.
Sends the cross-origin isolation headers threads need, and answers range requests, which the cache files are read with.
"""

import http.server
import os
import re
import sys

binaries_dir = os.path.abspath(sys.argv[1])
package_dir = os.path.abspath(sys.argv[2])
port = int(sys.argv[3]) if len(sys.argv) > 3 else 8080

content_types = {
    ".html": "text/html",
    ".js": "text/javascript",
    ".wasm": "application/wasm",
}


def root_is_package(full_path):
    return os.path.commonpath([full_path, package_dir]) == package_dir


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def resolve(self):
        path = self.path.split("?", 1)[0].lstrip("/") or "index.html"

        if ".." in path.split("/"):
            return None

        root = package_dir if path.startswith("Cache/") else binaries_dir
        full_path = os.path.join(root, *path.split("/"))

        return full_path if os.path.isfile(full_path) else None

    def respond(self, send_body):
        full_path = self.resolve()

        if full_path is None:
            self.send_response(404)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return

        size = os.path.getsize(full_path)
        start, end = 0, size - 1
        range_match = re.fullmatch(r"bytes=(\d+)-(\d*)", self.headers.get("Range", ""))

        if range_match and size > 0:
            start = min(int(range_match.group(1)), size - 1)
            end = min(int(range_match.group(2)), size - 1) if range_match.group(2) else size - 1
            self.send_response(206)
            self.send_header("Content-Range", "bytes %d-%d/%d" % (start, end, size))
        else:
            self.send_response(200)

        length = end - start + 1 if size > 0 else 0

        if send_body and root_is_package(full_path):
            print("%s %d-%d of %d" % (os.path.basename(full_path), start, end, size), flush=True)

        self.send_header("Content-Type", content_types.get(os.path.splitext(full_path)[1], "application/octet-stream"))
        self.send_header("Content-Length", str(length))
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()

        if not send_body:
            return

        with open(full_path, "rb") as file:
            file.seek(start)
            remaining = length

            while remaining > 0:
                chunk = file.read(min(remaining, 1 << 20))

                if not chunk:
                    break

                self.wfile.write(chunk)
                remaining -= len(chunk)

    def do_GET(self):
        self.respond(True)

    def do_HEAD(self):
        self.respond(False)

    def log_message(self, format, *args):
        pass


print("Serving %s (package %s) at http://127.0.0.1:%d/" % (binaries_dir, package_dir, port))
http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
