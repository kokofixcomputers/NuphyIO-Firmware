import { hexDump } from "./cipher";
import { CMD, FRAME_LEN, IN_FLAG, MAX_DATA, Parsed, buildFrame, buildHandshake, parseFrame } from "./frames";
import { EMPTY_TD, EMPTY_TGL, TD_BYTES, TD_SLOTS, TGL_BYTES, TGL_SLOTS, TapDance, encodeTapDance, encodeTgl } from "./advanced";
import { keyAddr, keymapBytes, setMatrixCols } from "./keymap";
import { BACK_ADDR, BackLight, LED_BYTES, LIGHT_BYTES, LightState, SIDE_ADDR, SideLight, backBytes, parseLightState, sideBytes } from "./lighting";
import { MACRO_LIMIT, MacroEvent, replaceMacro } from "./macro";

// Air75 V3 (from ioreg): VID 6645, PID 4136; config interface = usage page 0x01 / usage 0x00,
// one 64-byte input + one 64-byte output report, no report IDs.
export const NUPHY_VID = 0x19f5;
export const NUPHY_PID = 0x1028;
/** The open Air75 V2 firmware can present either its own ID (0x3246) or the V3's. */
export const NUPHY_PIDS = [0x1028, 0x102d, 0x3246];      // 0x102D = nuphy.io's Air100 V3, which the Air96 V2 firmware presents

const LED_CHUNK = 54; // chunk size the official app uses when polling LED colours
const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

export interface DeviceEvents {
  log?: (dir: "tx" | "rx" | "info" | "warn", msg: string) => void;
  disconnect?: () => void; // user-initiated or unrecoverable
  sleepChanged?: () => void; // unsolicited F4: the keyboard toggled auto sleep itself
  reset?: () => void; // restore-factory finished (unsolicited report F2): everything must be read again
  mode?: (mode: 0 | 1) => void; // the OS slide switch moved (unsolicited report A2: [base layer 0|4, mode 0|1])
  lightChanged?: (state: Uint8Array) => void; // unsolicited D7 + 17-byte light state of the active profile (sent after a mode switch / FN lighting keys)
  lost?: () => void; // the device vanished unexpectedly (e.g. the keyboard restarted); call reconnect()
}

export interface ConnectInfo { k: number; base: Uint8Array; firmware: Uint8Array }

interface Pending { cmd: number; resolve: (p: Parsed) => void; reject: (e: Error) => void; timer: ReturnType<typeof setTimeout> }

const ACCESSORY_ADDR = 168;
export interface StoreStatus { rev: number; build: string; bank: number; appends: number; compactions: number; failures: number; saves: number; loadedAtBoot: boolean; pending: boolean; freeKB: number }
export interface ModeSettings { stabilization: number; disableWin: boolean; disableAltF4: boolean; disableAltTab: boolean }   // stabilization: 0-1 low, 2 intermediate, 3 high
export interface SleepCfg { enabled: boolean; level1: number; level2: number; reserved: number }

export class NuPhyDevice {
  private lastMode: 0 | 1 = 0;
  private hid: HIDDevice | null = null;
  private k = 0;
  private chain: Promise<unknown> = Promise.resolve();
  private pending: Pending | null = null;
  private polling = false;
  private keyUpload: ((slot: number, down: boolean) => void) | null = null;
  private onInput = (e: HIDInputReportEvent) => this.handleInput(e);
  private onDetach = (e: HIDConnectionEvent) => { if (e.device === this.hid) this.lose(); };

  constructor(private ev: DeviceEvents = {}) {}

  // ---------- connection ----------

  /** Show the browser's device chooser and return the keyboard the user picked (without opening it). */
  static async pick(): Promise<HIDDevice> {
    const [dev] = await navigator.hid.requestDevice({
      filters: NUPHY_PIDS.map((productId) => ({ vendorId: NUPHY_VID, productId, usagePage: 0x01, usage: 0x00 })),
    });
    if (!dev) throw new Error("No device selected.");
    return dev;
  }

  async connect(): Promise<ConnectInfo> { return this.attach(await NuPhyDevice.pick()); }

  /** Connect to a keyboard the user already picked. */
  connectTo(dev: HIDDevice): Promise<ConnectInfo> { return this.attach(dev); }

  get productName(): string | undefined { return this.hid?.productName; }

  /** Open an already-authorised device and run the handshake. */
  private async attach(dev: HIDDevice): Promise<ConnectInfo> {
    if (!dev.opened) await dev.open();
    this.hid = dev;
    dev.addEventListener("inputreport", this.onInput);
    navigator.hid.addEventListener("disconnect", this.onDetach);
    this.ev.log?.("info", `${dev.productName} opened`);

    const { frame, k } = buildHandshake();
    this.k = k;
    await this.exchange(frame, CMD.SET_SECRET);
    this.ev.log?.("info", `Handshake ok, K = 0x${k.toString(16)}`);

    const base = await this.read(CMD.GET_BASE, 0, 8);
    const firmware = await this.read(CMD.GET_FIRMWARE, 0, 8);
    this.lastMode = base[0] === 1 ? 1 : 0;
    setMatrixCols(base[4]);                                 // the keymap is 6 * width + 2 slots per layer
    this.ev.log?.("info", `base=${hexDump(base)} firmware=${hexDump(firmware)}`);
    return { k, base, firmware };
  }

  /**
   * After the keyboard restarts itself (it does this after some writes, e.g. macros) it drops off USB
   * and comes back a moment later. Poll for it among already-authorised devices and handshake again.
   */
  async reconnect(timeoutMs = 15000): Promise<ConnectInfo> {
    const t0 = Date.now();
    this.ev.log?.("info", "Keyboard went away — waiting for it to come back…");
    while (Date.now() - t0 < timeoutMs) {
      await sleep(400);
      const devs = await navigator.hid.getDevices();
      const dev = devs.find((d) => d.vendorId === NUPHY_VID && NUPHY_PIDS.includes(d.productId) && d.collections.some((c) => c.usagePage === 1 && c.usage === 0));
      if (!dev) continue;
      try { return await this.attach(dev); } catch (e) { this.ev.log?.("warn", `reconnect attempt failed: ${e}`); this.detachListeners(); }
    }
    throw new Error("The keyboard didn't come back. Unplug it, plug it in again, and reconnect.");
  }

  async disconnect() {
    const d = this.hid;
    this.teardown();
    if (d?.opened) await d.close();
  }

  private detachListeners() {
    if (this.hid) this.hid.removeEventListener("inputreport", this.onInput);
    navigator.hid.removeEventListener("disconnect", this.onDetach);
    this.pending?.reject(new Error("Disconnected"));
    this.pending = null;
    this.polling = false;
    this.keyUpload = null;
    this.hid = null;
    this.k = 0;
  }

  private teardown() {
    this.detachListeners();
    this.ev.disconnect?.();
  }

  /** The device vanished on its own: hand over to reconnect logic if anyone listens, else disconnect. */
  private lose() {
    this.detachListeners();
    (this.ev.lost ?? this.ev.disconnect)?.();
  }

  // ---------- transport (one request in flight at a time) ----------

  private handleInput(e: HIDInputReportEvent) {
    const f = new Uint8Array(e.data.buffer, e.data.byteOffset, Math.min(e.data.byteLength, FRAME_LEN));
    if (f[0] === CMD.SET_KEY_UPLOAD) { this.keyUpload?.(f[1] * 16 + f[2], f[3] !== 0); return; }   // macro recording: unframed [C1, row, col, pressed]
    if (f[0] === 0xa2) {                                       // [A2, layer, mode]: sent on a mode switch and whenever the active FN layer changes
      const m = f[2] === 1 ? 1 : 0;
      if (m !== this.lastMode) { this.lastMode = m; this.ev.mode?.(m); }
      return;
    }
    if (f[0] === 0xf4) { this.ev.sleepChanged?.(); return; }      // [F4, enabled, level-1 min, level-2 min, 4]: the sleep setting changed on the keyboard (FN + ])
    if (f[0] === 0xf2) { this.ev.reset?.(); return; }          // the keyboard was reset to factory settings
    if (f[0] === 0xd7) { this.ev.lightChanged?.(f.slice(1, 18)); return; }
    if (f[0] !== IN_FLAG) return;
    const p = this.pending;
    if (!p || f[1] !== p.cmd) {
      this.ev.log?.("rx", `(unsolicited) ${hexDump(f, 16)} …`);
      return;
    }
    clearTimeout(p.timer);
    this.pending = null;
    p.resolve(parseFrame(this.k, f));
  }

  private enqueue<T>(fn: () => Promise<T>): Promise<T> {
    const run = this.chain.then(fn);
    this.chain = run.catch(() => undefined);
    return run;
  }

  private exchange(frame: Uint8Array<ArrayBuffer>, cmd: number, timeoutMs = 1500): Promise<Parsed> {
    return this.enqueue(() => new Promise<Parsed>((resolve, reject) => {
      if (!this.hid) return reject(new Error("Not connected"));
      const timer = setTimeout(() => {
        this.pending = null;
        reject(new Error(`Timed out waiting for 0x${cmd.toString(16)} response`));
      }, timeoutMs);
      this.pending = { cmd, resolve, reject, timer };
      if (cmd !== CMD.GET_LED_COLOR) this.ev.log?.("tx", hexDump(frame, 16) + " …");
      this.hid.sendReport(0, frame).catch((err) => {
        clearTimeout(timer);
        this.pending = null;
        reject(err instanceof Error ? err : new Error(String(err)));
      });
    }));
  }

  // ---------- memory access ----------

  async readChunk(cmd: number, addr: number, len: number, handle = 0): Promise<Uint8Array> {
    const r = await this.exchange(buildFrame(this.k, cmd, len, addr, handle), cmd);
    if (!r.sumOk) throw new Error(`Bad checksum on 0x${cmd.toString(16)} reply`);
    return r.data;
  }

  async read(cmd: number, addr: number, length: number, chunk = MAX_DATA, handle = 0): Promise<Uint8Array> {
    const out = new Uint8Array(length);
    for (let o = 0; o < length; o += chunk) {
      const n = Math.min(chunk, length - o);
      out.set(await this.readChunk(cmd, addr + o, n, handle), o);
    }
    return out;
  }

  async write(cmd: number, addr: number, data: ArrayLike<number>, handle = 0): Promise<void> {
    for (let o = 0; o < data.length; o += MAX_DATA) {
      const part = Array.prototype.slice.call(data, o, o + MAX_DATA) as number[];
      const sent = buildFrame(this.k, cmd, part.length, addr + o, handle, part);
      const r = await this.exchange(sent, cmd);
      if (r.len !== part.length || r.addr !== addr + o) throw new Error(`Write to 0x${cmd.toString(16)}@${addr + o} not acknowledged`);
    }
  }

  /** Open-firmware boards only (command 0xF8): how the settings store is doing. Null on a stock V3 (it stays silent). */
  async readStoreStatus(): Promise<StoreStatus | null> {
    try {
      const b = await this.read(0xf8, 0, 32);
      if (b[0] !== 1) return null;
      return { rev: b[11], build: String.fromCharCode(...Array.from(b.subarray(12, 32)).filter((c) => c >= 32 && c < 127)).trim(), bank: b[1], appends: b[2] | (b[3] << 8), compactions: b[4], failures: b[5], saves: b[6] | (b[7] << 8), loadedAtBoot: b[8] !== 0, pending: b[9] !== 0, freeKB: Math.round(((b[10] * 256) / 1024) * 10) / 10 };
    } catch { return null; }
  }

  // ---------- mode settings (E1 read / E2 write; 4 bytes: [stabilization level, disable Win, disable Alt+F4, disable Alt+Tab]) ----------

  async readModeSettings(): Promise<ModeSettings> {
    const b = await this.read(CMD.GET_KBFUNC, 0, 4);
    return { stabilization: b[0], disableWin: b[1] !== 0, disableAltF4: b[2] !== 0, disableAltTab: b[3] !== 0 };
  }
  async writeModeSettings(m: ModeSettings): Promise<ModeSettings> {
    await this.write(CMD.SET_KBFUNC, 0, [m.stabilization, m.disableWin ? 1 : 0, m.disableAltF4 ? 1 : 0, m.disableAltTab ? 1 : 0]);
    return this.readModeSettings();
  }

  // ---------- sleep (F3 read / F5 write; 4 bytes: [auto sleep on, level-1 minutes, level-2 minutes, reserved]) ----------

  async readSleep(): Promise<SleepCfg> {
    const b = await this.read(CMD.GET_SLEEP, 0, 4);
    return { enabled: b[0] !== 0, level1: b[1], level2: b[2], reserved: b[3] };
  }

  async writeSleep(c: SleepCfg): Promise<SleepCfg> {
    await this.write(CMD.SET_SLEEP, 0, [c.enabled ? 1 : 0, c.level1, c.level2, c.reserved]);
    return this.readSleep();
  }

  // ---------- recording + accessory ----------

  /**
   * Macro recording: tell the keyboard to stop typing into the OS and report every key event to us instead
   * (the same switch nuphy.io flips). Events arrive as raw `[C1, row, col, pressed]` reports; slot = row * 16 + col.
   */
  async startKeyUpload(cb: (slot: number, down: boolean) => void) {
    this.keyUpload = cb;
    await this.write(CMD.SET_KEY_UPLOAD, 0, [1]);
  }
  async stopKeyUpload() {
    this.keyUpload = null;
    await this.write(CMD.SET_KEY_UPLOAD, 0, [0]);
  }

  /** What sits in the keyboard's top-right corner: byte 168 of the app-define blob (0 = knob, 1 = button). */
  async readAccessory(): Promise<"knob" | "button"> {
    const b = await this.read(CMD.GET_APPDEF, ACCESSORY_ADDR, 1);
    return b[0] === 1 ? "button" : "knob";
  }
  async writeAccessory(a: "knob" | "button"): Promise<"knob" | "button"> {
    await this.write(CMD.SET_APPDEF, ACCESSORY_ADDR, [a === "button" ? 1 : 0]);
    return this.readAccessory();
  }

  // ---------- keymap ----------

  readKeymap() { return this.read(CMD.GET_KEYS, 0, keymapBytes()); }

  /** Write one 16-bit entry (big-endian) and return what the keyboard reads back. */
  async writeKey(layer: number, slot: number, entry: number): Promise<number> {
    const addr = keyAddr(layer, slot);
    await this.write(CMD.SET_KEYS, addr, [entry >> 8, entry & 0xff]);
    const back = await this.readChunk(CMD.GET_KEYS, addr, 2);
    return (back[0] << 8) | back[1];
  }

  /** Factory entry for a key (from the default-keymap region, same addressing as the live keymap). */
  async readDefaultKey(layer: number, slot: number): Promise<number> {
    const d = await this.readChunk(CMD.GET_DEFAULT_KEYS, keyAddr(layer, slot), 2);
    return (d[0] << 8) | d[1];
  }

  // ---------- advanced keys ----------

  async readAdvanced(): Promise<{ td: Uint8Array; tgl: Uint8Array }> {
    return { td: await this.read(CMD.GET_TAPDANCE, 0, TD_SLOTS * TD_BYTES), tgl: await this.read(CMD.GET_TGL, 0, TGL_SLOTS * TGL_BYTES) };
  }

  /** Write one Tap Dance slot (null frees it) and return the bytes the keyboard reads back. */
  async writeTapDance(slot: number, td: TapDance | null): Promise<Uint8Array> {
    const bytes = td ? encodeTapDance(td) : EMPTY_TD;
    await this.write(CMD.SET_TAPDANCE, slot * TD_BYTES, bytes);
    const back = await this.readChunk(CMD.GET_TAPDANCE, slot * TD_BYTES, TD_BYTES);
    if (back.some((v, i) => v !== bytes[i])) throw new Error(`Tap Dance slot ${slot + 1} read back differently than written`);
    return back;
  }

  async writeTgl(slot: number, kc: number | null): Promise<Uint8Array> {
    const bytes = kc === null ? EMPTY_TGL : encodeTgl(kc);
    await this.write(CMD.SET_TGL, slot * TGL_BYTES, bytes);
    const back = await this.readChunk(CMD.GET_TGL, slot * TGL_BYTES, TGL_BYTES);
    if (back.some((v, i) => v !== bytes[i])) throw new Error(`Toggle slot ${slot + 1} read back differently than written`);
    return back;
  }

  // ---------- lighting ----------

  /** `mode` is the handle: 0 = Mac, 1 = Windows. */
  async readLight(mode = 0): Promise<LightState> {
    return parseLightState(await this.read(CMD.GET_LIGHT, 0, LIGHT_BYTES, MAX_DATA, mode));
  }

  /** Mirrors the official app: the block, then the brightness byte on its own. */
  async writeBack(mode: number, l: BackLight): Promise<void> {
    await this.write(CMD.SET_LIGHT, BACK_ADDR, backBytes(l), mode);
    await this.write(CMD.SET_LIGHT, BACK_ADDR + 1, [l.brightness], mode);
  }

  async writeSide(mode: number, l: SideLight): Promise<void> {
    await this.write(CMD.SET_LIGHT, SIDE_ADDR, sideBytes(l), mode);
    await this.write(CMD.SET_LIGHT, SIDE_ADDR + 1, [l.brightness], mode);
  }

  /** Live LED colours: 104 x RGB (already brightness-scaled by the keyboard). */
  startPolling(onLeds: (rgb: Uint8Array) => void, rateMs = 50) {
    if (this.polling) return;
    this.polling = true;
    void (async () => {
      let errors = 0;
      while (this.polling && this.hid) {
        try {
          const buf = new Uint8Array(LED_BYTES);
          for (let o = 0; o < LED_BYTES && this.polling; o += LED_CHUNK) {
            buf.set(await this.readChunk(CMD.GET_LED_COLOR, o, Math.min(LED_CHUNK, LED_BYTES - o)), o);
          }
          if (this.polling) onLeds(buf);
          errors = 0;
        } catch (err) {
          if (++errors >= 5) { this.ev.log?.("warn", `LED polling stopped: ${err}`); this.polling = false; }
        }
        await sleep(rateMs);
      }
    })();
  }

  stopPolling() { this.polling = false; }
  get isPolling() { return this.polling; }

  // ---------- macros ----------

  readMacroRegion() { return this.read(CMD.GET_MACRO, 0, MACRO_LIMIT); }

  /**
   * Rewrite the macro region with `slot` replaced; only changed chunks are sent. The keyboard restarts
   * itself after a macro write, so chunks go last-to-first (the offset table at the front is written
   * last, so an interrupted write can't leave it pointing at half-written data) and the verify read is
   * allowed to fail — the caller reloads after the automatic reconnect.
   */
  async writeMacro(slot: number, events: MacroEvent[]): Promise<Uint8Array> {
    const old = await this.readMacroRegion();
    const next = replaceMacro(old, slot, events);
    for (let o = Math.floor((MACRO_LIMIT - 1) / MAX_DATA) * MAX_DATA; o >= 0; o -= MAX_DATA) {
      const a = old.subarray(o, o + MAX_DATA), b = next.subarray(o, o + MAX_DATA);
      if (a.some((v, i) => v !== b[i])) await this.write(CMD.SET_MACRO, o, b);
    }
    try { return await this.readMacroRegion(); } catch { return next; }
  }
}
