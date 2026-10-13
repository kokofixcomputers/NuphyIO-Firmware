#!/usr/bin/env python3
"""Generate src/boards/<board>/board_data.h from the QMK sources + our captured V3 keymap.

    python3 tools/gen_board.py air75v2|halo75v2|air96v2 [path/to/qmk-firmware]

Everything hardware-specific (matrix pins, LED wiring, key -> V3 slot map, LED driver registers, halo ring order) is derived from
keyboards/nuphy/<board>/ansi/{keyboard.json,ansi.c,side.c} so nothing is transcribed by hand.

The Air96 V2 must be generated from NuPhy's official tree (git clone https://github.com/nuphy-src/qmk_firmware): the community QMK tree
does not run on that board. The official tree names the directory air96_v2 and keeps the IS31FL3733 wiring in keymaps/default/keymap.c.
"""
import json, os, re, sys

BOARD = sys.argv[1] if len(sys.argv) > 1 else "air75v2"
QMK = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/qmk-firmware")
SRC = next(d for d in (f"{QMK}/keyboards/nuphy/{BOARD}/ansi", f"{QMK}/keyboards/nuphy/{BOARD.replace('v2', '_v2')}/ansi") if os.path.isdir(d))
kb = json.load(open(f"{SRC}/keyboard.json"))
ROWS, COLS = len(kb["matrix_pins"]["rows"]), len(kb["matrix_pins"]["cols"])

def run(a, b): return list(range(a, b + 1))
# V3 slot geometry (must match src/layout.ts in the web app)
V3_ROWS = [
    run(0, 14),
    run(16, 28) + [29, 31],
    [32] + run(33, 44) + [45, 47],
    [48] + run(49, 59) + [61, 63],
    [64] + run(66, 75) + [76, 78, 79],
    [80, 81, 82, 86, 90, 91, 92, 93, 94, 95],
]
NP = 1000          # marker: numpad key number i has no V3 slot (V3 slot field = NP + i)
BUTTON = 60       # nuphy.io addresses the top-right button / knob push at slot 60

CFG = {
    "air75v2": dict(
        name="Air75 V2", usb="Air75 V2 (open firmware)", ble="NuPhy Air75 V2", dongle="NuPhy Air75 V2 Dongle",
        os_high_is_win=0, nrf_wake=("B", 8), driver="ws2812", side_strips=2, strip_len=6, bat_first=0, bat_count=6,
        # V2 keys by x within each y; the V2 has Print / Ins / Del where the V3 has a function key / Ins / knob
        group="by_y", rows=[V3_ROWS[0][:14] + [BUTTON] if False else run(0, 14) + [BUTTON]] + V3_ROWS[1:],
        right_cluster={},
    ),
    "halo75v2": dict(
        name="Halo75 V2", usb="Halo75 V2 (open firmware)", ble="NuPhy Halo75 V2", dongle="NuPhy Halo75 V2 Dongle",
        os_high_is_win=1, nrf_wake=("C", 4), driver="is31fl3733", side_strips=1, strip_len=45, bat_first=18, bat_count=5,
        # layout array order = keymap rows (16,15,15,14,14,9 keys); the Halo has no right Ctrl (V3 slot 92) and its right-hand column
        # is Home / End / PgUp / PgDn from top to bottom where the V3 has PgUp / PgDn / Home / End
        group="by_count", counts=[16, 15, 15, 14, 14, 9],
        rows=[run(0, 14) + [BUTTON]] + V3_ROWS[1:5] + [[80, 81, 82, 86, 90, 91, 93, 94, 95]],
        right_cluster={31: 0x004A, 47: 0x004D, 63: 0x004B, 79: 0x004E},     # slot -> default HID usage on the base layers
    ),
    "air96v2": dict(
        name="Air96 V2", usb="Air96 V2 (open firmware)", ble="NuPhy Air96 V2", dongle="NuPhy Air96 V2 Dongle",
        os_high_is_win=0, nrf_wake=("C", 4), driver="is31fl3733", side_strips=2, strip_len=5, bat_first=0, bat_count=5,
        # nuphy.io has this keyboard as the Air100 V3 (USB PID 0x102D): 6 rows x 21 columns, so its keymap has 6 * 21 + 2 = 128 slots per layer
        # and a key's slot is simply row * 21 + column of the QMK matrix. nuphy.io finds each key's slot by looking for the key's default
        # usage in command B1, so the factory defaults only have to contain the right usages.
        # Each y-row of the layout is the Air75's key block (V3 slot numbers, used for the effect model) followed by extras: row 0 ends
        # with Home / End / PgUp / PgDn, every other row ends with its numpad keys (NP + index, no V3 slot).
        group="by_y", numpad=True, slot_cols=21, v3_leds=99, button_proto=(2, 14),
        rows=[run(0, 14) + [31, 47, 63, 79],
              run(16, 28) + [29] + [NP + i for i in range(0, 4)],
              [32] + run(33, 44) + [45] + [NP + i for i in range(4, 8)],
              [48] + run(49, 59) + [61] + [NP + i for i in range(8, 11)],
              [64] + run(66, 75) + [76, 78] + [NP + i for i in range(11, 15)],
              [80, 81, 82, 86, 90, 91, 92, 93, 94, 95] + [NP + 15, NP + 16]],
        right_cluster={31: 0x004A, 47: 0x004D, 63: 0x004B, 79: 0x004E},
    ),
}[BOARD]
SLOT_COLS = CFG.get("slot_cols", 16)
SLOTS = 6 * SLOT_COLS + 2                  # keymap slots per layer
NUMPAD_USAGE = [0x53, 0x54, 0x55, 0x56, 0x5F, 0x60, 0x61, 0x57, 0x5C, 0x5D, 0x5E, 0x59, 0x5A, 0x5B, 0x58, 0x62, 0x63]   # keypad usages in ext-slot order

layout = list(kb["layouts"].values())[0]["layout"]
slot_of, label_of = {}, {}
if CFG["group"] == "by_y":
    by_row = {}
    for k in layout: by_row.setdefault(k["y"], []).append(k)
    groups = []
    for y in sorted(by_row): groups.append(sorted(by_row[y], key=lambda k: k["x"]))
else:
    groups, i = [], 0
    for n in CFG["counts"]: groups.append(layout[i:i + n]); i += n
for keys, v3 in zip(groups, CFG["rows"]):
    assert len(keys) == len(v3), (len(keys), len(v3))
    for k, s in zip(keys, v3):
        r, c = k["matrix"]; slot_of[(r, c)] = s; label_of[(r, c)] = k["label"]

key_leds = [l for l in kb["rgb_matrix"]["layout"] if "matrix" in l]           # entries without a matrix position are the side LEDs
led_chain = [tuple(l["matrix"]) for l in key_leds]
KEY_LEDS = len(led_chain)
assert all(m in slot_of for m in led_chain)

proto_of = {(r, c): (slot_of[(r, c)] if SLOT_COLS == 16 else r * SLOT_COLS + c) for (r, c) in slot_of}      # slot used on the wire

# ---- default keymap: start from the V3 layer data we captured, remapped onto this board -------------
b2 = open(os.path.join(os.path.dirname(__file__), "v3_keymap_b2.bin"), "rb").read()  # real V3 keymap dump (1568 B)
assert len(b2) == 1568
v3d = bytearray(b2)                           # 8 layers x 98 slots x 2 bytes, big-endian entries
def put(layer, slot, entry):
    o = layer * 196 + slot * 2
    v3d[o], v3d[o + 1] = entry >> 8, entry & 0xFF
for layer in range(8):
    base_layer = layer % 4 == 0
    # slot 60 = Del, the top-right button (nuphy.io addresses a button / knob push at that slot). Slot 13 keeps the V3's own screenshot functions.
    put(layer, 15, 0x0000)
    if BOARD == "air96v2": put(layer, 14, 0x004C if base_layer else 0x0001)           # the Air96 has a real Del key where the V3 has Ins
    else: put(layer, 60, 0x004C if base_layer else 0x0001)
    put(layer, 96, 0x0000); put(layer, 97, 0x0000)
    for slot, usage in CFG["right_cluster"].items():
        if base_layer: put(layer, slot, usage)
    if BOARD == "halo75v2": put(layer, 92, 0x0000)            # no right Ctrl on the Halo
if SLOT_COLS == 16:
    default = v3d
else:                                          # re-slot: every key's V3 layer entries move to its wire slot; numpad keys get keypad usages
    default = bytearray(8 * SLOTS * 2)
    for layer in range(8):
        for (r, c), v in slot_of.items():
            o = (layer * SLOTS + proto_of[(r, c)]) * 2
            if v >= NP: e = NUMPAD_USAGE[v - NP] if layer % 4 == 0 else 0x0001
            else: e = (v3d[layer * 196 + v * 2] << 8) | v3d[layer * 196 + v * 2 + 1]
            default[o], default[o + 1] = e >> 8, e & 0xFF
        if layer % 4 == 0:                    # hidden knob-turn entries nuphy.io's Air100 V3 view looks for (volume down / up); no key sends them
            for slot, usage in ((SLOTS - 2, 0xAA), (SLOTS - 1, 0xA9)): default[(layer * SLOTS + slot) * 2 + 1] = usage

# ---- IS31FL3733 wiring (Halo): LED index -> {chip, R, G, B PWM register} -----------------------------
SW_CS = lambda sw, cs: (int(sw) - 1) * 16 + int(cs) - 1
is31 = []
ring = []
if CFG["driver"] == "is31fl3733":
    if BOARD == "halo75v2":                                           # community tree: SWx_CSy table in ansi.c, ring order in side.c
        t = open(f"{SRC}/ansi.c").read()
        body = t[t.index("g_is31fl3733_leds"):]
        for m in re.finditer(r"\{\s*(\d)\s*,\s*SW(\d+)_CS(\d+)\s*,\s*SW(\d+)_CS(\d+)\s*,\s*SW(\d+)_CS(\d+)\s*\}", body):
            d, s1, c1, s2, c2, s3, c3 = m.groups()
            is31.append((int(d), SW_CS(s1, c1), SW_CS(s2, c2), SW_CS(s3, c3)))
        s = open(f"{SRC}/side.c").read()
        tab = s[s.index("power_led_index_tab"):]
        tab = tab[tab.index("{"):tab.index("};")]
        ring = [int(x) for x in re.findall(r"SIDE_INDEX \+ (\d+)", tab)]
        assert len(ring) == 45 and sorted(ring) == list(range(45)), ring
    else:                                                             # official tree: {chip, A_16, B_16, C_16} (A = SW1 ... L = SW12) in keymaps/default/keymap.c
        t = open(f"{SRC}/keymaps/default/keymap.c").read()
        body = t[t.index("g_is31_leds"):]
        reg = lambda letter, cs: (ord(letter) - ord("A")) * 16 + int(cs) - 1
        for m in re.finditer(r"\{\s*(\d)\s*,\s*([A-L])_(\d+)\s*,\s*([A-L])_(\d+)\s*,\s*([A-L])_(\d+)\s*\}", body):
            d, l1, c1, l2, c2, l3, c3 = m.groups()
            is31.append((int(d), reg(l1, c1), reg(l2, c2), reg(l3, c3)))
        ring = list(range(CFG["side_strips"] * CFG["strip_len"]))     # side LEDs are in chain order: left strip bottom -> top, then right strip top -> bottom
SIDE_LEDS = CFG["side_strips"] * CFG["strip_len"]

# live LED list (command D2): the host expects the keys in its own order, then the accessory's LED last. Air75-family: ascending V3 slot,
# accessory (slot 60 / Del) = 83. Air96 (Air100 V3 view): reading order of the 98 keys nuphy.io draws (row by row, left to right), Del = 98.
snap = {}
if SLOT_COLS == 16:
    order = sorted(x for row in V3_ROWS for x in row)        # the V3's 83 keys; a board that lacks one still leaves its place free
    for m, v in slot_of.items(): snap[m] = order.index(v) if v in order else len(order)
    SNAP_KEYS = len(order) + 1
    BUTTON_PROTO = BUTTON
else:
    by_y = {}
    for (r, c), v in slot_of.items():
        k = next(k for k in layout if tuple(k["matrix"]) == (r, c)); by_y.setdefault(k["y"], []).append((k["x"], (r, c), v))
    seq = []
    for y in sorted(by_y):
        for x, m, v in sorted(by_y[y]):
            if v in (13, 14): continue                    # Print and Del are not among nuphy.io's 98 drawn keys
            seq.append(m)
    for i, m in enumerate(seq): snap[m] = i
    bm = CFG["button_proto"]; snap[bm] = len(seq)           # Del = the accessory (knob press) LED
    SNAP_KEYS = len(seq) + 1
    assert SNAP_KEYS == CFG["v3_leds"], (SNAP_KEYS, CFG["v3_leds"])
    BUTTON_PROTO = bm[0] * SLOT_COLS + bm[1]

cfg = []
w = cfg.append
w(f"/* GENERATED by tools/gen_board.py {BOARD} from the QMK tree - do not edit. */")
w("#pragma once\n#include <stdint.h>\n")
w(f"#define BD_{BOARD.upper()} 1")
w(f"#define BD_ROWS {ROWS}\n#define BD_COLS {COLS}\n#define BD_KEY_LEDS {KEY_LEDS}\n#define BD_SIDE_LEDS {SIDE_LEDS}")
w(f"#define BD_SIDE_STRIPS {CFG['side_strips']}\n#define BD_SIDE_STRIP_LEN {CFG['strip_len']}")
w(f"#define BD_BAT_FIRST {CFG['bat_first']}\n#define BD_BAT_COUNT {CFG['bat_count']}      /* side LEDs the battery bar uses */")
w(f"#define BD_OS_HIGH_IS_WIN {CFG['os_high_is_win']}")
w(f"#define BD_SLOT_COLS {SLOT_COLS}      /* keymap slots are row * BD_SLOT_COLS + column; reported to the host as the matrix width */")
w(f"#define BD_SLOTS {SLOTS}")
w(f"#define BD_KEYMAP_BYTES {8 * SLOTS * 2}")
w(f"#define BD_V3_KEY_LEDS {SNAP_KEYS}      /* key LEDs in the live LED list the host reads (the accessory's LED is the last one) */")
if BUTTON_PROTO is not None: w(f"#define BD_BUTTON_SLOT {BUTTON_PROTO}      /* the top-right key (the V2's button / Del) */")
if SLOT_COLS == 16: w("#define BD_BUTTON_IS_MUTE 1      /* nuphy.io's Air75 V3 view only accepts the corner key when its factory default is volume-mute */")
port, pin_ = CFG["nrf_wake"]
w(f"#define BD_NRF_WAKE_PORT {ord(port) - ord('A')}\n#define BD_NRF_WAKE_PIN {pin_}")
w(f'#define BD_NAME "{CFG["name"]}"\n#define BD_USB_NAME "{CFG["usb"]}"\n#define BD_BLE_NAME "{CFG["ble"]}"\n#define BD_DONGLE_NAME "{CFG["dongle"]}"')
w(f"#define BD_LED_DRIVER_{CFG['driver'].upper()} 1")
w("")
out = []
w = out.append
w(f"/* GENERATED by tools/gen_board.py {BOARD} from the QMK tree - do not edit. */")
w("#pragma once\n#include \"board_cfg.h\"\n")
def pin(name): return (ord(name[0]) - ord("A"), int(name[1:]))      # (port index A=0, pin number)
w("/* matrix pins from keyboard.json: {port (A=0,B=1,C=2), pin}. Rows are driven low one at a time, columns read with pull-ups. */")
w("static const uint8_t BD_ROW_PIN[BD_ROWS][2] = {" + ", ".join("{%d,%d}" % pin(n) for n in kb["matrix_pins"]["rows"]) + "};")
w("static const uint8_t BD_COL_PIN[BD_COLS][2] = {" + ", ".join("{%d,%d}" % pin(n) for n in kb["matrix_pins"]["cols"]) + "};\n")
w("/* matrix (row, col) -> keymap slot on the wire; 0xFF = no key */")
w(f"static const uint8_t BD_SLOT[{ROWS}][{COLS}] = {{")
for r in range(ROWS):
    w("    {" + ", ".join(f"0x{proto_of[(r, c)]:02X}" if (r, c) in proto_of else "0xFF" for c in range(COLS)) + "},")
w("};\n")
w("/* LED chain index (wiring order) -> keymap slot on the wire */")
w(f"static const uint8_t BD_LED_SLOT[BD_KEY_LEDS] = {{" + ", ".join(str(proto_of[m]) for m in led_chain) + "};")
w("/* LED chain index -> the Air75 V3 key whose lighting this key copies (effect model); 0xFF = none, borrow the nearest key */")
w(f"static const uint8_t BD_LED_V3[BD_KEY_LEDS] = {{" + ", ".join(str(slot_of[m]) if slot_of[m] < NP else "0xFF" for m in led_chain) + "};")
w("/* LED chain index -> position in the live LED list the host reads (command D2); 0xFF = not in the list */")
w(f"static const uint8_t BD_LED_SNAP[BD_KEY_LEDS] = {{" + ", ".join(str(snap[m]) if m in snap else "0xFF" for m in led_chain) + "};\n")
w("/* LED chain index -> physical position, in QMK's 0..224 x 0..64 units (for spatial effects) */")
w(f"static const uint8_t BD_LED_X[BD_KEY_LEDS] = {{" + ", ".join(str(int(round(l["x"]))) for l in key_leds) + "};")
w(f"static const uint8_t BD_LED_Y[BD_KEY_LEDS] = {{" + ", ".join(str(int(round(l["y"]))) for l in key_leds) + "};\n")
if is31:
    N = len(is31)
    assert N == KEY_LEDS + SIDE_LEDS, (N, KEY_LEDS, SIDE_LEDS)
    w(f"/* IS31FL3733: LED index (0..{KEY_LEDS - 1} keys in chain order, then the side LEDs) -> chip and the PWM register of its red / green / blue die */")
    w(f"#define BD_IS31_N {N}")
    w(f"static const uint8_t BD_IS31_CHIP[{N}] = {{" + ", ".join(str(d) for d, *_ in is31) + "};")
    for name, idx in (("R", 1), ("G", 2), ("B", 3)):
        w(f"static const uint8_t BD_IS31_{name}[{N}] = {{" + ", ".join(str(t[idx]) for t in is31) + "};")
    w(f"/* side light position (0..{SIDE_LEDS - 1}: the ring going round the Halo, or strip by strip) -> side LED number (LED index {KEY_LEDS} + n) */")
    w(f"static const uint8_t BD_RING[{SIDE_LEDS}] = {{" + ", ".join(str(n) for n in ring) + "};\n")
w("/* default keymap image: 8 layers x BD_SLOTS slots, big-endian 16-bit entries */")
w("static const uint8_t BD_DEFAULT_KEYMAP[BD_KEYMAP_BYTES] = {")
for i in range(0, len(default), 16):
    w("    " + ", ".join(f"0x{b:02X}" for b in default[i:i + 16]) + ",")
w("};")
dst = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "boards", BOARD)
os.makedirs(dst, exist_ok=True)
open(os.path.join(dst, "board_data.h"), "w").write("\n".join(out) + "\n")
open(os.path.join(dst, "board_cfg.h"), "w").write("\n".join(cfg) + "\n")
print(f"wrote src/boards/{BOARD}/board_data.h:", len(slot_of), "keys mapped,", KEY_LEDS, "key LEDs" + (f", {SIDE_LEDS} side LEDs" if is31 else ""))
for r in range(ROWS):
    print(" row", r, [(c, label_of[(r, c)], slot_of[(r, c)]) for c in range(COLS) if (r, c) in slot_of][:8], "…")
