import { KEY_INDEX } from "./keycodes";

/** macOS calls the Alt key Option and the Win key Cmd; the labels follow the layer set being shown. */
let osWin = false;
export const setLabelOS = (win: boolean) => { osWin = win; };
export const osLabel = (label: string, win = osWin) =>
  win ? label.replace(/\bCmd\b/, "Win") : label.replace(/\bAlt\b/, "Opt");

export const LAYER_BYTES = 196; // 98 entries x 2 bytes
export const LAYERS = 8; // 0-3 = Mac FN0-3, 4-7 = Win FN0-3
export const KEYMAP_BYTES = LAYER_BYTES * LAYERS;

export const keyAddr = (layer: number, slot: number) => layer * LAYER_BYTES + slot * 2;
export const entryAt = (map: Uint8Array, layer: number, slot: number) => {
  const a = keyAddr(layer, slot);
  return (map[a] << 8) | map[a + 1];
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
