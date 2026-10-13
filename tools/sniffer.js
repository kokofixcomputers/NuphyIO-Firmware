// Paste into the DevTools console on nuphy.io BEFORE clicking Connect.
// Logs every frame sent to / received from the keyboard, then lets you download the log.
(() => {
  const log = [];
  const hex = (u8) => Array.from(u8, (b) => b.toString(16).padStart(2, "0")).join(" ");
  const t0 = performance.now();
  const rec = (dir, id, bytes) => {
    const line = `${((performance.now() - t0) | 0)}ms ${dir} id=${id} ${hex(bytes)}`;
    log.push(line);
    if (!line.includes(" rx id=0 aa d2")) console.log(line); // skip noisy poll replies on screen, still saved
  };

  const send = HIDDevice.prototype.sendReport;
  HIDDevice.prototype.sendReport = function (id, data) {
    const u8 = data instanceof ArrayBuffer ? new Uint8Array(data) : new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
    rec("tx", id, u8);
    return send.call(this, id, data);
  };
  const sendF = HIDDevice.prototype.sendFeatureReport;
  HIDDevice.prototype.sendFeatureReport = function (id, data) {
    const u8 = data instanceof ArrayBuffer ? new Uint8Array(data) : new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
    rec("txfeat", id, u8);
    return sendF.call(this, id, data);
  };

  const add = EventTarget.prototype.addEventListener;
  EventTarget.prototype.addEventListener = function (type, fn, opts) {
    if (type === "inputreport" && this instanceof HIDDevice && !fn.__sniffed) {
      const wrapped = function (e) {
        rec("rx", e.reportId, new Uint8Array(e.data.buffer, e.data.byteOffset, e.data.byteLength));
        return fn.call(this, e);
      };
      wrapped.__sniffed = true;
      return add.call(this, type, wrapped, opts);
    }
    return add.call(this, type, fn, opts);
  };

  window.saveNuphyLog = () => {
    const a = document.createElement("a");
    a.href = URL.createObjectURL(new Blob([log.join("\n")], { type: "text/plain" }));
    a.download = "nuphy-capture.txt";
    a.click();
  };
  console.log("NuPhy sniffer armed. Connect, do actions, then run saveNuphyLog().");
})();
