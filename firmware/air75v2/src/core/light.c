#include "light.h"
#include <string.h>
#include "board_data.h"
#include "effect_data.h"

/* ---------------------------------------------------------------------------------------------------------
 * Model (see tools/gen_effects.py): phase_i(t) = P_e[i] + rate_e * speed(gear) * t, in 32-bit turns.
 *   auto colour   : HSV(phase, S = 1, V = 2 * brightness%)                 (the V3 tops out at 200)
 *   custom colour : hue and saturation of the stored RGB, V = F_e(phase + d_e) * brightness%
 * ------------------------------------------------------------------------------------------------------ */

static uint8_t v3idx_of_slot[98];            /* V3 slot -> index among the V3's 83 physical keys (255 = none) */
static uint8_t led_v3[BD_KEY_LEDS];          /* this board's LED chain index -> V3 table index */
static bool    ready;

static bool v3_has(int s) {
    return s <= 14 || (s >= 16 && s <= 29) || s == 31 || (s >= 32 && s <= 45) || s == 47 || (s >= 48 && s <= 59) || s == 61 || s == 63 ||
           s == 64 || (s >= 66 && s <= 76) || s == 78 || s == 79 || s == 80 || s == 81 || s == 82 || s == 86 || (s >= 90 && s <= 95);
}

static uint32_t rnd(light_t *l) { l->seed = l->seed * 1664525u + 1013904223u; return l->seed >> 8; }

void light_init(light_t *l, nph_t *n) {
    memset(l, 0, sizeof *l); l->nph = n; l->seed = 0x2545F491u;
    if (!ready) {
        int idx = 0;
        memset(v3idx_of_slot, 255, sizeof v3idx_of_slot);
        for (int s = 0; s < 98; s++) if (v3_has(s)) v3idx_of_slot[s] = (uint8_t)idx++;
        for (int i = 0; i < BD_KEY_LEDS; i++) {
            const uint8_t s = BD_LED_V3[i];
            if (s != 0xFF) { led_v3[i] = v3idx_of_slot[s] != 255 ? v3idx_of_slot[s] : v3idx_of_slot[14]; continue; }    /* the V2's extra Del key borrows Ins */
            int best = 0x7FFFFFFF;                                  /* keys the V3 doesn't have (a numpad) borrow the effect phase of the nearest V3 key */
            for (int j = 0; j < BD_KEY_LEDS; j++) {
                if (BD_LED_V3[j] == 0xFF || v3idx_of_slot[BD_LED_V3[j]] == 255) continue;
                const int dx = BD_LED_X[i] - BD_LED_X[j], dy = BD_LED_Y[i] - BD_LED_Y[j], d = dx * dx + dy * dy;
                if (d < best) { best = d; led_v3[i] = v3idx_of_slot[BD_LED_V3[j]]; }
            }
        }
        ready = true;
    }
}

/* ---- colour helpers -------------------------------------------------------------------------------- */

static rgb_t hsv16(uint16_t h, uint8_t s, uint8_t v) {                 /* h: full turn = 65536 */
    const uint32_t h6 = (uint32_t)h * 6u;
    const uint8_t region = (uint8_t)(h6 >> 16), rem = (uint8_t)(h6 >> 8);
    const uint8_t p = (uint8_t)((v * (255 - s)) / 255), q = (uint8_t)((v * (255 - (s * rem) / 255)) / 255), t = (uint8_t)((v * (255 - (s * (255 - rem)) / 255)) / 255);
    switch (region) { case 0: return (rgb_t){v, t, p}; case 1: return (rgb_t){q, v, p}; case 2: return (rgb_t){p, v, t};
                      case 3: return (rgb_t){p, q, v}; case 4: return (rgb_t){t, p, v}; default: return (rgb_t){v, p, q}; }
}

/* custom colour: keep hue + saturation, drop its own brightness; v = 0..255 */
static rgb_t tint(uint8_t r, uint8_t g, uint8_t b, uint8_t v) {
    uint8_t m = r > g ? (r > b ? r : b) : (g > b ? g : b);
    if (!m) return (rgb_t){v, v, v};
    return (rgb_t){(uint8_t)(r * v / m), (uint8_t)(g * v / m), (uint8_t)(b * v / m)};
}

static uint8_t table_lerp(const uint8_t *t, int n, uint32_t phase32) {    /* periodic table, linear interpolation */
    const uint32_t pos = (uint32_t)(((uint64_t)phase32 * (uint32_t)n) >> 16);            /* 16.16 */
    const uint32_t i = (pos >> 16) % (uint32_t)n, j = (i + 1) % (uint32_t)n, f = pos & 0xFFFF;
    return (uint8_t)((t[i] * (65536 - f) + t[j] * f) >> 16);
}

/* The brightness setting (0..100 %) is spread over 0..BRIGHT_MAX_PM per mille of the old full output: the LEDs top out at 43 % of what they used to,
 * and every 1 % step of the setting is a finer physical step because the scaling happens before the colour is rounded to 8 bits. */
#define BRIGHT_MAX_PM 430
static uint16_t bright_pm(uint8_t pct) { return (uint16_t)((pct > 100 ? 100 : pct) * BRIGHT_MAX_PM / 100); }
static uint8_t scale_pct(uint32_t v, uint16_t pm) { return (uint8_t)(v * pm / 1000); }

/* turns*2^32 advanced after t_ms at `deg_per_s` */
static uint32_t drift32(float deg_per_s, uint32_t t_ms) {
    const double turns = (double)deg_per_s / 360.0 * (double)t_ms / 1000.0;
    return (uint32_t)(int64_t)(turns * 4294967296.0 > 0 ? (int64_t)(turns * 4294967296.0) : -(int64_t)(-turns * 4294967296.0));
}

typedef struct { const uint8_t *P, *F; float rate; uint8_t d; } hue_fx_t;
static bool hue_fx(uint8_t e, hue_fx_t *o) {
    switch (e) {
        case 1:  *o = (hue_fx_t){FX_P_1,  FX_F_1,  FX_RATE_1,  FX_D_1};  return true;
        case 2:  *o = (hue_fx_t){FX_P_2,  FX_F_2,  0.0f,       FX_D_2};  return true;
        case 6:  *o = (hue_fx_t){FX_P_6,  FX_F_6,  FX_RATE_6,  FX_D_6};  return true;
        case 7:  *o = (hue_fx_t){FX_P_7,  FX_F_7,  FX_RATE_7,  FX_D_7};  return true;
        case 8:  *o = (hue_fx_t){FX_P_8,  FX_F_8,  FX_RATE_8,  FX_D_8};  return true;
        case 9:  *o = (hue_fx_t){FX_P_9,  FX_F_9,  FX_RATE_9,  FX_D_9};  return true;
        case 10: *o = (hue_fx_t){FX_P_10, FX_F_10, FX_RATE_10, FX_D_10}; return true;
        case 14: *o = (hue_fx_t){FX_P_14, FX_F_14, FX_RATE_14, FX_D_14}; return true;
        default: return false;
    }
}

static bool in_list(const uint8_t *list, uint8_t n, uint8_t v) { for (uint8_t i = 0; i < n; i++) if (list[i] == v) return true; return false; }

/* ---- key events (reactive effects) ----------------------------------------------------------------- */

void light_key_event(light_t *l, uint8_t slot, bool pressed, uint32_t now) {
    if (slot >= NPH_SLOTS) return;
    int led = -1;
    for (int i = 0; i < BD_KEY_LEDS; i++) if (BD_LED_SLOT[i] == slot) { led = i; break; }
    if (led < 0) return;
    l->held[led] = pressed;
    if (!pressed) return;
    l->heat[led] = 255;
    if (l->press_n < LT_MAX_PRESS) l->press_n++;
    for (int k = LT_MAX_PRESS - 1; k > 0; k--) l->press[k] = l->press[k - 1];
    l->press[0].led = (uint8_t)led; l->press[0].t = now;
}

/* per-key custom-colour model (see tools/gen_effects.py): brightness = G(rate * t + phase_i) for Ray / Wave / Ripple / Spout */
typedef struct { const uint8_t *ph, *curve; float rate; } perkey_t;
static bool perkey_fx(uint8_t e, perkey_t *o) {
    switch (e) {
        case 1: *o = (perkey_t){FX_PH_1, FX_FC_1, FX_PT_1}; return true;
        case 6: *o = (perkey_t){FX_PH_6, FX_FC_6, FX_PT_6}; return true;
        case 7: *o = (perkey_t){FX_PH_7, FX_FC_7, FX_PT_7}; return true;
        case 8: *o = (perkey_t){FX_PH_8, FX_FC_8, FX_PT_8}; return true;
        default: return false;
    }
}

/* ---- back light ------------------------------------------------------------------------------------ */

static void render_back(light_t *l, uint32_t now, bool caps_on, uint32_t dt) {
    const uint8_t *st = l->nph->light[l->mode & 1];
    const uint8_t e = st[0];
    const uint16_t bright = bright_pm(st[1]);
    const uint8_t raw = st[2] > 4 ? 4 : st[2], gear = (uint8_t)(4 - raw);
    const bool custom = st[4] == 0;
    const uint8_t cr = st[6], cg = st[7], cb = st[8];
    const float ratio = FX_GEAR_RATIO[gear];
    const float dir = st[3] ? -1.0f : 1.0f;                    /* nuphy.io's Direction control: b3 = 1 runs the sweep the other way */
    hue_fx_t hf;

    if (bright == 0) { memset(l->key, 0, sizeof l->key); return; }

    if (hue_fx(e, &hf)) {
        const uint32_t drift = drift32(hf.rate * ratio * dir, now);
        for (int i = 0; i < BD_KEY_LEDS; i++) {
            const uint8_t k = led_v3[i];
            const uint32_t phase = ((uint32_t)hf.P[k] << 24) + drift;
            rgb_t c;
            if (e == 14 && !custom) c = hsv16((uint16_t)(phase >> 16), 255, scale_pct(table_lerp(FX_FA_14, FX_BINS, phase), bright));
            else if (e == 9 || !custom) c = hsv16((uint16_t)(phase >> 16), 255, scale_pct(200, bright));
            else {
                perkey_t pk; uint8_t f;
                if (perkey_fx(e, &pk)) f = table_lerp(pk.curve, 40, drift32(pk.rate * ratio * dir, now) + ((uint32_t)pk.ph[k] << 24));
                else f = table_lerp(hf.F, FX_BINS, phase + ((uint32_t)hf.d << 24));
                c = tint(cr, cg, cb, scale_pct(f, bright));
            }
            l->key[i] = c;
        }
    } else {
        for (int i = 0; i < BD_KEY_LEDS; i++) {
            const uint8_t k = led_v3[i];
            rgb_t c = {0, 0, 0};
            const uint8_t V = scale_pct(200, bright);
            switch (e) {
                case 3: c = custom ? tint(cr, cg, cb, V) : (rgb_t){V, V, V}; break;                       /* Static */
                case 4: {                                                                                  /* Breath */
                    const uint32_t per = (uint32_t)((float)FX_BREATH_PERIOD_MS / (ratio < 0.02f ? 0.02f : ratio));
                    const uint32_t ph = per ? (uint32_t)(((uint64_t)(now % per) << 32) / per) : 0;
                    const uint8_t f = table_lerp(FX_BREATH, 64, ph), v = scale_pct(f, bright);
                    c = custom ? tint(cr, cg, cb, v) : hsv16((uint16_t)(drift32(FX_BREATH_DRIFT * ratio, now) >> 16), 255, v);
                    break; }
                case 17: case 18: {                                                                       /* Gaming / Identify: fixed highlights in the stored colour */
                    const bool lit = e == 17 ? in_list(FX_GAMING_LEDS, FX_GAMING_N, k) : in_list(FX_IDENTIFY_LEDS, FX_IDENTIFY_N, k);
                    if (lit) c = (cr | cg | cb) ? tint(cr, cg, cb, V) : (rgb_t){V, 0, 0};
                    break; }
                case 12: case 13: case 11: case 15: case 5: case 16: case 19: case 20: break;            /* handled below */
                default: c = hsv16((uint16_t)(drift32(FX_RATE_1 * ratio, now) >> 16), 255, V); break;
            }
            l->key[i] = c;
        }
        /* effects that depend on key presses or randomness */
        const uint8_t V = scale_pct(200, bright);
        const uint32_t gdrift = drift32(FX_RATE_1 * ratio * 0.25f, now);     /* Point / Grid hue clock: a quarter of the base rate (measured 43.9 deg/s) */
        const uint8_t decay = (uint8_t)(dt * (95 + gear * 20) / 1000 + 1);
        const uint32_t d1920 = drift32((e == 20 ? FX_RATE_14 : FX_RATE_10) * ratio * 0.6f * dir, now);
        for (int i = 0; i < BD_KEY_LEDS; i++) {
            rgb_t c = {0, 0, 0};
            const uint8_t k = led_v3[i];
            const int x = BD_LED_X[i] / 10, y = BD_LED_Y[i] / 10;
            if (e == 12) {                                                                                   /* Point */
                if (l->held[i]) l->heat[i] = 255;
                c = custom ? tint(cr, cg, cb, scale_pct(190u * l->heat[i] / 255u, bright)) : hsv16((uint16_t)(gdrift >> 16), 255, scale_pct(190u * l->heat[i] / 255u, bright));
            } else if (e == 11) {                                                                            /* Ripple 2: expanding ring from each press */
                uint8_t best = 0;
                for (uint8_t p = 0; p < l->press_n; p++) {
                    const uint32_t age = now - l->press[p].t; if (age > 1400) continue;
                    const int dx = x - BD_LED_X[l->press[p].led] / 10, dy = y - BD_LED_Y[l->press[p].led] / 10;
                    const uint32_t d = (uint32_t)(dx < 0 ? -dx : dx) + (uint32_t)(dy < 0 ? -dy : dy);
                    const int32_t ring = (int32_t)(age * 9 / 1000) - (int32_t)d;                          /* 9 keys per second */
                    if (ring >= 0 && ring < 3) { const uint8_t v = (uint8_t)((255 - age * 255 / 1400) / (ring + 1)); if (v > best) best = v; }
                }
                c = custom ? tint(cr, cg, cb, scale_pct(200u * best / 255u, bright)) : hsv16((uint16_t)(gdrift >> 16) + (uint16_t)(k << 9), 255, scale_pct(200u * best / 255u, bright));
            } else if (e == 13) {                                                                            /* Grid: the keys around a press flash */
                uint8_t best = 0;
                for (uint8_t p = 0; p < l->press_n; p++) {
                    const uint32_t age = now - l->press[p].t; if (age > 700) continue;
                    const int dx = x - BD_LED_X[l->press[p].led] / 10, dy = y - BD_LED_Y[l->press[p].led] / 10;
                    if (dx * dx + dy * dy <= 5) { const uint8_t v = (uint8_t)(255 - age * 255 / 700); if (v > best) best = v; }
                }
                c = custom ? tint(cr, cg, cb, scale_pct(200u * best / 255u, bright)) : hsv16((uint16_t)(gdrift >> 16), 255, scale_pct(200u * best / 255u, bright));
            } else if (e == 15) {                                                                            /* Rain: random drops falling down the columns */
                const uint32_t slot = now / 120, col = (uint32_t)x;
                const uint32_t drop_row = (slot + col * 7) % 24;                                           /* a drop per column every 24 steps, staggered */
                const bool active = ((col * 2654435761u + (slot + col * 7) / 24 * 40503u) >> 12) % 5 == 0;
                const int row = (int)drop_row;
                if (active && (row == y || row == y + 1)) c = custom ? tint(cr, cg, cb, scale_pct(row == y ? 200u : 90u, bright)) : hsv16((uint16_t)((col * 5000u) + (gdrift >> 16)), 255, scale_pct(row == y ? 200u : 90u, bright));
                if (l->heat[i]) c = custom ? tint(cr, cg, cb, scale_pct(200u * l->heat[i] / 255u, bright)) : hsv16((uint16_t)(gdrift >> 16), 255, scale_pct(200u * l->heat[i] / 255u, bright));
            } else if (e == 5) {                                                                             /* Flower: random keys fade in and out in a few colours */
                if ((int32_t)(now - l->flower_t[i]) >= 0) {
                    l->flower_v[i] = (rnd(l) & 3) ? 200 : 0; l->flower_h[i] = (uint8_t)(rnd(l) & 3);
                    l->flower_t[i] = now + 600 + (rnd(l) % 2400);
                }
                c = custom ? tint(cr, cg, cb, scale_pct(l->flower_v[i], bright)) : hsv16((uint16_t)(l->flower_h[i] * 16384u), 255, scale_pct(l->flower_v[i], bright));
            } else if (e == 16) {                                                                            /* Ribbon: scrolling pseudo-random bars */
                const uint32_t step = now / 220 + (uint32_t)x;
                const bool on = ((step * 2654435761u + (uint32_t)y * 40503u) >> 13) & 1;
                if (on) c = hsv16((uint16_t)(((step * 2654435761u) >> 8) & 0xFFFF), 255, V);
            } else if (e == 19) {                                                                            /* Windmill: three blades of the stored colour sweeping over a white background */
                const uint32_t ph = drift32(FX_RATE_19 * ratio * dir, now) - ((uint32_t)FX_P_19[k] << 24);
                const uint8_t m = table_lerp(FX_F_19, 40, ph);
                const rgb_t blade = tint(cr, cg, cb, V);
                c.r = (uint8_t)(V + ((int)blade.r - V) * m / 255); c.g = (uint8_t)(V + ((int)blade.g - V) * m / 255); c.b = (uint8_t)(V + ((int)blade.b - V) * m / 255);
            } else if (e == 20) {                                                                 /* Windmill / Diagonal: saturation comes from the stored RGB */
                const uint8_t mx = cr > cg ? (cr > cb ? cr : cb) : (cg > cb ? cg : cb), mn = cr < cg ? (cr < cb ? cr : cb) : (cg < cb ? cg : cb);
                const uint8_t sat = mx ? (uint8_t)(255u * (mx - mn) / mx) : 0;
                const uint32_t ph = ((uint32_t)(e == 20 ? FX_P_14[k] : FX_P_10[k]) << 24) + d1920;
                c = hsv16((uint16_t)(ph >> 16), sat, V);
            } else continue;
            l->key[i] = c;
            if (l->heat[i] && !l->held[i]) l->heat[i] = l->heat[i] > decay ? (uint8_t)(l->heat[i] - decay) : 0;
        }
    }
    if (caps_on) for (int i = 0; i < BD_KEY_LEDS; i++) if (BD_LED_V3[i] == 48) { const uint8_t w0 = scale_pct(255, bright), w = w0 < 12 ? 12 : w0; l->key[i] = (rgb_t){w, w, w}; }   /* caps-lock indicator: white at the current brightness (at least dimly visible) */
}

/* ---- side light ------------------------------------------------------------------------------------ */

static void render_side(light_t *l, uint32_t now) {
    const uint8_t *sd = l->nph->light[l->mode & 1] + 9;
    const uint8_t e = sd[0];
    const uint16_t b = bright_pm(sd[1]);
    const bool custom = sd[3] == 0;
    const uint8_t cr = sd[5], cg = sd[6], cb = sd[7];
    if (b == 0) { memset(l->side, 0, sizeof l->side); return; }
    const uint8_t V = (uint8_t)(255u * b / 1000u);
    for (int j = 0; j < BD_SIDE_LEDS; j++) {
        const int k = (j % BD_SIDE_STRIP_LEN) * 9 / (BD_SIDE_STRIP_LEN - 1);   /* map this board's LEDs along a strip (or the halo ring) onto the V3's 10 */
        rgb_t c;
        switch (e) {
            case 0: {                                                    /* Time: a hue gradient along the strip, drifting */
                const uint32_t ph = ((uint32_t)FX_SIDE_TIME_P[k] << 24) + drift32(FX_SIDE_TIME_DRIFT, now);
                c = custom ? tint(cr, cg, cb, (uint8_t)(ph >> 24) * V / 255) : hsv16((uint16_t)(ph >> 16), 247, V);
                break; }
            case 1: {                                                    /* Neon: the whole strip cycles */
                const uint32_t ph = drift32(FX_SIDE_NEON_DRIFT, now);
                c = custom ? tint(cr, cg, cb, (uint8_t)(ph >> 24) * V / 255) : hsv16((uint16_t)(ph >> 16), 247, V);
                break; }
            case 2:                                                      /* Static */
                if (custom) c = tint(cr, cg, cb, V);
                else c = (rgb_t){(uint8_t)(FX_SIDE_STATIC_RGB[k * 3] * (uint32_t)b / 1000), (uint8_t)(FX_SIDE_STATIC_RGB[k * 3 + 1] * (uint32_t)b / 1000), (uint8_t)(FX_SIDE_STATIC_RGB[k * 3 + 2] * (uint32_t)b / 1000)};
                break;
            case 3: {                                                    /* Breathe (speed has no effect, like the V3) */
                const uint32_t ph = (uint32_t)(((uint64_t)(now % FX_SIDE_BREATH_PERIOD_MS) << 32) / FX_SIDE_BREATH_PERIOD_MS);
                const uint8_t v = (uint8_t)(table_lerp(FX_SIDE_BREATH, 64, ph) * (uint32_t)b / 1000);
                c = custom ? tint(cr, cg, cb, v) : (rgb_t){v, 0, 0};
                break; }
            default: {                                                   /* Rhythm: there is no audio input on this board */
                const uint8_t v = (uint8_t)(V / 10);
                c = custom ? tint(cr, cg, cb, v) : (rgb_t){v, v, v};
            }
        }
        l->side[j] = c;
    }
}

/* Battery bar on the left strip, tiers and colours as in the stock firmware: <=20 % one red LED ... >80 % five green LEDs.
 * Charging: the next LED breathes. Under 10 % and not charging: the first LED blinks red. */
static void battery_bar(light_t *l, uint32_t now) {
    const uint8_t p = l->bat_pct > 100 ? 100 : l->bat_pct;
    uint8_t n; rgb_t c;
    rgb_t *bar = &l->side[BD_BAT_FIRST];
    if (p <= 20) { n = 1; c = (rgb_t){0x60, 0, 0}; } else if (p <= 40) { n = 2; c = (rgb_t){0x60, 0x30, 0}; } else if (p <= 60) { n = 3; c = (rgb_t){0x60, 0x60, 0}; }
    else if (p <= 80) { n = 4; c = (rgb_t){0x18, 0, 0x60}; } else { n = 5; c = (rgb_t){0, 0x60, 0}; }
    for (int i = 0; i < BD_BAT_COUNT; i++) bar[i] = (rgb_t){0, 0, 0};
    if (!l->bat_charging && p < 10) { if ((now / 500) & 1) bar[0] = (rgb_t){0x60, 0, 0}; return; }
    for (int i = 0; i < n; i++) bar[i] = c;
    if (l->bat_charging) {
        const uint32_t ph = now % 1600; const uint32_t tri = ph < 800 ? ph : 1600 - ph;                  /* 0..800..0 */
        if (n < BD_BAT_COUNT) bar[n] = (rgb_t){(uint8_t)(c.r * tri / 800), (uint8_t)(c.g * tri / 800), (uint8_t)(c.b * tri / 800)};
        else { const uint32_t f = 200 + tri * 3 / 4; for (int i = 0; i < n; i++) bar[i] = (rgb_t){(uint8_t)(c.r * f / 800), (uint8_t)(c.g * f / 800), (uint8_t)(c.b * f / 800)}; }   /* a full bar has no spare LED: it breathes */
    }
}

static void overlays(light_t *l, uint32_t now) {
    if (l->test) {
        static const rgb_t col[5] = {{0, 0, 0}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}};
        const rgb_t c = col[l->test > 4 ? 4 : l->test];
        for (int i = 0; i < BD_KEY_LEDS; i++) l->key[i] = c;
        for (int i = 0; i < BD_SIDE_LEDS; i++) l->side[i] = c;
        return;
    }
    if (l->bat_on) battery_bar(l, now);
}

void light_render(light_t *l, uint32_t now, bool caps_on) {
    const uint32_t dt = now - l->last_ms; l->last_ms = now;
    render_back(l, now, caps_on, dt);
    render_side(l, now);
    overlays(l, now);
}

/* what the live LED view on nuphy.io / Air Deck shows: the LEDs at their new maximum (43 % of the old output) read as 100 % */
static uint8_t full_scale(uint8_t c) { const uint32_t v = (uint32_t)c * 1000u / BRIGHT_MAX_PM; return (uint8_t)(v > 255 ? 255 : v); }

/* The V3's 83 physical keys in ascending slot order, then the top-right accessory LED (the V2's button, slot 60 - where the V3 has its knob), then 2 x 10 side LEDs. */
void light_v3_snapshot(const light_t *l, uint8_t out[NPH_LED_COUNT_V3 * 3]) {
    memset(out, 0, NPH_LED_COUNT_V3 * 3);
    for (int i = 0; i < BD_KEY_LEDS; i++) {
        const uint8_t k = BD_LED_SNAP[i]; if (k == 0xFF) continue;
        out[k * 3] = full_scale(l->key[i].r); out[k * 3 + 1] = full_scale(l->key[i].g); out[k * 3 + 2] = full_scale(l->key[i].b);
    }
    for (int st = 0; st < 2; st++) for (int k = 0; k < 10; k++) {
        const int src = BD_SIDE_STRIPS >= 2 ? st * BD_SIDE_STRIP_LEN + (k * (BD_SIDE_STRIP_LEN - 1) + 4) / 9 : ((st * 10 + k) * BD_SIDE_LEDS) / 20;
        const rgb_t c = l->side[src]; uint8_t *o = &out[(BD_V3_KEY_LEDS + st * 10 + k) * 3]; o[0] = full_scale(c.r); o[1] = full_scale(c.g); o[2] = full_scale(c.b);
    }
}
