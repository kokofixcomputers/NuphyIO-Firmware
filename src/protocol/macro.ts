import { SHIFT, charToUsage } from "./hid";

/**
 * Macro memory: table of 32 little-endian u16 start offsets at 0 (macro i spans [t[i], t[i+1])),
 * then event streams. Each event = 4 bytes: [delayLo delayHi][type key], type 0x40 = down,
 * 0x00 = up, and the final event carries 0x80 (end). Verified against a stored macro.
 */
export interface MacroEvent { down: boolean; key: number; delayMs: number }

export const MACRO_TABLE_ENTRIES = 32;
export const MACRO_SLOTS = MACRO_TABLE_ENTRIES - 1;
export const MACRO_DATA_START = MACRO_TABLE_ENTRIES * 2; // 0x40
export const MACRO_LIMIT = 0x240; // conservative: memory past here is not macro storage

export function decodeEvents(b: ArrayLike<number>): MacroEvent[] {
  const ev: MacroEvent[] = [];
  for (let i = 0; i + 3 < b.length; i += 4) {
    const type = b[i + 2];
    ev.push({ delayMs: b[i] | (b[i + 1] << 8), down: (type & 0x40) !== 0, key: b[i + 3] });
    if (type & 0x80) break;
  }
  return ev;
}

export function encodeEvents(events: MacroEvent[]): number[] {
  const out: number[] = [];
  events.forEach((e, i) => {
    const d = Math.max(0, Math.min(0xffff, Math.round(e.delayMs)));
    const last = i === events.length - 1;
    out.push(d & 0xff, d >> 8, (e.down ? 0x40 : 0x00) | (last ? 0x80 : 0), e.key & 0xff);
  });
  return out;
}

/** Make sure every pressed key is released, otherwise the macro would end with a stuck key. */
export function closeHeldKeys(events: MacroEvent[]): MacroEvent[] {
  const held: number[] = [];
  for (const e of events) {
    if (e.down) held.push(e.key);
    else if (held.includes(e.key)) held.splice(held.indexOf(e.key), 1);
  }
  return [...events, ...held.map((key) => ({ down: false, key, delayMs: 10 }))];
}

export function readTable(region: Uint8Array): number[] {
  return Array.from({ length: MACRO_TABLE_ENTRIES }, (_, i) => region[i * 2] | (region[i * 2 + 1] << 8));
}

export function splitMacros(region: Uint8Array): number[][] {
  const t = readTable(region);
  return Array.from({ length: MACRO_SLOTS }, (_, i) =>
    t[i + 1] >= t[i] && t[i + 1] <= region.length ? Array.from(region.slice(t[i], t[i + 1])) : []);
}

/** Rebuild the region with macro `slot` replaced; throws if it doesn't fit. */
export function replaceMacro(region: Uint8Array, slot: number, events: MacroEvent[]): Uint8Array {
  const macros = splitMacros(region);
  macros[slot] = events.length ? encodeEvents(events) : [];
  const out = new Uint8Array(MACRO_LIMIT);
  let off = MACRO_DATA_START;
  const starts: number[] = [];
  for (const m of macros) {
    starts.push(off);
    if (off + m.length > MACRO_LIMIT) throw new Error(`Macros too large: ${off + m.length} > ${MACRO_LIMIT} bytes`);
    out.set(m, off);
    off += m.length;
  }
  starts.push(off); // end of the last macro
  starts.forEach((s, i) => { out[i * 2] = s & 0xff; out[i * 2 + 1] = s >> 8; });
  return out;
}

export function textToMacro(text: string, delayMs: number): MacroEvent[] {
  const ev: MacroEvent[] = [];
  for (const ch of text) {
    const m = charToUsage(ch);
    if (!m) continue;
    const [key, shift] = m;
    if (shift) ev.push({ down: true, key: SHIFT, delayMs });
    ev.push({ down: true, key, delayMs });
    ev.push({ down: false, key, delayMs });
    if (shift) ev.push({ down: false, key: SHIFT, delayMs });
  }
  return ev;
}
