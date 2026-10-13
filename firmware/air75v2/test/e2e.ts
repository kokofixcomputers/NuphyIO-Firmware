/* End-to-end: the web app's real NuPhyDevice client <-> the firmware's protocol core (native build, via pipes). */
import { spawn } from "node:child_process";
import { NuPhyDevice } from "../../../src/protocol/device";
import { decodeEvents, splitMacros } from "../../../src/protocol/macro";
import { parseTapDance, parseTgl } from "../../../src/protocol/advanced";
import { entryAt } from "../../../src/protocol/keymap";

const sim = spawn("./test/sim", [], { stdio: ["pipe", "pipe", "inherit"] });
let buf = Buffer.alloc(0);
const waiters: ((f: Buffer) => void)[] = [];
sim.stdout.on("data", (d: Buffer) => { buf = Buffer.concat([buf, d]); while (buf.length >= 64) { const f = buf.subarray(0, 64); buf = buf.subarray(64); waiters.shift()?.(Buffer.from(f)); } });

class FakeHID extends EventTarget {
  opened = false; productName = "Air75 V2 (firmware sim)"; vendorId = 0x19f5; productId = 0x1028;
  collections = [{ usagePage: 1, usage: 0 }];
  async open() { this.opened = true; }
  async close() { this.opened = false; }
  async sendReport(_id: number, data: Uint8Array) {
    const reply = new Promise<Buffer>((r) => waiters.push(r));
    sim.stdin.write(Buffer.from(data));
    const f = await reply;
    if (f.every((b) => b === 0)) return;                       // device stayed silent
    const ev = new Event("inputreport");
    Object.assign(ev, { device: this, reportId: 0, data: new DataView(f.buffer, f.byteOffset, 64) });
    setTimeout(() => this.dispatchEvent(ev), 0);
  }
}
const dev = new FakeHID();
const hid = Object.assign(new EventTarget(), { requestDevice: async () => [dev], getDevices: async () => [dev] });
Object.defineProperty(globalThis, "navigator", { value: { hid }, configurable: true });

let fails = 0;
const check = (name: string, ok: boolean, extra = "") => { if (!ok) fails++; console.log(ok ? "OK  " : "FAIL", name, extra); };
const hex = (a: ArrayLike<number>) => Array.from(a, (x) => x.toString(16).padStart(2, "0")).join(" ");

(async () => {
  const d = new NuPhyDevice({});
  const info = await d.connect();
  check("handshake + base/firmware", info.base[0] === 1 && info.firmware[2] === 1 && info.firmware[0] === 0x0e, `base=${hex(info.base)} fw=${hex(info.firmware)}`);

  const km = await d.readKeymap();
  check("keymap read (Esc / area-screenshot / Del button on slots 0 / 13 / 60)", entryAt(km, 0, 0) === 0x29 && entryAt(km, 0, 13) === 0x7e0b && entryAt(km, 0, 60) === 0x4c);
  check("keymap: V3 FN layer entries preserved (MO(1) on layer 0 slot 91)", entryAt(km, 0, 91) === 0x5221);

  const ms0 = await d.readModeSettings();
  const ms1 = await d.writeModeSettings({ ...ms0, disableWin: true, disableAltF4: true, disableAltTab: false, stabilization: 3 });
  check("mode settings (Disable Win / Alt+F4 / Alt+Tab, stabilization) round-trip", ms1.disableWin && ms1.disableAltF4 && !ms1.disableAltTab && ms1.stabilization === 3, JSON.stringify(ms1));
  await d.writeModeSettings(ms0);

  const back = await d.writeKey(0, 14, 0x0047);
  check("remap write + read-back", back === 0x47);
  const km2 = await d.readKeymap();
  check("remap visible in a full re-read, neighbours untouched", entryAt(km2, 0, 14) === 0x47 && entryAt(km2, 0, 13) === 0x7e0b);
  check("default key read", (await d.readDefaultKey(0, 14)) === 0x49);

  const l0 = await d.readLight(0), l1 = await d.readLight(1);
  check("light profiles are separate objects", l0.back.effect === 0x14 && l1.back.effect === 0x14);
  l1.side.effect = 3; l1.side.brightness = 42; l1.side.gear = 4; l1.side.auto = false; l1.side.r = 1; l1.side.g = 2; l1.side.b = 3;
  await d.writeSide(1, l1.side);
  const l1b = await d.readLight(1), l0b = await d.readLight(0);
  check("side-light write lands in profile 1 only", l1b.side.effect === 3 && l1b.side.brightness === 42 && l1b.side.gear === 4 && !l1b.side.auto && l1b.side.b === 3 && l0b.side.effect === 0 && l0b.side.brightness === 0x64);
  l0.back.effect = 3; l0.back.auto = false; l0.back.r = 0; l0.back.g = 0x95; l0.back.b = 0xff; l0.back.brightness = 59; l0.back.gear = 3;
  await d.writeBack(0, l0.back);
  const lb = await d.readLight(0);
  check("back-light write (colour, gear inversion)", lb.back.effect === 3 && lb.back.g === 0x95 && lb.back.b === 0xff && lb.back.brightness === 59 && lb.back.gear === 3 && !lb.back.auto);

  const evs = [{ down: true, key: 0x04, delayMs: 20 }, { down: false, key: 0x04, delayMs: 30 }];
  const region = await d.writeMacro(1, evs as never);
  const back1 = decodeEvents(splitMacros(region)[1]);
  check("macro write + parse", back1.length === 2 && back1[0].key === 4 && back1[0].down && back1[1].delayMs === 30);

  await d.writeTapDance(2, { tap: 0x14, double: 0x1a, hold: 0x08, termMs: 200 });
  await d.writeTgl(5, 0x2c);
  const adv = await d.readAdvanced();
  const td2 = parseTapDance(adv.td.subarray(16, 24)), tg5 = parseTgl(adv.tgl.subarray(10, 12));
  check("tap dance slot", !!td2 && td2.tap === 0x14 && td2.double === 0x1a && td2.hold === 0x08 && td2.termMs === 200);
  check("toggle slot + empty slots stay empty", tg5 === 0x2c && parseTgl(adv.tgl.subarray(0, 2)) === null && parseTapDance(adv.td.subarray(0, 8)) === null);

  const led = await d.readChunk(0xd2, 10, 6);
  check("LED colour read goes to the platform hook", hex(led) === hex([30, 33, 36, 39, 42, 45]));

  await d.disconnect();
  sim.kill();
  console.log(fails === 0 ? "\nE2E: ALL PASS" : `\nE2E: ${fails} FAILED`);
  process.exit(fails ? 1 : 0);
})().catch((e) => { console.error("E2E crashed:", e); sim.kill(); process.exit(2); });
