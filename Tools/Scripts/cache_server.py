#!/usr/bin/env python3
"""
Hyperion Cache Server

Serves cooked cache files (blob storage .bin files, toc.bin, and cook_manifest.hmf)
to remote clients (e.g. an Android app) over HTTP. The client fetches the manifest
first, compares timestamps, then downloads only the files it needs.

Usage:
    python cache_server.py -d <cache_directory> [-p 8080] [-b 0.0.0.0]
"""

import argparse
import http.server
import os
import sys


class CacheHandler(http.server.SimpleHTTPRequestHandler):
    """Serves cache files with path-traversal protection."""

    cache_dir: str = ""

    def do_GET(self):
        if self.path == "/manifest":
            self._serve_file("cook_manifest.hmf", "application/octet-stream")
        elif self.path.startswith("/file/"):
            filename = self.path[len("/file/"):]
            self._serve_file(filename, "application/octet-stream")
        else:
            self.send_error(404, "Not Found")

    def _serve_file(self, filename, content_type):
        safe_path = os.path.normpath(os.path.join(self.cache_dir, filename))
        if not safe_path.startswith(os.path.normpath(self.cache_dir)):
            self.send_error(403, "Forbidden")
            return

        if not os.path.exists(safe_path) or not os.path.isfile(safe_path):
            self.send_error(404, "File Not Found")
            return

        try:
            file_size = os.path.getsize(safe_path)
            with open(safe_path, "rb") as f:
                self.send_response(200)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(file_size))
                self.end_headers()
                self.wfile.write(f.read())
        except OSError:
            self.send_error(500, "Internal Server Error")

    def log_message(self, format, *args):
        print(f"[{self.log_date_time_string()}] {args[0]}", flush=True)


def main():
    parser = argparse.ArgumentParser(description="Hyperion Cache Server")
    parser.add_argument("-d", "--cache-dir", required=True,
                        help="Path to the cooked cache directory")
    parser.add_argument("-p", "--port", type=int, default=8080,
                        help="Port to listen on (default: 8080)")
    parser.add_argument("-b", "--bind", default="0.0.0.0",
                        help="Address to bind (default: 0.0.0.0)")
    args = parser.parse_args()

    cache_dir = os.path.abspath(args.cache_dir)
    if not os.path.isdir(cache_dir):
        print(f"Error: cache directory does not exist: {cache_dir}", file=sys.stderr)
        sys.exit(1)

    manifest_path = os.path.join(cache_dir, "cook_manifest.hmf")
    if not os.path.isfile(manifest_path):
        print(f"Warning: no cook_manifest.hmf found in {cache_dir} — run the cook commandlet first",
              file=sys.stderr)

    CacheHandler.cache_dir = cache_dir

    server = http.server.ThreadingHTTPServer((args.bind, args.port), CacheHandler)
    print(f"Cache server listening on http://{args.bind}:{args.port}")
    print(f"Serving files from: {cache_dir}")
    print("Press Ctrl+C to stop.")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down.")
        server.shutdown()


if __name__ == "__main__":
    main()
