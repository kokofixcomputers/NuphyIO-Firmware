import { KEY_INDEX } from "./keycodes";

/** macOS calls the Alt key Option and the Win key Cmd; the labels follow the layer set being shown. */
let osWin = false;
export const setLabelOS = (win: boolean) => { osWin = win; };
export const osLabel = (label: string, win = osWin) =>
  win ? label.replace(/\bCmd\b/, "Win") : label.replace(/\bAlt\b/, "Opt");

export const LAYERS = 8; // 0-3 = Mac FN0-3, 4-7 = Win FN0-3
/** Slots per layer: 6 * matrix width + 2, with the width read from the keyboard (command A0 byte 4): 98 on the Air75 V3 / V2 boards, 128 on the Air100 V3-style Air96 V2. */
let layerSlots = 98;
export const setMatrixCols = (cols: number) => { if (cols >= 10 && cols <= 32) layerSlots = 6 * cols + 2; };
export const keymapBytes = () => LAYERS * layerSlots * 2;
export const keyAddr = (layer: number, slot: number) => (layer * layerSlots + slot) * 2;
export const entryAt = (map: Uint8Array, layer: number, slot: number) => {
  const a = keyAddr(layer, slot);
  return a + 1 < map.length ? (map[a] << 8) | map[a + 1] : 0;
};

export const stdEntry = (usage: number) => usage & 0xff;
export const macroEntry = (slot: number) => 0x7700 | (slot & 0xff);

/** Human-readable label for a 16-bit keymap entry. */
export function describeEntry(e: number): string {
  const hi = e >> 8, lo = e & 0xff;
  if (e === 0x0000) return "";
  if (e === 0x0001) return "▽";
  const known = KEY_INDEX.get(e);
  if (known) return osLabel(known.label);
  if (hi === 0x00) return `0x${lo.toString(16)}`;
  if (hi === 0x77) return `M${lo + 1}`;
  if (hi === 0x7f) return lo < 64 ? `SOCD${lo + 1}` : lo < 128 ? `TD${lo - 63}` : `TGL${lo - 127}`;
  if (hi === 0x52) return `MO ${lo & 31}`;
  if (hi === 0x7e) return `Fn·${lo.toString(16)}`;
  return e.toString(16).padStart(4, "0");
}
