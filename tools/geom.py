import json, os, numpy as np
QMK = os.path.expanduser("~/qmk-firmware")
kb = json.load(open(f"{QMK}/keyboards/nuphy/air75v2/ansi/keyboard.json"))
def run(a, b): return list(range(a, b + 1))
V3_ROWS = [run(0,14), run(16,28)+[29,31], [32]+run(33,44)+[45,47], [48]+run(49,59)+[61,63], [64]+run(66,75)+[76,78,79], [80,81,82,86,90,91,92,93,94,95]]
by_row = {}
for k in kb["layouts"]["LAYOUT_75_ansi"]["layout"]: by_row.setdefault(k["y"], []).append(k)
slot_xy = {}
for y, keys in sorted(by_row.items()):
    keys.sort(key=lambda k: k["x"])
    slots = run(0, 15) if int(y) == 0 else V3_ROWS[int(y)]
    for k, s in zip(keys, slots): slot_xy[s] = (k["x"] + k.get("w", 1) / 2, float(y) + 0.5)
# V3 D2 order (verified from the Gaming/Identify effects): the 83 physical keys in ascending slot order, then the knob LED last
V3_SET = [s for s in range(98) if s <= 14 or 16 <= s <= 29 or s == 31 or 32 <= s <= 45 or s == 47 or 48 <= s <= 59 or s in (61, 63, 64) or 66 <= s <= 76 or s in (78, 79, 80, 81, 82, 86) or 90 <= s <= 95]
assert len(V3_SET) == 83
V3_SET = V3_SET + [60]
XY = np.array([slot_xy.get(s, (np.nan, np.nan)) for s in V3_SET])      # (84,2) key-unit centres; row 83 (knob) is NaN
