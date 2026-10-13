#!/usr/bin/env python3
"""Compare the C lighting model against recordings of a real V3 at identical timestamps."""
import subprocess, sys, numpy as np
sys.path.insert(0, '../../tools')
from fx import load
def model(args, times_ms):
    p = subprocess.run(['./test/fx_dump'] + [str(a) for a in args], input='\n'.join(str(int(t)) for t in times_ms).encode(), capture_output=True)
    return np.frombuffer(p.stdout, np.uint8).reshape(len(times_ms), 104, 3).astype(int)
cases = [('Ray auto g2', 'back_e1_g2_auto', (1, 2, 1, 0, 0, 0, 100)), ('Ray auto g0', 'back_e1_g0_auto', (1, 0, 1, 0, 0, 0, 100)), ('Ray auto g4', 'back_e1_g4_auto', (1, 4, 1, 0, 0, 0, 100)),
         ('Ray red', 'back_e1_g2_red', (1, 2, 0, 255, 0, 0, 100)), ('Ray blue', 'col_e1_blue', (1, 2, 0, 0, 0, 255, 100)), ('Ray orange', 'col_e1_orange', (1, 2, 0, 255, 128, 0, 100)),
         ('Stair auto', 'back_e2_g2_auto', (2, 2, 1, 0, 0, 0, 100)), ('Stair red', 'back_e2_g2_red', (2, 2, 0, 255, 0, 0, 100)),
         ('Static auto', 'back_e3_g2_auto', (3, 2, 1, 0, 0, 0, 100)), ('Static mix', 'col_e3_mix', (3, 2, 0, 128, 64, 32, 100)),
         ('Breath auto', 'back_e4_g2_auto', (4, 2, 1, 0, 0, 0, 100)), ('Breath red', 'back_e4_g2_red', (4, 2, 0, 255, 0, 0, 100)),
         ('Wave auto', 'back_e6_g2_auto', (6, 2, 1, 0, 0, 0, 100)), ('Wave red', 'back_e6_g2_red', (6, 2, 0, 255, 0, 0, 100)), ('Wave g4', 'back_e6_g4_auto', (6, 4, 1, 0, 0, 0, 100)),
         ('Ripple auto', 'back_e7_g2_auto', (7, 2, 1, 0, 0, 0, 100)), ('Spout auto', 'back_e8_g2_auto', (8, 2, 1, 0, 0, 0, 100)), ('Galaxy auto', 'back_e9_g2_auto', (9, 2, 1, 0, 0, 0, 100)),
         ('Rotation auto', 'back_e10_g2_auto', (10, 2, 1, 0, 0, 0, 100)), ('Time auto', 'back_e14_g2_auto', (14, 2, 1, 0, 0, 0, 100)), ('Time g4', 'back_e14_g4_auto', (14, 4, 1, 0, 0, 0, 100)),
         ('Gaming auto', 'back_e17_g2_auto', (17, 2, 1, 0, 0, 0, 100)), ('Identify auto', 'back_e18_g2_auto', (18, 2, 1, 0, 0, 0, 100))]
tot = []
print('%-16s %9s %12s %s' % ('effect', 'rms (0-255)', 'LEDs within 12', '(83 key LEDs, 8 s of frames)'))
for name, rec, args in cases:
    t, rgb = load(rec); idx = np.arange(0, len(t), max(1, len(t) // 80)); tt = np.round(t[idx] * 1000)
    m = model(args, tt)[:, :83]; r = rgb[idx, :83].astype(int)
    err = np.abs(m - r); rms = np.sqrt((err ** 2).mean()); ok = (err.max(-1) <= 12).mean()
    tot.append((name, rms, ok)); print('%-16s %9.1f %11.0f%%' % (name, rms, ok * 100))
print('mean fraction of LEDs within 12/255: %.0f%%' % (100 * np.mean([o for _, _, o in tot])))
