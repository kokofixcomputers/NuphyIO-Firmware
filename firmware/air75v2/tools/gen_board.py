#!/usr/bin/env python3
"""Generate src/boards/<board>/board_data.h from the QMK sources + our captured V3 keymap.

    python3 tools/gen_board.py air75v2|halo75v2 [path/to/qmk-firmware]

Everything hardware-specific (matrix pins, LED wiring, key -> V3 slot map, LED driver registers, halo ring order) is derived from
keyboards/nuphy/<board>/ansi/{keyboard.json,ansi.c,side.c} so nothing is transcribed by hand.
"""
import json, os, re, sys

BOARD = sys.argv[1] if len(sys.argv) > 1 else "air75v2"
QMK = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/qmk-firmware")
SRC = f"{QMK}/keyboards/nuphy/{BOARD}/ansi"
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
}[BOARD]

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

led_chain = [tuple(l["matrix"]) for l in kb["rgb_matrix"]["layout"]]
KEY_LEDS = len(led_chain)
assert all(m in slot_of for m in led_chain)

# ---- default keymap: start from the V3 layer data we captured, remapped onto this board -------------
b2 = open(os.path.join(os.path.dirname(__file__), "v3_keymap_b2.bin"), "rb").read()  # real V3 keymap dump (1568 B)
assert len(b2) == 1568
default = bytearray(b2)                       # 8 layers x 98 slots x 2 bytes, big-endian entries
def put(layer, slot, entry):
    o = layer * 196 + slot * 2
    default[o], default[o + 1] = entry >> 8, entry & 0xFF
for layer in range(8):
    base_layer = layer % 4 == 0
    # slot 60 = Del, the top-right button (nuphy.io addresses a button / knob push at that slot). Slot 13 keeps the V3's own screenshot functions.
    put(layer, 15, 0x0000)
    put(layer, 60, 0x004C if base_layer else 0x0001)
    put(layer, 96, 0x0000); put(layer, 97, 0x0000)
    for slot, usage in CFG["right_cluster"].items():
        if base_layer: put(layer, slot, usage)
    if BOARD == "halo75v2": put(layer, 92, 0x0000)            # no right Ctrl on the Halo

# ---- IS31FL3733 wiring (Halo): LED index -> {chip, R, G, B PWM register} -----------------------------
SW_CS = lambda sw, cs: (int(sw) - 1) * 16 + int(cs) - 1
is31 = []
ring = []
if CFG["driver"] == "is31fl3733":
    t = open(f"{SRC}/ansi.c").read()
    body = t[t.index("g_is31fl3733_leds"):]
    for m in re.finditer(r"\{\s*(\d)\s*,\s*SW(\d+)_CS(\d+)\s*,\s*SW(\d+)_CS(\d+)\s*,\s*SW(\d+)_CS(\d+)\s*\}", body):
        d, s1, c1, s2, c2, s3, c3 = m.groups()
        is31.append((int(d), SW_CS(s1, c1), SW_CS(s2, c2), SW_CS(s3, c3)))
    assert len(is31) == 128, len(is31)
    s = open(f"{SRC}/side.c").read()
    tab = s[s.index("power_led_index_tab"):]
    tab = tab[tab.index("{"):tab.index("};")]
    ring = [int(x) for x in re.findall(r"SIDE_INDEX \+ (\d+)", tab)]
    assert len(ring) == 45 and sorted(ring) == list(range(45)), ring
SIDE_LEDS = CFG["side_strips"] * CFG["strip_len"]

cfg = []
w = cfg.append
w(f"/* GENERATED by tools/gen_board.py {BOARD} from the QMK tree - do not edit. */")
w("#pragma once\n#include <stdint.h>\n")
w(f"#define BD_{BOARD.upper()} 1")
w(f"#define BD_ROWS {ROWS}\n#define BD_COLS {COLS}\n#define BD_KEY_LEDS {KEY_LEDS}\n#define BD_SIDE_LEDS {SIDE_LEDS}")
w(f"#define BD_SIDE_STRIPS {CFG['side_strips']}\n#define BD_SIDE_STRIP_LEN {CFG['strip_len']}")
w(f"#define BD_BAT_FIRST {CFG['bat_first']}\n#define BD_BAT_COUNT {CFG['bat_count']}      /* side LEDs the battery bar uses */")
w(f"#define BD_OS_HIGH_IS_WIN {CFG['os_high_is_win']}")
port, pin_ = CFG["nrf_wake"]
w(f"#define BD_NRF_WAKE_PORT {ord(port) - ord('A')}\n#define BD_NRF_WAKE_PIN {pin_}")
w(f'#define BD_NAME "{CFG["name"]}"\n#define BD_USB_NAME "{CFG["usb"]}"\n#define BD_BLE_NAME "{CFG["ble"]}"\n#define BD_DONGLE_NAME "{CFG["dongle"]}"')
w(f"#define BD_LED_DRIVER_{CFG['driver'].upper()} 1\n")
out = []
w = out.append
w(f"/* GENERATED by tools/gen_board.py {BOARD} from the QMK tree - do not edit. */")
w("#pragma once\n#include \"board_cfg.h\"\n")
def pin(name): return (ord(name[0]) - ord("A"), int(name[1:]))      # (port index A=0, pin number)
w("/* matrix pins from keyboard.json: {port (A=0,B=1,C=2), pin}. Rows are driven low one at a time, columns read with pull-ups. */")
w("static const uint8_t BD_ROW_PIN[BD_ROWS][2] = {" + ", ".join("{%d,%d}" % pin(n) for n in kb["matrix_pins"]["rows"]) + "};")
w("static const uint8_t BD_COL_PIN[BD_COLS][2] = {" + ", ".join("{%d,%d}" % pin(n) for n in kb["matrix_pins"]["cols"]) + "};\n")
w("/* matrix (row, col) -> V3 keymap slot (row*16+col style numbering used by the V3 protocol); 0xFF = no key */")
w(f"static const uint8_t BD_SLOT[{ROWS}][{COLS}] = {{")
for r in range(ROWS):
    w("    {" + ", ".join(f"0x{slot_of[(r, c)]:02X}" if (r, c) in slot_of else "0xFF" for c in range(COLS)) + "},")
w("};\n")
w("/* LED chain index (wiring order) -> V3 slot. The V3 protocol reports LEDs in ascending slot order. */")
w(f"static const uint8_t BD_LED_SLOT[BD_KEY_LEDS] = {{" + ", ".join(str(slot_of[m]) for m in led_chain) + "};\n")
w("/* LED chain index -> physical position, in QMK's 0..224 x 0..64 units (for spatial effects) */")
w(f"static const uint8_t BD_LED_X[BD_KEY_LEDS] = {{" + ", ".join(str(l["x"]) for l in kb["rgb_matrix"]["layout"]) + "};")
w(f"static const uint8_t BD_LED_Y[BD_KEY_LEDS] = {{" + ", ".join(str(l["y"]) for l in kb["rgb_matrix"]["layout"]) + "};\n")
if is31:
    w("/* IS31FL3733: LED index (0..82 keys in chain order, 83..127 halo) -> chip and the PWM register of its red / green / blue die */")
    w("static const uint8_t BD_IS31_CHIP[128] = {" + ", ".join(str(d) for d, *_ in is31) + "};")
    for name, idx in (("R", 1), ("G", 2), ("B", 3)):
        w(f"static const uint8_t BD_IS31_{name}[128] = {{" + ", ".join(str(t[idx]) for t in is31) + "};")
    w("/* halo ring position (0..44, going round the board) -> halo LED number (0..44, i.e. LED index 83 + n) */")
    w("static const uint8_t BD_RING[45] = {" + ", ".join(str(n) for n in ring) + "};\n")
w("/* default keymap image: 8 layers x 98 slots, big-endian 16-bit entries (V3 layout) */")
w("static const uint8_t BD_DEFAULT_KEYMAP[1568] = {")
for i in range(0, 1568, 16):
    w("    " + ", ".join(f"0x{b:02X}" for b in default[i:i + 16]) + ",")
w("};")
dst = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "boards", BOARD)
os.makedirs(dst, exist_ok=True)
open(os.path.join(dst, "board_data.h"), "w").write("\n".join(out) + "\n")
open(os.path.join(dst, "board_cfg.h"), "w").write("\n".join(cfg) + "\n")
print(f"wrote src/boards/{BOARD}/board_data.h:", len(slot_of), "keys mapped,", KEY_LEDS, "key LEDs" + (", 45 halo LEDs" if is31 else ""))
for r in range(ROWS):
    print(" row", r, [(c, label_of[(r, c)], slot_of[(r, c)]) for c in range(COLS) if (r, c) in slot_of][:8], "…")
