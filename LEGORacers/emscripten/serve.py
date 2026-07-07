#!/usr/bin/env python3
# Local test server for the Emscripten build.
#
# The WASMFS fetch backend streams game data with HTTP Range requests and needs 206 +
# Content-Range responses (emrun's plain server answers 200 with the whole file, which
# the patched fetch backend rejects). This server adds single-range support, and sets
# the cross-origin isolation headers (COOP/COEP/CORP) that SharedArrayBuffer/pthreads
# require. Serve the build directory (with the game data staged under ./racers/).
#
#   python3 serve.py --dir build-web --port 8000
#   -> open http://127.0.0.1:8000/LEGORacers.html
#
import argparse
import os
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer


class Handler(SimpleHTTPRequestHandler):
    extensions_map = {
        **SimpleHTTPRequestHandler.extensions_map,
        ".wasm": "application/wasm",
        ".js": "text/javascript",
    }

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "cross-origin")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def send_head(self):
        rng = self.headers.get("Range")
        if rng is None:
            return super().send_head()

        path = self.translate_path(self.path)
        if os.path.isdir(path):
            return super().send_head()
        try:
            f = open(path, "rb")
        except OSError:
            self.send_error(404, "File not found")
            return None

        size = os.fstat(f.fileno()).st_size
        try:
            unit, span = rng.split("=", 1)
            start_s, end_s = span.split("-", 1)
            if unit.strip() != "bytes":
                raise ValueError
            start = int(start_s) if start_s else 0
            end = int(end_s) if end_s else size - 1
        except ValueError:
            f.close()
            self.send_error(400, "Malformed Range")
            return None

        if start >= size:
            f.close()
            self.send_response(416)
            self.send_header("Content-Range", "bytes */%d" % size)
            self.end_headers()
            return None

        end = min(end, size - 1)
        length = end - start + 1
        self.send_response(206)
        self.send_header("Content-Type", self.guess_type(path))
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Range", "bytes %d-%d/%d" % (start, end, size))
        self.send_header("Content-Length", str(length))
        self.end_headers()
        f.seek(start)
        self._range_remaining = length
        return f

    def copyfile(self, source, outputfile):
        remaining = getattr(self, "_range_remaining", None)
        if remaining is None:
            return super().copyfile(source, outputfile)
        while remaining > 0:
            chunk = source.read(min(64 * 1024, remaining))
            if not chunk:
                break
            outputfile.write(chunk)
            remaining -= len(chunk)


def main():
    ap = argparse.ArgumentParser(description="Range + COOP/COEP static server")
    ap.add_argument("--dir", default=".", help="directory to serve")
    ap.add_argument("--port", type=int, default=8000)
    ap.add_argument("--host", default="127.0.0.1")
    args = ap.parse_args()

    os.chdir(args.dir)
    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    print("serving %s at http://%s:%d/ (Range + COOP/COEP)" % (os.getcwd(), args.host, args.port))
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
