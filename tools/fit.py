import numpy as np, glob
from scipy.optimize import minimize
from fx import *; from geom import *
OK = ~np.isnan(XY[:, 0])
def circ_fit(t, rgb, sub=40):
    """Fit hue(deg) ~ a*x + b*y + c*t + d on key LEDs. Returns (a, b, c, d, coherence 0..1)."""
    h = hue(rgb[:, :84]); idx = np.linspace(0, len(t) - 1, sub).astype(int)
    T = t[idx][:, None]; X = XY[:, 0][None, :]; Y = XY[:, 1][None, :]
    H = np.radians(h[idx]); m = ~np.isnan(H) & OK[None, :]
    def score(p):
        a, b, c = p
        ph = H - np.radians(a * X + b * Y + c * T)
        z = np.where(m, np.exp(1j * ph), 0).sum() / max(m.sum(), 1)
        return -abs(z)
    best = None
    for c0 in np.arange(-420, 421, 12):
        for a0 in np.arange(-60, 61, 6):
            for b0 in (-40, -20, 0, 20, 40):
                s = score((a0, b0, c0))
                if best is None or s < best[0]: best = (s, (a0, b0, c0))
    r = minimize(score, best[1], method='Nelder-Mead', options={'xatol': 0.01, 'fatol': 1e-6, 'maxiter': 2000})
    a, b, c = r.x
    ph = H - np.radians(a * X + b * Y + c * T)
    d = np.degrees(np.angle(np.where(m, np.exp(1j * ph), 0).sum()))
    return a, b, c, d, -r.fun

def drift_fit(t, rgb, rates=np.arange(-420, 421, 0.5)):
    """Find the hue drift rate (deg/s) at which every key LED's hue is a fixed offset plus rate*t.
    Returns (rate, coherence, offsets_deg[83]) - offsets are circular means of hue - rate*t."""
    h = np.radians(hue(rgb[:, :83])); m = ~np.isnan(h)
    best = (0, -1, None)
    for r in rates:
        rel = h - np.radians(r) * t[:, None]
        z = np.where(m, np.exp(1j * rel), 0).sum(0) / np.maximum(m.sum(0), 1)
        ok = m.sum(0) > len(t) * 0.5
        coh = np.abs(z[ok]).mean() if ok.any() else 0
        if coh > best[1]: best = (r, coh, np.degrees(np.angle(z)) % 360)
    return best
