/** Physical Air75 V3 layout expressed in keymap slots (slot = row*16 + col). */
export interface PhysKey { slot: number; w: number }

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
