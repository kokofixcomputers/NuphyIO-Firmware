#include "keyeng.h"
#include <string.h>

#define E_NONE   0x0000
#define E_TRANS  0x0001

static uint16_t entry_at(const keyeng_t *k, uint8_t layer, uint8_t slot) {
    const uint8_t *p = &k->nph->keymap[(layer * NPH_SLOTS + slot) * 2];
    return (uint16_t)((p[0] << 8) | p[1]);
}

/* highest active layer first, falling through transparent entries down to the base layer */
static uint16_t resolve(const keyeng_t *k, uint8_t slot) {
    const uint8_t base = k->mode ? 4 : 0;
    for (int l = NPH_LAYERS - 1; l > base; l--) {
        if (!k->layer_cnt[l]) continue;
        uint16_t e = entry_at(k, (uint8_t)l, slot);
        if (e != E_TRANS) return e;
    }
    uint16_t e = entry_at(k, base, slot);
    return e == E_TRANS ? E_NONE : e;
}

uint8_t keyeng_layer(const keyeng_t *k) {
    const uint8_t base = k->mode ? 4 : 0;
    for (int l = NPH_LAYERS - 1; l > base; l--) if (k->layer_cnt[l]) return (uint8_t)l;
    return base;
}

uint16_t keyeng_usage_to_consumer(uint8_t u) {
    switch (u) {
        case 0xA8: return 0x00E2; case 0xA9: return 0x00E9; case 0xAA: return 0x00EA;   /* mute, vol+, vol- */
        case 0xAB: return 0x00B5; case 0xAC: return 0x00B6; case 0xAD: return 0x00B7;   /* next, prev, stop */
        case 0xAE: return 0x00CD; case 0xAF: return 0x0183; case 0xB0: return 0x00B8;   /* play/pause, select, eject */
        case 0xB1: return 0x018A; case 0xB2: return 0x0192; case 0xB3: return 0x0194;   /* mail, calc, my computer */
        case 0xB4: return 0x0221; case 0xB5: return 0x0223; case 0xB6: return 0x0224;   /* www search/home/back */
        case 0xB7: return 0x0225; case 0xB8: return 0x0226; case 0xB9: return 0x0227;   /* fwd/stop/refresh */
        case 0xBA: return 0x022A; case 0xBB: return 0x00B3; case 0xBC: return 0x00B4;   /* favourites, ffwd, rewind */
        case 0xBD: return 0x006F; case 0xBE: return 0x0070;                               /* brightness +/- */
        default: return 0;
    }
}

/* ---- NuPhy shortcut keys (0x7Exx): key combos, media and system usages the stock firmware sends ------------------------ */

typedef struct { uint16_t entry; uint8_t u[3]; uint16_t cons; uint8_t sys; } sc_t;
#define GUI 0xE3   /* Cmd on a Mac, Win on Windows */
#define CTL 0xE0
#define SFT 0xE1
#define ALT 0xE2
static const sc_t SHORTCUTS[] = {
    {0x7E06, {0}, 0x029F, 0},                    /* Mission Control (MAC_TASK) */
    {0x7E07, {GUI, 0x2C}, 0, 0},                 /* Spotlight: Cmd+Space (MAC_SEARCH) */
    {0x7E08, {0}, 0x00CF, 0},                    /* Dictation / voice control (MAC_VOICE) */
    {0x7E09, {0}, 0x02A0, 0},                    /* Launchpad / show apps (MAC_CONSOLE) */
    {0x7E0A, {GUI, SFT, 0x20}, 0, 0},            /* screenshot: Cmd+Shift+3 */
    {0x7E0B, {GUI, SFT, 0x21}, 0, 0},            /* screenshot area: Cmd+Shift+4 */
    {0x7E16, {0}, 0, 0x9B},                      /* Do Not Disturb (MAC_DND) */
    {0x7E3E, {GUI, SFT, 0x22}, 0, 0},            /* Mac screenshot / record: Cmd+Shift+5 */
    {0x7E3F, {GUI, 0x0F}, 0, 0},                 /* Windows lock: Win+L */
    {0x7E40, {CTL, GUI, 0x14}, 0, 0},            /* Mac lock screen: Ctrl+Cmd+Q */
    {0x7E41, {GUI, ALT, 0x2C}, 0, 0},            /* Finder search: Cmd+Opt+Space */
    {0x7E42, {GUI, ALT, 0x29}, 0, 0},            /* Force quit: Cmd+Opt+Esc */
    {0x7E43, {CTL, GUI, 0x2C}, 0, 0},            /* Emoji picker: Ctrl+Cmd+Space */
    {0x7E44, {0x44}, 0, 0},                      /* Mac back to desktop: F11 */
    {0x7E45, {CTL, SFT, 0x29}, 0, 0},            /* Windows task manager: Ctrl+Shift+Esc */
    {0x7E47, {GUI, CTL, 0x50}, 0, 0},            /* Windows desktop left */
    {0x7E48, {GUI, CTL, 0x4F}, 0, 0},            /* Windows desktop right */
    {0x7E49, {CTL, 0x50}, 0, 0},                 /* Mac desktop left: Ctrl+Left */
    {0x7E4A, {CTL, 0x4F}, 0, 0},                 /* Mac desktop right: Ctrl+Right */
    {0x7E4F, {GUI, 0x04}, 0, 0},                 /* Windows control center: Win+A */
    {0x7E52, {GUI, 0x07}, 0, 0},                 /* Windows show desktop: Win+D */
    {0x7E53, {CTL, 0x2C}, 0, 0},                 /* Mac language switch: Ctrl+Space */
    {0x7E54, {GUI, 0x0B}, 0, 0},                 /* Windows dictation: Win+H */
    {0x7E55, {0x31}, 0, 0},                      /* Flexo: backslash (the long-press Shift is not implemented) */
};

static const sc_t *shortcut_find(uint16_t e) {
    for (unsigned i = 0; i < sizeof SHORTCUTS / sizeof SHORTCUTS[0]; i++) if (SHORTCUTS[i].entry == e) return &SHORTCUTS[i];
    return 0;
}

/* ---- usage bookkeeping ------------------------------------------------------------------------------ */

/* nuphy.io "Mode Settings" (kbfunc bytes: [stabilization, disable Win, disable Alt+F4, disable Alt+Tab]): keys the board refuses to send */
static bool blocked(const keyeng_t *k, uint8_t u) {
    const uint8_t *f = k->nph->kbfunc;
    const bool alt = k->ucnt[0xE2] || k->ucnt[0xE6];
    if (f[1] && k->mode && (u == 0xE3 || u == 0xE7)) return true;      /* Win keys (on the Windows layers; Cmd on a Mac is left alone) */
    if (f[2] && alt && u == 0x3D) return true;                          /* F4 while Alt is down */
    if (f[3] && alt && u == 0x2B) return true;                          /* Tab while Alt is down */
    return false;
}

static void usage_down(keyeng_t *k, uint8_t u) {
    if (blocked(k, u)) return;
    if (u >= 0xD1 && u <= 0xDC) {                         /* mouse buttons 1-5 and scroll (QMK numbering) */
        if (u >= 0xD6 && u <= 0xD8) return;
        k->ucnt[u]++; k->dirty = true; return;
    }
    if (u >= 0xA5 && u <= 0xBE) {
        if (u <= 0xA7) k->sys_usage = (uint8_t)(0x81 + (u - 0xA5)); else k->cons_cnt_usage = keyeng_usage_to_consumer(u);
        k->dirty = true; return;
    }
    if (u < 4 && u != 0) return;
    if (k->ucnt[u]++ == 0 && !(u >= 0xE0 && u <= 0xE7) && k->order_n < sizeof k->order) k->order[k->order_n++] = u;
    k->dirty = true;
}

static void usage_up(keyeng_t *k, uint8_t u) {
    if (u >= 0xD1 && u <= 0xDC) { if (k->ucnt[u]) k->ucnt[u]--; k->dirty = true; return; }
    if (u >= 0xA5 && u <= 0xBE) {
        if (u <= 0xA7) k->sys_usage = 0; else k->cons_cnt_usage = 0;
        k->dirty = true; return;
    }
    if (!k->ucnt[u]) return;
    if (--k->ucnt[u] == 0 && !(u >= 0xE0 && u <= 0xE7)) {
        for (uint8_t i = 0; i < k->order_n; i++)
            if (k->order[i] == u) { memmove(&k->order[i], &k->order[i + 1], (size_t)(k->order_n - i - 1)); k->order_n--; break; }
    }
    k->dirty = true;
}


/* press: hold the combo (modifiers first, then the key) / media / system usage; release lets go in reverse */
static void shortcut(keyeng_t *k, uint16_t e, bool down) {
    const sc_t *s = shortcut_find(e);
    if (!s) return;
    static const sc_t WIN_AREA = {0x7E0B, {GUI, SFT, 0x16}, 0, 0}, WIN_SHOT = {0x7E0A, {0x46}, 0, 0};      /* the screenshot keys follow the OS: Win+Shift+S / Print Screen on Windows */
    if (k->mode) { if (e == 0x7E0B) s = &WIN_AREA; else if (e == 0x7E0A) s = &WIN_SHOT; }
    if (s->cons) { k->cons_cnt_usage = down ? s->cons : 0; k->dirty = true; }
    if (s->sys) { k->sys_usage = down ? s->sys : 0; k->dirty = true; }
    if (down) { for (int i = 0; i < 3; i++) if (s->u[i]) usage_down(k, s->u[i]); }
    else { for (int i = 2; i >= 0; i--) if (s->u[i]) usage_up(k, s->u[i]); }
}

/* ---- macros ----------------------------------------------------------------------------------------- */

static void macro_release_all(keyeng_t *k) {
    for (uint8_t i = 0; i < k->mac_down_n; i++) usage_up(k, k->mac_down[i]);
    k->mac_down_n = 0;
}

static void macro_start(keyeng_t *k, uint8_t idx, uint32_t now) {
    if (idx >= 31) return;
    const uint8_t *m = k->nph->macro;
    uint16_t a = (uint16_t)(m[idx * 2] | (m[idx * 2 + 1] << 8)), b = (uint16_t)(m[idx * 2 + 2] | (m[idx * 2 + 3] << 8));
    if (a < 0x40 || b <= a || b > NPH_MACRO_BYTES) return;
    macro_release_all(k);
    k->mac = &m[a]; k->mac_len = (uint16_t)(b - a); k->mac_pos = 0; k->mac_active = true;
    k->mac_next = now + (uint32_t)(k->mac[0] | (k->mac[1] << 8));
}

static void macro_step(keyeng_t *k, uint32_t now) {
    while (k->mac_active && (int32_t)(now - k->mac_next) >= 0) {
        const uint8_t *e = &k->mac[k->mac_pos];
        const uint8_t type = e[2], key = e[3];
        if (type & 0x40) { usage_down(k, key); if (k->mac_down_n < sizeof k->mac_down) k->mac_down[k->mac_down_n++] = key; }
        else {
            usage_up(k, key);
            for (uint8_t i = 0; i < k->mac_down_n; i++) if (k->mac_down[i] == key) { k->mac_down[i] = k->mac_down[--k->mac_down_n]; break; }
        }
        k->mac_pos = (uint16_t)(k->mac_pos + 4);
        if ((type & 0x80) || k->mac_pos + 4 > k->mac_len) { k->mac_active = false; macro_release_all(k); break; }
        k->mac_next += (uint32_t)(k->mac[k->mac_pos] | (k->mac[k->mac_pos + 1] << 8));
    }
}

/* ---- advanced keys: tap dance / toggle -------------------------------------------------------------- */

enum { TD_IDLE = 0, TD_PRESSED, TD_WAIT2, TD_HOLD, TD_DOUBLE, TD_TAPPING, TGL_PRESSED, TGL_HOLD };

static void td_load(const keyeng_t *k, uint8_t idx, uint16_t kc[3], uint16_t *term) {
    const uint8_t *p = &k->nph->td[idx * 8];
    for (int i = 0; i < 3; i++) { kc[i] = (uint16_t)((p[i * 2] << 8) | p[i * 2 + 1]); if (kc[i] == 0xFFFF) kc[i] = 0; }
    *term = (uint16_t)((p[6] << 8) | p[7]);
    if (*term == 0xFFFF || *term < 30) *term = 170;
}

static uint16_t tgl_load(const keyeng_t *k, uint8_t idx) {
    const uint8_t *p = &k->nph->tgl[idx * 2];
    uint16_t v = (uint16_t)((p[0] << 8) | p[1]);
    return v == 0xFFFF ? 0 : v;
}

/* SOCD pair p: record at socd[p*8] = [key1 hi, key1 lo, -, prio1, key2 hi, key2 lo, -, prio2]; priority bits (prio1, prio2):
 * (1,1) last input wins, (0,0) neutral (neither is sent), (1,0) key 1 always wins, (0,1) key 2 always wins. */
static void socd_update(keyeng_t *k, uint8_t p) {
    const uint8_t *r = &k->nph->socd[p * 8];
    const bool h0 = k->socd_held[p][0], h1 = k->socd_held[p][1];
    bool want[2] = {h0, h1};
    if (h0 && h1) {
        const uint8_t b1 = r[3] & 1, b2 = r[7] & 1;
        if (b1 && b2) { want[0] = k->socd_last[p] == 0; want[1] = !want[0]; }
        else if (!b1 && !b2) want[0] = want[1] = false;
        else { want[0] = b1; want[1] = b2; }
    }
    for (int i = 0; i < 2; i++) {
        if (want[i] == k->socd_out[p][i]) continue;
        k->socd_out[p][i] = want[i];
        const uint8_t hi = r[i * 4], lo = r[i * 4 + 1];
        if (hi != 0 || lo == 0) continue;
        if (want[i]) usage_down(k, lo); else usage_up(k, lo);
    }
}

static void socd_event(keyeng_t *k, uint8_t idx, bool down) {
    const uint8_t p = idx >> 1, side = idx & 1;
    if (p >= 32) return;
    k->socd_held[p][side] = down;
    if (down) k->socd_last[p] = side;
    socd_update(k, p);
}

static void advanced_press(keyeng_t *k, uint8_t slot, uint32_t now) {
    ke_slot_t *s = &k->slot[slot];
    const uint8_t lo = s->entry & 0xFF;
    if (lo < 64) { socd_event(k, lo, true); return; }
    if (lo >= 64 && lo < 128) {
        if (s->state == TD_WAIT2) { s->state = TD_DOUBLE; uint16_t kc[3], term; td_load(k, lo - 64, kc, &term); if (kc[1]) usage_down(k, (uint8_t)kc[1]); return; }
        s->state = TD_PRESSED; s->t = now;
    } else if (lo >= 128 && lo < 192) {
        s->state = TGL_PRESSED; s->t = now;
    }
}

static void advanced_release(keyeng_t *k, uint8_t slot, uint32_t now) {
    ke_slot_t *s = &k->slot[slot];
    const uint8_t lo = s->entry & 0xFF;
    if (lo < 64) { socd_event(k, lo, false); return; }
    if (lo >= 64 && lo < 128) {
        uint16_t kc[3], term; td_load(k, lo - 64, kc, &term);
        if (s->state == TD_PRESSED) { s->state = TD_WAIT2; s->t_rel = now; }                 /* quick tap: wait for a second */
        else if (s->state == TD_HOLD) { if (kc[2]) usage_up(k, (uint8_t)kc[2]); s->state = TD_IDLE; }
        else if (s->state == TD_DOUBLE) { if (kc[1]) usage_up(k, (uint8_t)kc[1]); s->state = TD_IDLE; }
    } else if (lo >= 128 && lo < 192) {
        const uint8_t i = lo - 128; const uint16_t kc = tgl_load(k, i);
        if (s->state == TGL_PRESSED) {                                                          /* tapped: flip the lock */
            k->tgl_on[i] = !k->tgl_on[i];
            if (kc && (kc >> 8) == 0) { if (k->tgl_on[i]) usage_down(k, (uint8_t)kc); else usage_up(k, (uint8_t)kc); }
        } else if (s->state == TGL_HOLD) { if (kc && (kc >> 8) == 0 && !k->tgl_on[i]) usage_up(k, (uint8_t)kc); }
        s->state = TD_IDLE;
    }
}

/* ---- public ----------------------------------------------------------------------------------------- */

void keyeng_init(keyeng_t *k, nph_t *n) { memset(k, 0, sizeof *k); k->nph = n; }

void keyeng_release_all(keyeng_t *k) {
    memset(k->ucnt, 0, sizeof k->ucnt); k->order_n = 0; k->cons_cnt_usage = 0; k->sys_usage = 0;
    memset(k->layer_cnt, 0, sizeof k->layer_cnt); memset(k->slot, 0, sizeof k->slot);
    memset(k->socd_held, 0, sizeof k->socd_held); memset(k->socd_out, 0, sizeof k->socd_out);
    k->mac_active = false; k->mac_down_n = 0; k->dirty = true;
}

void keyeng_set_mode(keyeng_t *k, uint8_t mode) {
    if (mode == k->mode) return;
    k->mode = mode ? 1 : 0;
    memset(k->tgl_on, 0, sizeof k->tgl_on);
    keyeng_release_all(k);
}

void keyeng_event(keyeng_t *k, uint8_t slot, bool pressed, uint32_t now) {
    if (slot >= NPH_SLOTS) return;
    ke_slot_t *s = &k->slot[slot];
    if (pressed) {
        if (s->state == TD_WAIT2) { advanced_press(k, slot, now); return; }                    /* second tap of a tap dance */
        s->entry = resolve(k, slot); s->t = now; s->state = TD_IDLE;
        const uint16_t e = s->entry; const uint8_t hi = e >> 8, lo = e & 0xFF;
        if (e == E_NONE) return;
        if (hi == 0x00) usage_down(k, lo);
        else if (hi == 0x52) { if ((lo & 0x1F) < NPH_LAYERS) k->layer_cnt[lo & 0x1F]++; k->dirty = true; }
        else if (hi == 0x77) macro_start(k, lo, now);
        else if (hi == 0x7F) advanced_press(k, slot, now);
        else if (hi == 0x7E && shortcut_find(e)) shortcut(k, e, true);
        else if ((hi == 0x7E || hi == 0x78) && k->fn_cb) k->fn_cb(k->fn_ctx, e, true);      /* radio / lighting hotkeys are handled by the board */
    } else {
        const uint16_t e = s->entry; const uint8_t hi = e >> 8, lo = e & 0xFF;
        if (e == E_NONE) return;
        if (hi == 0x00) usage_up(k, lo);
        else if (hi == 0x52) { if ((lo & 0x1F) < NPH_LAYERS && k->layer_cnt[lo & 0x1F]) k->layer_cnt[lo & 0x1F]--; k->dirty = true; }
        else if (hi == 0x7F) advanced_release(k, slot, now);
        else if (hi == 0x7E && shortcut_find(e)) shortcut(k, e, false);
        else if ((hi == 0x7E || hi == 0x78) && k->fn_cb) k->fn_cb(k->fn_ctx, e, false);
    }
}

void keyeng_tick(keyeng_t *k, uint32_t now) {
    macro_step(k, now);
    for (uint8_t i = 0; i < NPH_SLOTS; i++) {
        ke_slot_t *s = &k->slot[i];
        const uint8_t lo = s->entry & 0xFF;
        if ((s->entry >> 8) != 0x7F || s->state == TD_IDLE) continue;
        if (lo >= 64 && lo < 128) {
            uint16_t kc[3], term; td_load(k, lo - 64, kc, &term);
            if (s->state == TD_PRESSED && (uint32_t)(now - s->t) >= term) { s->state = TD_HOLD; if (kc[2]) usage_down(k, (uint8_t)kc[2]); }
            else if (s->state == TD_WAIT2 && (uint32_t)(now - s->t_rel) >= term) { s->state = TD_TAPPING; s->t_rel = now; if (kc[0]) usage_down(k, (uint8_t)kc[0]); }
            else if (s->state == TD_TAPPING && (uint32_t)(now - s->t_rel) >= 10) { if (kc[0]) usage_up(k, (uint8_t)kc[0]); s->state = TD_IDLE; }
        } else if (lo >= 128 && lo < 192 && s->state == TGL_PRESSED && (uint32_t)(now - s->t) >= 170) {
            s->state = TGL_HOLD;                                                               /* held: behave as the normal key */
            const uint16_t kc = tgl_load(k, lo - 128);
            if (kc && (kc >> 8) == 0 && !k->tgl_on[lo - 128]) usage_down(k, (uint8_t)kc);
        }
    }
}

bool keyeng_report(keyeng_t *k, ke_report_t *out) {
    if (!k->dirty) return false;
    k->dirty = false;
    memset(out, 0, sizeof *out);
    for (int i = 0; i < 8; i++) if (k->ucnt[0xE0 + i]) out->mods |= (uint8_t)(1u << i);
    for (uint8_t i = 0; i < k->order_n && i < KE_MAX_KEYS; i++) out->keys[i] = k->order[i];
    out->consumer = k->cons_cnt_usage;
    out->system = k->sys_usage;
    for (int i = 0; i < 5; i++) if (k->ucnt[0xD1 + i]) out->buttons |= (uint8_t)(1u << i);
    out->wheel = (int8_t)((k->ucnt[0xD9] ? 1 : 0) - (k->ucnt[0xDA] ? 1 : 0));
    out->hwheel = (int8_t)((k->ucnt[0xDC] ? 1 : 0) - (k->ucnt[0xDB] ? 1 : 0));
    return true;
}
