# Tiny local receiver: the sniffed page POSTs its log here; we write it to a file.
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

OUT = sys.argv[1]

class H(BaseHTTPRequestHandler):
    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.send_header("Access-Control-Allow-Private-Network", "true")
    def do_OPTIONS(self):
        self.send_response(204); self._cors(); self.end_headers()
    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        name = self.path.strip("/") or "capture"
        with open(f"{OUT}/{name}.txt", "wb") as f:
            f.write(self.rfile.read(n))
        self.send_response(200); self._cors(); self.end_headers(); self.wfile.write(b"ok")
    def log_message(self, *a): pass

HTTPServer(("127.0.0.1", 8765), H).serve_forever()
