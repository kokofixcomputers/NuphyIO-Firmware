#include "nph_proto.h"
#include <string.h>
#include "board_data.h"

uint8_t nph_checksum(const uint8_t f[NPH_FRAME]) {
    uint8_t s = 0;
    for (int i = 4; i < NPH_FRAME; i++) s += f[i];
    return s;
}

/* Factory light states, one per profile (handle 0 = Mac, 1 = Windows):
 *   back light  [effect, brightness, speedRaw, b3, auto, b5, R, G, B]
 *   side light  [effect, brightness, speedRaw, auto, b4, R, G, B]      (speedRaw = 4 - gear) */
static const uint8_t DEFAULT_LIGHT[NPH_LIGHT_BYTES] = {
    0x14, 0x64, 0x02, 0x00, 0x01, 0x00, 0xFF, 0xFF, 0xFF,   /* Diagonal, 100 % (= 43 % of the LEDs' old full output), gear 2, automatic colour */
    0x00, 0x64, 0x02, 0x01, 0x00, 0xFF, 0x00, 0x00,         /* Time, 100 %, gear 2, automatic colour */
};

void nph_load_defaults(nph_t *s) {
    memcpy(s->keymap, BD_DEFAULT_KEYMAP, NPH_KEYMAP_BYTES);
    memset(s->macro, 0, sizeof s->macro);
    for (int i = 0; i < 32; i++) { s->macro[i * 2] = 0x40; s->macro[i * 2 + 1] = 0x00; }   /* 32 empty macros start at 0x40 */
    memcpy(s->light[0], DEFAULT_LIGHT, NPH_LIGHT_BYTES);
    memcpy(s->light[1], DEFAULT_LIGHT, NPH_LIGHT_BYTES);
    memset(s->td, 0xFF, sizeof s->td);
    memset(s->tgl, 0xFF, sizeof s->tgl);
    memset(s->socd, 0xFF, sizeof s->socd);
    const uint8_t kbfunc[4] = {0x02, 0x00, 0x00, 0x00};
    const uint8_t sleepcfg[4] = {0x01, 0x06, 0x18, 0x04};
    memcpy(s->kbfunc, kbfunc, 4);
    memcpy(s->sleepcfg, sleepcfg, 4);
    memset(s->appdef, 0, sizeof s->appdef);
    s->appdef[NPH_APPDEF_ACCESSORY] = 1;                                    /* the V2 has a button, not a knob, in the top-right corner */
    memset(s->fwcfg, 0, sizeof s->fwcfg);
    s->fwcfg[NPH_FW_SIDE_PREV] = 50;
    s->fwcfg[NPH_FW_BRIGHT_REV] = 1;
    s->keyupload = false;
}

void nph_init(nph_t *s, const nph_hooks_t *hooks) {
    memset(s, 0, sizeof *s);
    if (hooks) s->hooks = *hooks;
    /* Reports firmware 1.0.14.6 (bytes: patch, minor, major, 0xAA marker, build lo, build hi) - the same
     * as the V3 this was reverse-engineered from, so the official app enables the same feature set. */
    const uint8_t fw[8] = {0x0E, 0x00, 0x01, 0xAA, 0x06, 0x00, 0x00, 0x00};
    memcpy(s->firmware, fw, 8);
    nph_load_defaults(s);
}

/* ---- region access ---------------------------------------------------------------------------------- */

typedef struct { uint8_t *data; uint16_t size; bool rw; uint8_t dirty; uint8_t idx; uint16_t base; } region_t;

static void mark(nph_t *s, uint8_t idx, uint16_t off, uint16_t len) { if (s->hooks.mark) s->hooks.mark(idx, off, len); }
static void mark_all(nph_t *s, uint8_t idx, uint16_t size) { mark(s, idx, 0, size); }

static bool region_for(nph_t *s, uint8_t cmd, uint8_t handle, region_t *r) {
    switch (cmd) {
        case NPH_GET_KEYS: case NPH_SET_KEYS:             *r = (region_t){ s->keymap, NPH_KEYMAP_BYTES, true, NPH_D_KEYMAP, NPH_R_KEYMAP, 0 }; return true;
        case NPH_GET_MACRO: case NPH_SET_MACRO:           *r = (region_t){ s->macro, NPH_MACRO_BYTES, true, NPH_D_MACRO, NPH_R_MACRO, 0 }; return true;
        case NPH_GET_LIGHT: case NPH_SET_LIGHT:           *r = (region_t){ s->light[handle & 1], NPH_LIGHT_BYTES, true, NPH_D_LIGHT, NPH_R_LIGHT, (uint16_t)((handle & 1) * NPH_LIGHT_BYTES) }; return true;
        case NPH_GET_TAPDANCE: case NPH_SET_TAPDANCE:     *r = (region_t){ s->td, sizeof s->td, true, NPH_D_TD, NPH_R_TD, 0 }; return true;
        case NPH_GET_TGL: case NPH_SET_TGL:               *r = (region_t){ s->tgl, sizeof s->tgl, true, NPH_D_TGL, NPH_R_TGL, 0 }; return true;
        case NPH_GET_SOCD: case NPH_SET_SOCD:             *r = (region_t){ s->socd, sizeof s->socd, true, NPH_D_SOCD, NPH_R_SOCD, 0 }; return true;
        case NPH_GET_KBFUNC: case NPH_SET_KBFUNC:         *r = (region_t){ s->kbfunc, sizeof s->kbfunc, true, NPH_D_MISC, NPH_R_KBFUNC, 0 }; return true;
        case NPH_GET_SLEEP: case NPH_SET_SLEEP:           *r = (region_t){ s->sleepcfg, sizeof s->sleepcfg, true, NPH_D_MISC, NPH_R_SLEEP, 0 }; return true;
        case NPH_GET_APPDEF: case NPH_SET_APPDEF:         *r = (region_t){ s->appdef, sizeof s->appdef, true, NPH_D_MISC, NPH_R_APPDEF, 0 }; return true;
        case NPH_GET_FIRMWARE:                            *r = (region_t){ s->firmware, sizeof s->firmware, false, 0, 0, 0 }; return true;
        case NPH_GET_DEFAULT_KEYS:                        *r = (region_t){ (uint8_t *)BD_DEFAULT_KEYMAP, NPH_KEYMAP_BYTES, false, 0, 0, 0 }; return true;
        default: return false;
    }
}

static bool is_write(uint8_t cmd) {
    switch (cmd) {
        case NPH_SET_KEYS: case NPH_SET_SOCD: case NPH_SET_TAPDANCE: case NPH_SET_TGL: case NPH_SET_MACRO:
        case NPH_SET_LIGHT: case NPH_SET_KBFUNC: case NPH_SET_SLEEP: case NPH_SET_APPDEF: case NPH_SET_KEYUPLOAD:
        case NPH_SET_DEBOUNCE: return true;
        default: return false;
    }
}

static void put_response_header(uint8_t out[NPH_FRAME], const uint8_t in[NPH_FRAME]) {
    memset(out, 0, NPH_FRAME);
    out[0] = 0xAA;
    out[1] = in[1];
    out[4] = in[4]; out[5] = in[5]; out[6] = in[6]; out[7] = in[7];   /* the device echoes the request header */
}

bool nph_handle(nph_t *s, const uint8_t in[NPH_FRAME], uint8_t out[NPH_FRAME]) {
    if (in[0] != 0x55) return false;
    if (nph_checksum(in) != in[3]) return false;          /* corrupt frame: the real device stays silent */
    const uint8_t cmd = in[1];

    /* handshake: the host picks K = payload[20] (or 170); we echo the payload XORed with K */
    if (cmd == NPH_SET_SECRET) {
        uint8_t k = in[NPH_DATA_OFF + 20];
        s->session_key = k ? k : 170;
        s->have_key = true;
        const uint8_t K = s->session_key;
        memset(out, 0, NPH_FRAME);
        out[0] = 0xAA; out[1] = cmd;
        out[4] = out[5] = out[6] = out[7] = K;
        for (int i = NPH_DATA_OFF; i < NPH_FRAME; i++) out[i] = in[i] ^ K;
        out[3] = nph_checksum(out);
        return true;
    }
    if (!s->have_key) return false;

    const uint8_t K = s->session_key;
    const uint8_t len = in[4] ^ K;
    const uint16_t addr = (uint16_t)((in[5] ^ K) | ((in[6] ^ K) << 8));
    const uint8_t handle = in[7] ^ K;
    const uint8_t n = len > NPH_MAX_DATA ? NPH_MAX_DATA : len;

    put_response_header(out, in);

    if (cmd == NPH_SET_KEYUPLOAD) {                          /* start / stop macro recording */
        s->keyupload = n >= 1 && (in[NPH_DATA_OFF] ^ K) != 0;
        memcpy(&out[NPH_DATA_OFF], &in[NPH_DATA_OFF], n);
        out[3] = nph_checksum(out);
        return true;
    }
    if (is_write(cmd)) {
        region_t r;
        if (region_for(s, cmd, handle, &r) && r.rw) {
            for (uint8_t i = 0; i < n; i++)
                if (addr + i < r.size) r.data[addr + i] = in[NPH_DATA_OFF + i] ^ K;
            s->dirty |= r.dirty;
            if (n) mark(s, r.idx, (uint16_t)(r.base + addr), n);
        }
        memcpy(&out[NPH_DATA_OFF], &in[NPH_DATA_OFF], n);       /* acknowledge by echoing the data as sent */
        out[3] = nph_checksum(out);
        return true;
    }

    uint8_t plain[NPH_MAX_DATA] = {0};
    switch (cmd) {
        case NPH_GET_BASE: {
            /* [0] = active profile (OS slide switch); the rest is the V3's identity block, kept verbatim */
            const uint8_t base[8] = {0x00, 0x08, 0x04, 0x06, 0x10, 0x82, 0x00, 0x00};
            memcpy(plain, base, 8);
            plain[0] = s->hooks.current_mode ? s->hooks.current_mode() & 1 : 0;
            break;
        }
        case NPH_GET_LIGHT_COUNT:
            plain[0] = 104;     /* the V3 reports 84 key + 20 side LEDs; we synthesise 20 side entries from our 12 */
            break;
        case NPH_GET_LED_COLOR:
            if (s->hooks.led_read) s->hooks.led_read(addr, plain, n);
            break;
        case NPH_GET_STORE: if (s->hooks.status) s->hooks.status(plain); break;
        case NPH_GET_APPDEF_SIZE: plain[0] = NPH_APPDEF_BYTES >> 8; plain[1] = NPH_APPDEF_BYTES & 0xFF; break;
        case NPH_RESTORE_KEYS:
            memcpy(s->keymap, BD_DEFAULT_KEYMAP, NPH_KEYMAP_BYTES); s->dirty |= NPH_D_KEYMAP; mark_all(s, NPH_R_KEYMAP, NPH_KEYMAP_BYTES); break;
        case NPH_RESET_SOCD: memset(s->socd, 0xFF, sizeof s->socd); s->dirty |= NPH_D_SOCD; mark_all(s, NPH_R_SOCD, sizeof s->socd); break;
        case NPH_RESET_TAPDANCE: memset(s->td, 0xFF, sizeof s->td); s->dirty |= NPH_D_TD; mark_all(s, NPH_R_TD, sizeof s->td); break;
        case NPH_RESET_TGL: memset(s->tgl, 0xFF, sizeof s->tgl); s->dirty |= NPH_D_TGL; mark_all(s, NPH_R_TGL, sizeof s->tgl); break;
        case NPH_RESTORE_FACTORY: if (s->hooks.factory_reset) s->hooks.factory_reset(); break;
        case NPH_SET_IAP: if (s->hooks.enter_dfu) s->hooks.enter_dfu(); break;
        default: {
            region_t r;
            if (!region_for(s, cmd, handle, &r)) return false;   /* unknown command: stay silent */
            for (uint8_t i = 0; i < n; i++) plain[i] = (addr + i < r.size) ? r.data[addr + i] : 0;
            if (cmd == NPH_GET_DEFAULT_KEYS)        /* nuphy.io only treats the knob / button position (slot 60) as a bindable key if its factory default is volume-mute, as on the V3 */
                for (uint8_t i = 0; i < n; i++) { const uint16_t o = (uint16_t)((addr + i) % (NPH_SLOTS * 2)); if (o == 60 * 2) plain[i] = 0x00; else if (o == 60 * 2 + 1) plain[i] = 0xA8; }
        }
    }
    for (uint8_t i = 0; i < n; i++) out[NPH_DATA_OFF + i] = plain[i] ^ K;
    out[3] = nph_checksum(out);
    return true;
}
