/**
 * Demo mode (?demo): a simulated Air75 V3 behind a fake navigator.hid that speaks the real wire
 * protocol (same frame builder/parser as the app), so the UI can be explored without hardware.
 */
import { CMD, FRAME_LEN, MAX_DATA, checksum, parseFrame } from "../protocol/frames";
import { ledSlots } from "../layout";
import { MACRO_LIMIT, MACRO_DATA_START, encodeEvents } from "../protocol/macro";
import { LED_COUNT } from "../protocol/lighting";

const L0 = [
  0x0029, 0x0069, 0x006a, 0x7e06, 0x7e07, 0x7e08, 0x7e16, 0x00ac, 0x00ae, 0x00ab, 0x00a8, 0x00aa, 0x00a9, 0x7e0b, 0x0049, 0,
  0x0035, 0x001e, 0x001f, 0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x002d, 0x002e, 0x002a, 0, 0x004b,
  0x002b, 0x0014, 0x001a, 0x0008, 0x0015, 0x0017, 0x001c, 0x0018, 0x000c, 0x0012, 0x0013, 0x002f, 0x0030, 0x0031, 0, 0x004e,
  0x0039, 0x0004, 0x0016, 0x0007, 0x0009, 0x000a, 0x000b, 0x000d, 0x000e, 0x000f, 0x0033, 0x0034, 0x00a8, 0x0028, 0, 0x004a,
  0x00e1, 0, 0x001d, 0x001b, 0x0006, 0x0019, 0x0005, 0x0011, 0x0010, 0x0036, 0x0037, 0x0038, 0x00e5, 0, 0x0052, 0x004d,
  0x00e0, 0x00e2, 0x00e3, 0, 0, 0, 0x002c, 0, 0, 0, 0x00e7, 0x5221, 0x00e4, 0x0050, 0x0051, 0x004f,
  0x00aa, 0x00a9,
];

function hsv(h: number, s: number, v: number): [number, number, number] {
  const f = (n: number) => { const k = (n + h / 60) % 6; return v - v * s * Math.max(0, Math.min(k, 4 - k, 1)); };
  return [Math.round(f(5) * 255), Math.round(f(3) * 255), Math.round(f(1) * 255)];
}

function makeBoard(name: string, button: boolean, hid: EventTarget) {
  const keymap = new Uint8Array(196 * 8);
  for (let layer = 0; layer < 8; layer++) {
    L0.forEach((e, i) => {
      let v = e === 0 ? 0 : layer % 4 === 0 ? e : 0x0001;
      if (layer === 4 && i >= 1 && i <= 12) v = 0x3a + i - 1;
      keymap[layer * 196 + i * 2] = v >> 8;
      keymap[layer * 196 + i * 2 + 1] = v & 0xff;
    });
  }
  for (let layer = 0; layer < 8; layer++) {                // top-right control = slot 60: the V2's Del button, or the V3 knob's push (mute)
    const v = layer % 4 === 0 ? (button ? 0x4c : 0xa8) : 0x0001;
    keymap[layer * 196 + 120] = v >> 8; keymap[layer * 196 + 121] = v & 0xff;
  }
  const macros = new Uint8Array(MACRO_LIMIT);
  const demo = encodeEvents([
    { down: true, key: 0x38, delayMs: 23 }, { down: false, key: 0x38, delayMs: 65 },
    { down: true, key: 0x16, delayMs: 26 }, { down: false, key: 0x16, delayMs: 35 },
    { down: true, key: 0x13, delayMs: 34 }, { down: false, key: 0x13, delayMs: 42 },
  ]);
  macros.set(demo, MACRO_DATA_START);
  const end = MACRO_DATA_START + demo.length;
  for (let i = 0; i < 32; i++) { const v = i === 0 ? MACRO_DATA_START : end; macros[i * 2] = v & 255; macros[i * 2 + 1] = v >> 8; }

  const defaults = keymap.slice();
  const td = new Uint8Array(512).fill(0xff);
  td.set([0x00, 0x09, 0x00, 0x07, 0x00, 0x09, 0x00, 0xaa], 0);
  const tgl = new Uint8Array(128).fill(0xff);
  tgl.set([0x00, 0x09], 0);
  // One 17-byte state per mode (handle 0 = Mac, 1 = Windows): back light (9) + side light (8).
  const light = [
    new Uint8Array([0x14, 0x3b, 1, 0, 1, 0, 0x96, 0x37, 0x37, 0x00, 0x36, 4, 1, 0, 0xff, 0, 0]),
    new Uint8Array([0x06, 0x32, 2, 0, 1, 0x4e, 0, 5, 0x80, 0x04, 0x3c, 2, 1, 0, 0xff, 0, 0]),
  ];
  const sleep = new Uint8Array([1, 6, 24, 4]);
  const kbfunc = new Uint8Array([2, 0, 0, 0]);
  const appdef = new Uint8Array(954); appdef[168] = button ? 1 : 0;          // 1 = button (the V2), 0 = knob (the V3)
  const slots = ledSlots(60);
  const t0 = performance.now();

  const leds = () => {
    const t = (performance.now() - t0) / 1000;
    const L = light[0];
    const out = new Uint8Array(LED_COUNT * 3);
    for (let i = 0; i < LED_COUNT; i++) {
      const side = i >= 84;
      const eff = side ? L[9] : L[0], bright = (side ? L[10] : L[1]) / 100, gear = 4 - (side ? L[11] : L[2]);
      const custom = side ? L[12] === 0 : L[4] === 0;
      const base = side ? 14 : 6;
      let rgb: [number, number, number];
      if (custom && (side ? eff === 2 || eff === 0 || eff === 3 || eff === 4 : eff === 3)) rgb = [L[base], L[base + 1], L[base + 2]].map((c) => Math.round(c * bright)) as [number, number, number];
      else if (!side && eff === 3) rgb = [255, 255, 255].map((c) => Math.round(c * bright)) as [number, number, number];
      else {
        const s = side ? (i - 84) * 18 : (slots[i] ?? i);
        rgb = hsv(((side ? s : (s & 15) * 14 + (s >> 4) * 26) + t * 70 * (0.5 + gear / 3)) % 360, 0.85, bright);
      }
      out.set(rgb, i * 3);
    }
    return out;
  };

  const regions = (cmd: number, addr: number, len: number, handle = 0): Uint8Array => {
    const src: Uint8Array | undefined =
      cmd === CMD.GET_KEYS ? keymap : cmd === CMD.GET_DEFAULT_KEYS ? defaults : cmd === CMD.GET_TAPDANCE ? td : cmd === CMD.GET_TGL ? tgl : cmd === CMD.GET_MACRO ? macros : cmd === CMD.GET_LIGHT ? light[handle & 1]
      : cmd === 0xf8 ? new Uint8Array([1, 0, 3, 0, 0, 0, 7, 0, 1, 0, 24, 2, ...Array.from("Oct 12 2026 12:00", (c) => c.charCodeAt(0))]) : cmd === 0xe1 ? kbfunc : cmd === 0xf3 ? sleep : cmd === 0xfb ? appdef : cmd === CMD.GET_LED_COLOR ? leds()
      : cmd === CMD.GET_BASE ? new Uint8Array([0, 8, 4, 6, 0x10, 0x82, 0, 0])
      : cmd === CMD.GET_FIRMWARE ? new Uint8Array([0x0e, 0, 1, 0xaa, 6, 0, 0, 0]) : undefined;
    const out = new Uint8Array(len);
    if (src) out.set(src.subarray(addr, addr + len));
    return out;
  };

  let k = 0;
  let p0: ReturnType<typeof parseFrame> | null = null;
  let gone = false;
  let restartTimer: ReturnType<typeof setTimeout> | undefined;
  const dev = Object.assign(new EventTarget(), {
    opened: false, vendorId: 0x19f5, productId: 0x1028, productName: name, collections: [{ usagePage: 1, usage: 0 }],
    open: async () => { dev.opened = true; },
    close: async () => { dev.opened = false; },
    sendReport: async (_id: number, data: BufferSource) => {
      const f = new Uint8Array(data instanceof ArrayBuffer ? data : data.buffer, (data as ArrayBufferView).byteOffset ?? 0, FRAME_LEN).slice();
      const resp = new Uint8Array(FRAME_LEN);
      resp[0] = 0xaa; resp[1] = f[1];
      if (f[1] === CMD.SET_SECRET) {
        k = f[28] || 170;
        resp.fill(k, 4, 8);
      } else {
        const p = parseFrame(k, f); p0 = p;
        resp.set(f.subarray(4, 8), 4);
        const len = Math.min(p.len, MAX_DATA);
        const writers: Record<number, Uint8Array> = { [CMD.SET_KEYS]: keymap, [CMD.SET_MACRO]: macros, [CMD.SET_LIGHT]: light[p.handle & 1], [CMD.SET_TAPDANCE]: td, [CMD.SET_TGL]: tgl, 0xf5: sleep, 0xe2: kbfunc, 0xfc: appdef };
        if (writers[f[1]]) {
          const dst = writers[f[1]];
          dst.set(p.data, p.addr);
          resp.set(f.subarray(8, 8 + len), 8);
        } else {
          const d = regions(f[1], p.addr, len, p.handle);
          for (let i = 0; i < len; i++) resp[8 + i] = d[i] ^ k;
        }
      }
      resp[3] = checksum(resp);
      if (f[1] === CMD.SET_KEY_UPLOAD && p0 && p0.data[0] === 1) {
        // demo: pretend the user taps a, b, c on the board (slots 49, 70, 68 = rows/cols 3,1 / 4,6 / 4,4)
        [[3, 1], [4, 6], [4, 4]].forEach(([r, c], i) => {
          for (const [dt, st] of [[1200 + i * 450, 1], [1290 + i * 450, 0]]) setTimeout(() => {
            const ev = new Event("inputreport");
            Object.assign(ev, { device: dev, reportId: 0, data: new DataView(new Uint8Array([CMD.SET_KEY_UPLOAD, r, c, st, ...new Array(60).fill(0)]).buffer) });
            dev.dispatchEvent(ev);
          }, dt);
        });
      }
      if (f[1] === CMD.SET_MACRO) {
        // Like the real board: restart after a macro write — drop off USB, return ~1.2 s later.
        clearTimeout(restartTimer);
        restartTimer = setTimeout(() => {
          gone = true; dev.opened = false;
          const ev = new Event("disconnect"); Object.assign(ev, { device: dev }); hid.dispatchEvent(ev);
          setTimeout(() => { gone = false; const c = new Event("connect"); Object.assign(c, { device: dev }); hid.dispatchEvent(c); }, 1200);
        }, 200);
      }
      setTimeout(() => {
        const ev = new Event("inputreport");
        Object.assign(ev, { device: dev, reportId: 0, data: new DataView(resp.buffer) });
        dev.dispatchEvent(ev);
      }, 2);
    },
  });

  return Object.assign(dev, { isGone: () => gone });
}

export function installMockHid() {
  const hid = Object.assign(new EventTarget(), {
    boards: [] as ReturnType<typeof makeBoard>[],
    n: 0,
    // each "Add keyboard" in demo mode hands out the next simulated board: a V3 (knob) first, then an Air75 V2 and a Halo75 V2 (both with a button)
    requestDevice: async () => {
      const i = hid.n++ % 3;
      if (!hid.boards[i]) hid.boards[i] = i === 0 ? makeBoard("Air75 V3 (demo)", false, hid) : i === 1 ? makeBoard("Air75 V2 (open firmware, demo)", true, hid) : makeBoard("Halo75 V2 (open firmware, demo)", true, hid);
      return [hid.boards[i]];
    },
    getDevices: async () => hid.boards.filter((b) => !b.isGone()),
  });
  Object.defineProperty(navigator, "hid", { value: hid, configurable: true });
}
