import base64, numpy as np, colorsys, glob, os
def load(name):
    import os
    lines = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), f'rec-{name}.txt')).read().strip().split('\n')
    t = np.array([int(l.split('\t')[0]) for l in lines], float) / 1000
    rgb = np.array([np.frombuffer(base64.b64decode(l.split('\t')[1]), np.uint8).reshape(104, 3) for l in lines])
    return t, rgb
def hue(rgb):                       # degrees 0..360 from an (...,3) uint8 array, nan where grey/black
    x = rgb.astype(float) / 255
    mx, mn = x.max(-1), x.min(-1); d = mx - mn
    r, g, b = x[..., 0], x[..., 1], x[..., 2]
    h = np.where(mx == r, ((g - b) / np.where(d == 0, 1, d)) % 6, np.where(mx == g, (b - r) / np.where(d == 0, 1, d) + 2, (r - g) / np.where(d == 0, 1, d) + 4)) * 60
    return np.where(d < 0.05, np.nan, h)
def val(rgb): return rgb.max(-1)

def board(frame_v, thr=(10, 60, 130)):
    """ASCII picture of per-LED value on the 6x16 board."""
    from geom import XY
    g = [[' .'[0]] * 16 for _ in range(6)]
    g = [['  '] * 16 for _ in range(6)]
    for i in range(84):
        if np.isnan(XY[i, 0]): continue
        c, r = int(XY[i, 0]), int(XY[i, 1]); v = frame_v[i]
        g[r][min(c, 15)] = '##' if v >= thr[2] else '++' if v >= thr[1] else '..' if v >= thr[0] else '  '
    return '\n'.join('|' + ''.join(row) + '|' for row in g)
