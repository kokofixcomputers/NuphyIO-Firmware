/**
 * Advanced keys (Air75 V3, firmware >= 1.0.2.5): Tap Dance and Toggle.
 * Both live in 64-slot memory tables and are bound to a physical key by writing an "advanced"
 * keymap entry: 0x7F00 | (64 + slot) for Tap Dance, 0x7F00 | (128 + slot) for Toggle.
 * Empty table entries are all 0xFF.
 */
export const TD_SLOTS = 64, TD_BYTES = 8;
export const TGL_SLOTS = 64, TGL_BYTES = 2;
export const TD_DEFAULT_TERM = 170;

export const tdEntry = (slot: number) => 0x7f00 | (64 + slot);
export const tglEntry = (slot: number) => 0x7f00 | (128 + slot);
export const entryToTd = (e: number) => ((e >> 8) === 0x7f && (e & 0xff) >= 64 && (e & 0xff) < 128 ? (e & 0xff) - 64 : null);
export const entryToTgl = (e: number) => ((e >> 8) === 0x7f && (e & 0xff) >= 128 && (e & 0xff) < 192 ? (e & 0xff) - 128 : null);

export interface TapDance { tap: number; double: number; hold: number; termMs: number }

const w = (b: ArrayLike<number>, i: number) => (b[i] << 8) | b[i + 1];

export function parseTapDance(b: ArrayLike<number>): TapDance | null {
  if (b.length < TD_BYTES || [0, 2, 4, 6].every((i) => w(b, i) === 0xffff)) return null;
  return { tap: w(b, 0), double: w(b, 2), hold: w(b, 4), termMs: w(b, 6) };
}

export function encodeTapDance(t: TapDance): number[] {
  const term = Math.max(10, Math.min(1000, Math.round(t.termMs / 10) * 10)) || TD_DEFAULT_TERM;
  return [t.tap, t.double, t.hold, term].flatMap((v) => [(v >> 8) & 0xff, v & 0xff]);
}

export function parseTgl(b: ArrayLike<number>): number | null {
  const v = w(b, 0);
  return v === 0xffff ? null : v;
}
export const encodeTgl = (kc: number) => [(kc >> 8) & 0xff, kc & 0xff];
export const EMPTY_TD = new Array(TD_BYTES).fill(0xff) as number[];
export const EMPTY_TGL = new Array(TGL_BYTES).fill(0xff) as number[];
