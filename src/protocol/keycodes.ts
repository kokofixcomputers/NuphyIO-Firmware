/**
 * Key library: 16-bit keymap entries with display names, grouped like the official app's picker.
 * 0x00uu = HID usage (verified on the keyboard), 0xA5-0xBE = consumer/system keys (QMK numbering,
 * verified for mute/volume/transport), 0x52xx = momentary layer, 0x7Exx = NuPhy firmware specials.
 */
export interface KeyDef { entry: number; label: string; name: string }
export interface KeyGroup { title: string; keys: KeyDef[] }
export interface Category { id: string; label: string; groups: KeyGroup[] }

const k = (entry: number, label: string, name = label): KeyDef => ({ entry, label, name });
const range = (from: number, n: number, fmt: (i: number) => [string, string?]): KeyDef[] =>
  Array.from({ length: n }, (_, i) => { const [l, nm] = fmt(i); return k(from + i, l, nm ?? l); });

const letters = range(0x04, 26, (i) => [String.fromCharCode(65 + i)]);
const digits = [...range(0x1e, 9, (i) => [String(i + 1)]), k(0x27, "0")];
const punct = [
  k(0x28, "Enter"), k(0x29, "Esc"), k(0x2a, "Bksp", "Backspace"), k(0x2b, "Tab"), k(0x2c, "Space"),
  k(0x2d, "-", "Minus"), k(0x2e, "=", "Equals"), k(0x2f, "[", "Left bracket"), k(0x30, "]", "Right bracket"),
  k(0x31, "\\", "Backslash"), k(0x33, ";", "Semicolon"), k(0x34, "'", "Quote"), k(0x35, "`", "Grave"),
  k(0x36, ",", "Comma"), k(0x37, ".", "Period"), k(0x38, "/", "Slash"), k(0x39, "Caps", "Caps Lock"),
  k(0x32, "# ISO", "Non-US hash"), k(0x64, "\\ ISO", "Non-US backslash"), k(0x65, "Menu", "Application / Menu"),
];
const fkeys = [...range(0x3a, 12, (i) => [`F${i + 1}`]), ...range(0x68, 12, (i) => [`F${i + 13}`])];
const nav = [
  k(0x46, "PrtSc", "Print Screen"), k(0x47, "ScrLk", "Scroll Lock"), k(0x48, "Pause"),
  k(0x49, "Ins", "Insert"), k(0x4a, "Home"), k(0x4b, "PgUp", "Page Up"), k(0x4c, "Del", "Delete"), k(0x4d, "End"), k(0x4e, "PgDn", "Page Down"),
  k(0x50, "←", "Left"), k(0x52, "↑", "Up"), k(0x51, "↓", "Down"), k(0x4f, "→", "Right"),
];
const mods = [
  k(0xe0, "Ctrl", "Left Control"), k(0xe1, "Shift", "Left Shift"), k(0xe2, "Alt", "Left Alt / Option"), k(0xe3, "Cmd", "Left Cmd / Win"),
  k(0xe4, "R Ctrl", "Right Control"), k(0xe5, "R Shift", "Right Shift"), k(0xe6, "R Alt", "Right Alt / Option"), k(0xe7, "R Cmd", "Right Cmd / Win"),
];
const numpad = [
  k(0x53, "Num", "Num Lock"), k(0x54, "KP /", "Keypad /"), k(0x55, "KP *", "Keypad *"), k(0x56, "KP -", "Keypad -"), k(0x57, "KP +", "Keypad +"), k(0x58, "KP Ent", "Keypad Enter"),
  ...range(0x59, 9, (i) => [`KP ${i + 1}`, `Keypad ${i + 1}`]), k(0x62, "KP 0", "Keypad 0"), k(0x63, "KP .", "Keypad ."), k(0x67, "KP =", "Keypad ="), k(0x85, "KP ,", "Keypad comma"),
];
const intl = [k(0x87, "Ro"), k(0x88, "Kana"), k(0x89, "Yen"), k(0x8a, "Henkan"), k(0x8b, "Muhenkan")];
const media = [
  k(0xa8, "Mute"), k(0xa9, "Vol +", "Volume up"), k(0xaa, "Vol −", "Volume down"),
  k(0xac, "Prev", "Previous track"), k(0xae, "Play", "Play / Pause"), k(0xab, "Next", "Next track"), k(0xad, "Stop", "Media stop"),
  k(0xbc, "Rew", "Rewind"), k(0xbb, "FFwd", "Fast forward"), k(0xaf, "Select", "Media select"), k(0xb0, "Eject"),
  k(0xbd, "Bri +", "Brightness up"), k(0xbe, "Bri −", "Brightness down"),
];
const apps = [
  k(0xb1, "Mail"), k(0xb2, "Calc", "Calculator"), k(0xb3, "My PC", "My Computer"), k(0xb4, "Search", "Web search"),
  k(0xb5, "Web Home"), k(0xb6, "Back", "Web back"), k(0xb7, "Fwd", "Web forward"), k(0xb8, "Web Stop"), k(0xb9, "Refresh", "Web refresh"), k(0xba, "Favs", "Web favorites"),
];
const system = [k(0xa5, "Power"), k(0xa6, "Sleep"), k(0xa7, "Wake")];

/** Mouse buttons and scroll: plain QMK codes (0xD1 left, 0xD2 right, 0xD3 middle, 0xD4 back, 0xD5 forward, 0xD9-0xDC scroll). */
const mouse: KeyDef[] = [
  k(0xd1, "Left", "Mouse left click"), k(0xd3, "Middle", "Mouse middle click"), k(0xd2, "Right", "Mouse right click"),
  k(0xd4, "Back", "Mouse back (button 4)"), k(0xd5, "Fwd", "Mouse forward (button 5)"),
  k(0xd9, "Scroll ↑", "Scroll up (repeats while held)"), k(0xda, "Scroll ↓", "Scroll down (repeats while held)"),
  k(0xdb, "Scroll ←", "Scroll left (repeats while held)"), k(0xdc, "Scroll →", "Scroll right (repeats while held)"),
];

/** NuPhy firmware specials (values from the official app's keycode table; names from its strings). */
const special: KeyDef[] = [
  k(0x7e06, "Mission", "Mac Mission Control"), k(0x7e07, "Spotlight", "Mac Spotlight search (Cmd+Space)"), k(0x7e08, "Dictate ", "Mac dictation / voice control"),
  k(0x7e09, "Launchpad", "Mac Launchpad / show apps"), k(0x7e0a, "Shot", "Mac screenshot (Cmd+Shift+3)"), k(0x7e0b, "Shot area", "Mac screenshot area (Cmd+Shift+4)"), k(0x7e16, "DND", "Mac Do Not Disturb"),
  k(0x7e03, "BLE 1", "Bluetooth channel 1 (hold to pair)"), k(0x7e04, "BLE 2", "Bluetooth channel 2 (hold to pair)"), k(0x7e05, "BLE 3", "Bluetooth channel 3 (hold to pair)"), k(0x7e02, "2.4G", "2.4 GHz dongle (hold to pair)"),
  k(0x7827, "Bri ▲", "Back-light brightness up"), k(0x7828, "Bri ▼", "Back-light brightness down"), k(0x7821, "FX ▶", "Next back-light effect"),
  k(0x7829, "Spd ▲", "Back-light speed up"), k(0x782a, "Spd ▼", "Back-light speed down"), k(0x7823, "Hue ▶", "Back-light hue (custom colour)"),
  k(0x7e0d, "Side ▲", "Side-light brightness up"), k(0x7e0e, "Side ▼", "Side-light brightness down"), k(0x7e0f, "Side FX", "Next side-light effect"),
  k(0x7e10, "Side Hue", "Side-light hue (custom colour)"), k(0x7e11, "Side Spd ▲", "Side-light speed up"), k(0x7e12, "Side Spd ▼", "Side-light speed down"),
  k(0x7e13, "Reset", "Hold 3 s: factory reset"), k(0x7e14, "Sleep", "Toggle auto sleep"), k(0x7e19, "RGB Test", "Hold: cycle all LEDs red / green / blue / white"),
  k(0x7e15, "Battery", "Left side light shows the battery level (toggle); the right one stays under software control"), k(0x7e3b, "Caps LED", "Toggle caps-lock indicator"), k(0x7e3c, "Side LED", "Toggle side light"),
  k(0x7e3d, "F-Rev", "Swap F-row / media keys"), k(0x7e46, "Touch", "Toggle touch bar"),
  k(0x7e3e, "Mac SRT", "Mac screenshot / record"), k(0x7e40, "Lock ⌘", "Mac lock screen"), k(0x7e3f, "Lock ⊞", "Windows lock screen"),
  k(0x7e41, "Finder", "Mac Finder search"), k(0x7e42, "Quit", "Mac force quit"), k(0x7e43, "Emoji", "Mac emoji picker"), k(0x7e44, "Desktop", "Mac back to desktop"),
  k(0x7e45, "TaskMgr", "Windows task manager"), k(0x7e47, "◫← Win", "Windows desktop left"), k(0x7e48, "◫→ Win", "Windows desktop right"),
  k(0x7e49, "◫← Mac", "Mac desktop left"), k(0x7e4a, "◫→ Mac", "Mac desktop right"), k(0x7e4f, "Ctrl Ctr", "Windows control center"),
  k(0x7e52, "Show Dsk", "Windows show desktop"), k(0x7e53, "Lang", "Mac language switch"), k(0x7e54, "Dictate", "Windows dictation"), k(0x7e55, "Flexo", "Short press \\, long press Shift"),
];

export const CATEGORIES: Category[] = [
  { id: "basic", label: "Basic", groups: [
    { title: "Letters", keys: letters }, { title: "Numbers", keys: digits }, { title: "Punctuation & editing", keys: punct },
    { title: "Function keys", keys: fkeys }, { title: "Navigation", keys: nav }, { title: "Modifiers", keys: mods }, { title: "International", keys: intl },
  ] },
  { id: "numpad", label: "Numpad", groups: [{ title: "Keypad", keys: numpad }] },
  { id: "media", label: "Media", groups: [{ title: "Playback & volume", keys: media }, { title: "Apps & browser", keys: apps }, { title: "System", keys: system }] },
  { id: "mouse", label: "Mouse", groups: [{ title: "Buttons & scroll", keys: mouse }] },
  { id: "special", label: "Special", groups: [{ title: "NuPhy functions", keys: special }] },
];

export const KEY_INDEX = new Map<number, KeyDef>(CATEGORIES.flatMap((c) => c.groups.flatMap((g) => g.keys)).map((d) => [d.entry, d]));

/** Entries usable as a macro event (8-bit HID usages only). */
export const MACRO_CATEGORY_IDS = ["basic", "numpad"];
export const isMacroKey = (entry: number) => entry > 0 && entry < 0xa5 || (entry >= 0xe0 && entry <= 0xe7);
