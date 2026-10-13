# Local mirror of https://drive.nuphy.io with a fake keyboard injected before the site's own scripts run. The fake keyboard
# forwards every 64-byte report to server.py, i.e. to the firmware's real protocol code compiled for the host.
import gzip, sys, urllib.request, urllib.error
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
UP = "https://drive.nuphy.io"
NAME = sys.argv[1] if len(sys.argv) > 1 else "Air75 V2 (open firmware)"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8767
PID = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x1028
REAL = NAME == "real"
LOG = NAME == "log"
LOGGER = """<script>(()=>{
  const hx = u => Array.from(u, b => b.toString(16).padStart(2, '0')).join('');
  const post = (kind, u) => fetch('http://127.0.0.1:8766/rec?k=' + kind, { method: 'POST', body: u, keepalive: true }).catch(() => {});
  const send = HIDDevice.prototype.sendReport;
  HIDDevice.prototype.sendReport = function (id, data) { const u = data instanceof ArrayBuffer ? new Uint8Array(data) : new Uint8Array(data.buffer, data.byteOffset, data.byteLength); post('tx', u.slice()); return send.call(this, id, data); };
  const add = EventTarget.prototype.addEventListener;
  EventTarget.prototype.addEventListener = function (type, fn, opts) {
    if (type === 'inputreport' && this instanceof HIDDevice && !fn.__s) { const w = function (e) { post('rx', new Uint8Array(e.data.buffer.slice(e.data.byteOffset, e.data.byteOffset + e.data.byteLength))); return fn.call(this, e); }; w.__s = true; return add.call(this, type, w, opts); }
    return add.call(this, type, fn, opts);
  };
})();</script>"""
INJECT = """<script>(()=>{
  const fake = Object.assign(new EventTarget(), {
    opened: false, vendorId: 0x19f5, productId: %d, productName: %r,
    collections: [{ usagePage: 1, usage: 0, inputReports: [{ reportId: 0 }], outputReports: [{ reportId: 0 }], featureReports: [] }],
    open: async () => { fake.opened = true; }, close: async () => { fake.opened = false; },
    sendFeatureReport: async () => {}, receiveFeatureReport: async () => new DataView(new ArrayBuffer(0)),
    sendReport: async (id, data) => {
      const u = data instanceof ArrayBuffer ? new Uint8Array(data) : new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
      const r = await fetch('http://127.0.0.1:8766/frame', { method: 'POST', body: u });
      if (r.status === 200) { const b = new Uint8Array(await r.arrayBuffer()); setTimeout(() => { const ev = new Event('inputreport'); Object.assign(ev, { device: fake, reportId: 0, data: new DataView(b.buffer) }); fake.dispatchEvent(ev); }, 1); }
    },
  });
  navigator.hid.getDevices = async () => [fake];
  navigator.hid.requestDevice = async () => [fake];
  window.__fake = fake;
})();</script>""" % (PID, NAME)
class H(BaseHTTPRequestHandler):
    def do_GET(self):
        try:
            req = urllib.request.Request(UP + self.path, headers={"User-Agent": self.headers.get("User-Agent", "Mozilla/5.0"), "Accept-Encoding": "gzip"})
            with urllib.request.urlopen(req, timeout=20) as r:
                body = r.read(); ctype = r.headers.get("Content-Type", "application/octet-stream"); status = r.status
                if r.headers.get("Content-Encoding") == "gzip": body = gzip.decompress(body)
        except urllib.error.HTTPError as e:
            self.send_response(e.code); self.end_headers(); return
        if "text/html" in ctype:
            t = body.decode("utf-8", "replace"); i = t.lower().find("<head>")
            if LOG: t = t[:i + 6] + LOGGER + t[i + 6:]
            elif not REAL: t = t[:i + 6] + INJECT + t[i + 6:] if i >= 0 else INJECT + t
            body = t.encode()
        self.send_response(status); self.send_header("Content-Type", ctype); self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body)
    def log_message(self, *a): pass
ThreadingHTTPServer(("127.0.0.1", PORT), H).serve_forever()
