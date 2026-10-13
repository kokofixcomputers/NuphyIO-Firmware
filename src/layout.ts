/** Physical Air75 V3 layout expressed in keymap slots (slot = row*16 + col). */
export interface PhysKey { slot: number; w: number }   // slot -1 = an empty gap

const run = (from: number, to: number, w = 1): PhysKey[] =>
  Array.from({ length: to - from + 1 }, (_, i) => ({ slot: from + i, w }));
const k = (slot: number, w = 1): PhysKey => ({ slot, w });

export const ROWS: PhysKey[][] = [
  [...run(0, 14)],
  [...run(16, 28), k(29, 2), k(31)],
  [k(32, 1.5), ...run(33, 44), k(45, 1.5), k(47)],
  [k(48, 1.75), ...run(49, 59), k(61, 2.25), k(63)],
  [k(64, 2.25), ...run(66, 75), k(76, 1.75), k(78), k(79)],
  [k(80, 1.25), k(81, 1.25), k(82, 1.25), k(86, 6.25), k(90), k(91), k(92), k(93), k(94), k(95)],
];

/** Air96 V2 (shown to nuphy.io as an Air100 V3): 21 matrix columns, so a key's slot is row * 21 + column. Same 15-unit main block, then Home / End / PgUp / PgDn above a numpad. */
const gap = (w = 1): PhysKey => ({ slot: -1, w });
const m = (row: number, col: number, w = 1): PhysKey => ({ slot: row * 21 + col, w });
const mrun = (row: number, c0: number, c1: number): PhysKey[] => Array.from({ length: c1 - c0 + 1 }, (_, i) => m(row, c0 + i));
export const PRINT_SLOT96 = 14, DEL_SLOT96 = 2 * 21 + 14;
export const ROWS_96: PhysKey[][] = [
  [m(0, 0), ...mrun(0, 2, 13), m(0, 14), m(2, 14), m(1, 15), m(2, 15), m(1, 16), m(2, 16)],
  [...mrun(1, 0, 12), m(1, 13, 2), ...mrun(1, 17, 20)],
  [m(2, 0, 1.5), ...mrun(2, 1, 12), m(2, 13, 1.5), ...mrun(2, 17, 20)],
  [m(3, 0, 1.75), ...mrun(3, 1, 11), m(3, 13, 2.25), ...mrun(3, 17, 19), gap()],
  [m(4, 0, 2.25), ...mrun(4, 2, 11), m(4, 13, 1.75), m(4, 15), ...mrun(4, 17, 20)],
  [m(5, 0, 1.25), m(5, 1, 1.25), m(5, 2, 1.25), m(5, 6, 6.25), m(5, 9), m(5, 10), m(5, 13), m(5, 14), m(5, 15), m(5, 16), m(5, 17, 2), m(5, 19), gap()],
];
export interface BoardLayout { rows: PhysKey[][]; units: number }
export const layoutFor = (numpad?: boolean): BoardLayout => (numpad ? { rows: ROWS_96, units: 19 } : { rows: ROWS, units: 16 });
/** Live LED list the Air96 reports: the 98 keys nuphy.io draws in reading order (Print and Del excluded), then Del (its accessory LED). */
export const ledSlots96 = (): number[] => [...ROWS_96.flat().map((k) => k.slot).filter((s) => s >= 0 && s !== PRINT_SLOT96 && s !== DEL_SLOT96), DEL_SLOT96];

/** What occupies the top-right corner: the V3's knob (push = slot 60, turns = 96/97) or a plain button (also slot 60: the V2's Del key). */
export type Accessory = "knob" | "button";
export const KNOB_PRESS_SLOT = 60;
export const BUTTON_SLOT = KNOB_PRESS_SLOT;   // nuphy.io addresses the button at the knob-press slot too, so a V2 button must live there
export const accessorySlot = (a: Accessory) => (a === "button" ? BUTTON_SLOT : KNOB_PRESS_SLOT);

/** LED i is the i-th physical key in ascending slot order; the accessory's LED (if it has one) is the 84th and last entry. */
export function ledSlots(top: number | null): number[] {
  const slots = ROWS.flat().map((key) => key.slot).sort((a, b) => a - b);
  if (top !== null) slots.push(top);
  return slots;
}
