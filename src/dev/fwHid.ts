/** `?fw` mode: a fake keyboard whose every report is answered by the firmware's real protocol code (tools/harness/server.py). */
export function installFwHid(name = "Air75 V2 (open firmware)") {
  const fake = Object.assign(new EventTarget(), {
    opened: false, vendorId: 0x19f5, productId: 0x1028, productName: name,
    collections: [{ usagePage: 1, usage: 0, inputReports: [{ reportId: 0 }], outputReports: [{ reportId: 0 }], featureReports: [] }],
    open: async () => { fake.opened = true; },
    close: async () => { fake.opened = false; },
    sendReport: async (_id: number, data: BufferSource) => {
      const u = data instanceof ArrayBuffer ? new Uint8Array(data) : new Uint8Array(data.buffer, (data as ArrayBufferView).byteOffset, (data as ArrayBufferView).byteLength);
      const r = await fetch("http://127.0.0.1:8766/frame", { method: "POST", body: u });
      if (r.status === 200) {
        const b = new Uint8Array(await r.arrayBuffer());
        setTimeout(() => { const ev = new Event("inputreport"); Object.assign(ev, { device: fake, reportId: 0, data: new DataView(b.buffer) }); fake.dispatchEvent(ev); }, 1);
      }
    },
  });
  const hid = Object.assign(new EventTarget(), { requestDevice: async () => [fake], getDevices: async () => [fake] });
  Object.defineProperty(navigator, "hid", { value: hid, configurable: true });
}
