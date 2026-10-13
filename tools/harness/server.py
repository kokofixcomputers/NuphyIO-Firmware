# POST /frame (64 raw bytes) -> response frame (or 204 if the firmware stays silent). Logs every exchange to harness.log.
import ctypes, sys, os
from http.server import BaseHTTPRequestHandler, HTTPServer
lib = ctypes.CDLL(os.path.join(os.path.dirname(os.path.abspath(__file__)), "libnph.dylib")); lib.sim_init()
log = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "harness.log"), "a")
class H(BaseHTTPRequestHandler):
    def _cors(self):
        for k, v in (("Access-Control-Allow-Origin", "*"), ("Access-Control-Allow-Methods", "POST, OPTIONS"), ("Access-Control-Allow-Headers", "*"), ("Access-Control-Allow-Private-Network", "true")):
            self.send_header(k, v)
    def do_OPTIONS(self): self.send_response(204); self._cors(); self.end_headers()
    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0)); body = self.rfile.read(n)
        if self.path.startswith("/rec"):
            kind = self.path.split("k=")[1]
            open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "real.log"), "a").write(f"{kind} {body[:64].hex()}\n")
            self.send_response(200); self._cors(); self.end_headers(); return
        if self.path.startswith("/poke"):
            import urllib.parse as up
            q = dict(up.parse_qsl(up.urlparse(self.path).query)); what = q["w"]
            if what == "key": lib.sim_poke_keymap(int(q["layer"]), int(q["slot"]), int(q["v"], 0))
            elif what == "default": lib.sim_poke_default(int(q["layer"]), int(q["slot"]), int(q["v"], 0))
            elif what == "appdef": lib.sim_poke_appdef(int(q["off"]), int(q["v"], 0))
            elif what == "kbfunc": lib.sim_poke_kbfunc(int(q["off"]), int(q["v"], 0))
            self.send_response(200); self._cors(); self.end_headers(); return
        if self.path == "/reset": lib.sim_init(); self.send_response(200); self._cors(); self.end_headers(); return
        inb = (ctypes.c_uint8 * 64)(*body[:64].ljust(64, b"\0")); out = (ctypes.c_uint8 * 64)()
        ok = lib.sim_frame(inb, out)
        log.write(f"tx {bytes(inb[:12]).hex()} -> {'rx ' + bytes(out[:12]).hex() if ok else 'SILENT'}\n"); log.flush()
        if ok: self.send_response(200); self._cors(); self.end_headers(); self.wfile.write(bytes(out))
        else: self.send_response(204); self._cors(); self.end_headers()
    def log_message(self, *a): pass
HTTPServer(("127.0.0.1", 8766), H).serve_forever()
