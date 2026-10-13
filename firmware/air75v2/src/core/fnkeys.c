#include "fnkeys.h"
#include <string.h>

#define BACK 0           /* back light state starts at byte 0 of the 17-byte profile record: [effect, bright, speedRaw, b3, auto, b5, R, G, B] */
#define SIDE 9           /* side light: [effect, bright, speedRaw, auto, b4, R, G, B] */

static void touch(nph_t *n, uint8_t profile, uint8_t off, uint8_t len) { if (n->hooks.mark) n->hooks.mark(NPH_R_LIGHT, (uint16_t)((profile & 1) * NPH_LIGHT_BYTES + off), len); }
static void touch_fw(nph_t *n, uint8_t idx) { if (n->hooks.mark) n->hooks.mark(NPH_R_FW, idx, 1); }

static uint8_t step_pct(uint8_t v, bool up) { if (v > 100) v = 100; return up ? (uint8_t)(v + 10 > 100 ? 100 : v + 10) : (uint8_t)(v < 10 ? 0 : v - 10); }
static uint8_t step_speed(uint8_t raw, bool faster) { if (raw > 4) raw = 4; return faster ? (uint8_t)(raw ? raw - 1 : 0) : (uint8_t)(raw < 4 ? raw + 1 : 4); }

/* rotate the hue of an RGB colour (custom colour mode); a grey or dark colour becomes saturated and bright so the key visibly does something */
static void hue_step(uint8_t rgb[3], int deg) {
    const int r = rgb[0], g = rgb[1], b = rgb[2];
    const int mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b), d = mx - mn;
    int h = 0;
    if (d) { if (mx == r) h = 960 * (g - b) / d; else if (mx == g) h = 1920 + 960 * (b - r) / d; else h = 3840 + 960 * (r - g) / d; }   /* 1/16 degree units */
    int s = mx ? d * 255 / mx : 0, v = mx;
    if (s < 50) s = 255;
    if (v < 50) v = 255;
    h = ((h + deg * 16) % 5760 + 5760) % 5760;
    const int sector = h / 960, f = (h % 960) * 255 / 960;
    const int p = v * (255 - s) / 255, q = v * (255 - s * f / 255) / 255, t = v * (255 - s * (255 - f) / 255) / 255;
    int R, G, B;
    switch (sector) { case 0: R = v; G = t; B = p; break; case 1: R = q; G = v; B = p; break; case 2: R = p; G = v; B = t; break;
                      case 3: R = p; G = q; B = v; break; case 4: R = t; G = p; B = v; break; default: R = v; G = p; B = q; }
    rgb[0] = (uint8_t)R; rgb[1] = (uint8_t)G; rgb[2] = (uint8_t)B;
}

bool fn_apply(nph_t *n, uint8_t profile, uint16_t e) {
    uint8_t *L = n->light[profile & 1];
    switch (e) {
        /* ---- back light ---- */
        case 0x7827: case 0x7828: L[BACK + 1] = step_pct(L[BACK + 1], e == 0x7827); touch(n, profile, BACK + 1, 1); return true;
        case 0x7821: L[BACK] = (uint8_t)(L[BACK] % 20 + 1); touch(n, profile, BACK, 1); return true;                    /* effects 1..20 */
        case 0x7829: case 0x782A: L[BACK + 2] = step_speed(L[BACK + 2], e == 0x7829); touch(n, profile, BACK + 2, 1); return true;
        case 0x7823: L[BACK + 4] = 0; hue_step(&L[BACK + 6], 15); touch(n, profile, BACK + 4, 5); return true;           /* switches to the custom colour */
        /* ---- side light ---- */
        case 0x7E0D: case 0x7E0E: L[SIDE + 1] = step_pct(L[SIDE + 1], e == 0x7E0D); touch(n, profile, SIDE + 1, 1); return true;
        case 0x7E0F: L[SIDE] = (uint8_t)((L[SIDE] + 1) % 5); touch(n, profile, SIDE, 1); return true;                      /* side effects 0..4 */
        case 0x7E10: L[SIDE + 3] = 0; hue_step(&L[SIDE + 5], 15); touch(n, profile, SIDE + 3, 5); return true;
        case 0x7E11: case 0x7E12: L[SIDE + 2] = step_speed(L[SIDE + 2], e == 0x7E11); touch(n, profile, SIDE + 2, 1); return true;
        case 0x7E3C:                                                                                                       /* side light on/off, remembering the level */
            if (L[SIDE + 1]) { n->fwcfg[NPH_FW_SIDE_PREV] = L[SIDE + 1]; L[SIDE + 1] = 0; }
            else L[SIDE + 1] = n->fwcfg[NPH_FW_SIDE_PREV] ? n->fwcfg[NPH_FW_SIDE_PREV] : 50;
            touch(n, profile, SIDE + 1, 1); touch_fw(n, NPH_FW_SIDE_PREV); return true;
        /* ---- power / indicators ---- */
        case 0x7E14: n->sleepcfg[0] = n->sleepcfg[0] ? 0 : 1; if (n->hooks.mark) n->hooks.mark(NPH_R_SLEEP, 0, 1); return true;
        case 0x7E15: n->fwcfg[NPH_FW_BAT_STRIP] ^= 1; touch_fw(n, NPH_FW_BAT_STRIP); return true;
        case 0x7E3B: n->fwcfg[NPH_FW_CAPS_OFF] ^= 1; touch_fw(n, NPH_FW_CAPS_OFF); return true;
        default: return false;
    }
}
