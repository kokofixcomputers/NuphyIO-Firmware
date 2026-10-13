/** USB HID keyboard usage (page 0x07) names. */
const names: Record<number, string> = {};
for (let i = 0; i < 26; i++) names[0x04 + i] = String.fromCharCode(65 + i);
"1234567890".split("").forEach((c, i) => (names[0x1e + i] = c));
Object.assign(names, {
  0x28: "Enter", 0x29: "Esc", 0x2a: "Bksp", 0x2b: "Tab", 0x2c: "Space",
  0x2d: "-", 0x2e: "=", 0x2f: "[", 0x30: "]", 0x31: "\\", 0x33: ";", 0x34: "'",
  0x35: "`", 0x36: ",", 0x37: ".", 0x38: "/", 0x39: "Caps",
  0x46: "PrtSc", 0x47: "ScrLk", 0x48: "Pause",
  0x49: "Ins", 0x4a: "Home", 0x4b: "PgUp", 0x4c: "Del", 0x4d: "End", 0x4e: "PgDn",
  0x4f: "→", 0x50: "←", 0x51: "↓", 0x52: "↑",
  0xe0: "Ctrl", 0xe1: "Shift", 0xe2: "Alt", 0xe3: "Cmd",
  0xe4: "R Ctrl", 0xe5: "R Shift", 0xe6: "R Alt", 0xe7: "R Cmd",
});
for (let i = 0; i < 12; i++) names[0x3a + i] = `F${i + 1}`;
for (let i = 0; i < 12; i++) names[0x68 + i] = `F${i + 13}`;

export const SHIFT = 0xe1;
export const HID_NAMES = names;
export const HID_OPTIONS = Object.entries(names)
  .map(([u, n]) => ({ usage: Number(u), name: n }))
  .sort((a, b) => a.usage - b.usage);

/** KeyboardEvent.code -> HID usage, for recording macros in the browser. */
export function codeToUsage(code: string): number | null {
  let m: RegExpMatchArray | null;
  if ((m = code.match(/^Key([A-Z])$/))) return 0x04 + m[1].charCodeAt(0) - 65;
  if ((m = code.match(/^Digit([1-9])$/))) return 0x1e + Number(m[1]) - 1;
  if (code === "Digit0") return 0x27;
  if ((m = code.match(/^F(\d+)$/)) && +m[1] >= 1 && +m[1] <= 12) return 0x3a + Number(m[1]) - 1;
  const map: Record<string, number> = {
    Enter: 0x28, Escape: 0x29, Backspace: 0x2a, Tab: 0x2b, Space: 0x2c, Minus: 0x2d, Equal: 0x2e,
    BracketLeft: 0x2f, BracketRight: 0x30, Backslash: 0x31, Semicolon: 0x33, Quote: 0x34, Backquote: 0x35,
    Comma: 0x36, Period: 0x37, Slash: 0x38, CapsLock: 0x39, Insert: 0x49, Home: 0x4a, PageUp: 0x4b,
    Delete: 0x4c, End: 0x4d, PageDown: 0x4e, ArrowRight: 0x4f, ArrowLeft: 0x50, ArrowDown: 0x51, ArrowUp: 0x52,
    ControlLeft: 0xe0, ShiftLeft: 0xe1, AltLeft: 0xe2, MetaLeft: 0xe3,
    ControlRight: 0xe4, ShiftRight: 0xe5, AltRight: 0xe6, MetaRight: 0xe7,
  };
  return map[code] ?? null;
}

/** Char -> [usage, needsShift] for text->macro. */
export function charToUsage(ch: string): [number, boolean] | null {
  if (/[a-z]/.test(ch)) return [0x04 + ch.charCodeAt(0) - 97, false];
  if (/[A-Z]/.test(ch)) return [0x04 + ch.charCodeAt(0) - 65, true];
  if (/[1-9]/.test(ch)) return [0x1e + Number(ch) - 1, false];
  if (ch === "0") return [0x27, false];
  const si = "!@#$%^&*()".indexOf(ch);
  if (si >= 0) return [si === 9 ? 0x27 : 0x1e + si, true];
  const map: Record<string, [number, boolean]> = {
    " ": [0x2c, false], "\n": [0x28, false], "\t": [0x2b, false],
    "-": [0x2d, false], _: [0x2d, true], "=": [0x2e, false], "+": [0x2e, true],
    "[": [0x2f, false], "{": [0x2f, true], "]": [0x30, false], "}": [0x30, true],
    "\\": [0x31, false], "|": [0x31, true], ";": [0x33, false], ":": [0x33, true],
    "'": [0x34, false], '"': [0x34, true], "`": [0x35, false], "~": [0x35, true],
    ",": [0x36, false], "<": [0x36, true], ".": [0x37, false], ">": [0x37, true],
    "/": [0x38, false], "?": [0x38, true],
  };
  return map[ch] ?? null;
}

/** Short label for an 8-bit usage (macro rows). */
export const usageLabel = (u: number) => HID_NAMES[u] ?? `0x${u.toString(16)}`;
