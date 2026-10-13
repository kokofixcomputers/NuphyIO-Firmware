/*
 * NuPhy "V3" configuration protocol, device side. Portable C: no hardware dependencies.
 *
 * Wire format (64-byte HID reports; verified against captures of the official nuphy.io app talking to a real Air75 V3):
 *   [0] 0x55 (host->dev) / 0xAA (dev->host)   [1] command   [2] 0
 *   [3] checksum = sum of bytes 4..63 (mod 256)
 *   [4] len^K  [5] addrLo^K  [6] addrHi^K  [7] handle^K     [8..] data^K   (K = session key; padding is raw 0)
 * Every command reads or writes a memory region. Handshake (0xEE): K = payload[20] || 170, payload is echoed ^K.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define NPH_FRAME      64
#define NPH_DATA_OFF   8
#define NPH_MAX_DATA   (NPH_FRAME - NPH_DATA_OFF)

/* command ids */
enum {
    NPH_SET_SECRET = 0xEE,
    NPH_GET_BASE = 0xA0, NPH_GET_FIRMWARE = 0xA1,
    NPH_GET_DEFAULT_KEYS = 0xB1, NPH_GET_KEYS = 0xB2, NPH_SET_KEYS = 0xB3, NPH_RESTORE_KEYS = 0xB4,
    NPH_GET_SOCD = 0xB5, NPH_SET_SOCD = 0xB6, NPH_RESET_SOCD = 0xB7,
    NPH_GET_TAPDANCE = 0xB8, NPH_SET_TAPDANCE = 0xB9, NPH_RESET_TAPDANCE = 0xBA,
    NPH_GET_TGL = 0xBB, NPH_SET_TGL = 0xBC, NPH_RESET_TGL = 0xBD,
    NPH_SET_KEYUPLOAD = 0xC1, NPH_GET_MACRO = 0xC2, NPH_SET_MACRO = 0xC3,
    NPH_GET_LIGHT_COUNT = 0xD1, NPH_GET_LED_COLOR = 0xD2, NPH_GET_LIGHT = 0xD5, NPH_SET_LIGHT = 0xD6,
    NPH_GET_KBFUNC = 0xE1, NPH_SET_KBFUNC = 0xE2, NPH_SET_DEBOUNCE = 0xE3,
    NPH_RESTORE_FACTORY = 0xF1, NPH_GET_SLEEP = 0xF3, NPH_SET_SLEEP = 0xF5,
    NPH_GET_APPDEF_SIZE = 0xFA, NPH_GET_APPDEF = 0xFB, NPH_SET_APPDEF = 0xFC,
    NPH_SET_IAP = 0xEF,
    NPH_GET_STORE = 0xF8,                          /* ours (the V3 has no such command): flash-store diagnostics */
};

/* region geometry */
#define NPH_LAYERS        8
#define NPH_SLOTS         98                       /* per layer (V3 layout; slots 96/97 = knob turns) */
#define NPH_KEYMAP_BYTES  (NPH_LAYERS * NPH_SLOTS * 2)
#define NPH_MACRO_BYTES   2048
#define NPH_TD_SLOTS      64
#define NPH_TGL_SLOTS     64
#define NPH_SOCD_SLOTS    64
#define NPH_LIGHT_BYTES   17
#define NPH_APPDEF_BYTES  954                      /* the V3's "app define" blob (0xFA size = 03 BA); byte 168 = accessory: 0 knob, 1 button (the button is keymap slot 60, the same slot the V3's knob press uses) */
/* open-firmware revision, reported by 0xF8: 1 = first builds, 2 = the top-right button lives at slot 60 (what nuphy.io and Air Deck address) */
#define NPH_FW_REV 2
#define NPH_APPDEF_ACCESSORY 168
/* fwcfg bytes */
#define NPH_FW_CAPS_OFF   0    /* 1 = caps-lock indicator disabled (FN+P toggles) */
#define NPH_FW_BAT_STRIP  1    /* 1 = left side light shows the battery (FN+\ toggles); the right one stays under software control */
#define NPH_FW_BRIGHT_REV 3    /* 1 once the saved brightness has been converted to the 43 %-is-100 % scale */
#define NPH_FW_SIDE_PREV  2    /* side brightness remembered while the side light is switched off with FN2+/ */
#define NPH_LED_COUNT_V3  104                      /* what the V3 app expects: 84 key LEDs + 20 side LEDs */

/* settings segments, in the order the flash store saves them (see wlstore.h) */
enum { NPH_R_KEYMAP = 0, NPH_R_MACRO, NPH_R_LIGHT, NPH_R_TD, NPH_R_TGL, NPH_R_SOCD, NPH_R_KBFUNC, NPH_R_SLEEP, NPH_R_APPDEF, NPH_R_FW, NPH_R_COUNT };

/* dirty flags: which persisted regions changed since the last commit */
enum { NPH_D_KEYMAP = 1, NPH_D_MACRO = 2, NPH_D_LIGHT = 4, NPH_D_TD = 8, NPH_D_TGL = 16, NPH_D_SOCD = 32, NPH_D_MISC = 64 };

typedef struct {
    /* platform hooks (all optional) */
    void (*led_read)(uint16_t byte_off, uint8_t *dst, uint8_t len);   /* live LED colours for 0xD2 */
    void (*enter_dfu)(void);                                           /* 0xEF */
    void (*factory_reset)(void);                                       /* 0xF1 */
    uint8_t (*current_mode)(void);                                     /* 0 = Mac, 1 = Windows (OS slide switch) */
    void (*status)(uint8_t out[32]);                                   /* flash-store diagnostics for 0xF8 */
    void (*mark)(uint8_t seg, uint16_t off, uint16_t len);             /* a settings range changed: persist it */
} nph_hooks_t;

typedef struct {
    uint8_t keymap[NPH_KEYMAP_BYTES];
    uint8_t macro[NPH_MACRO_BYTES];
    uint8_t light[2][NPH_LIGHT_BYTES];
    uint8_t td[NPH_TD_SLOTS * 8];
    uint8_t tgl[NPH_TGL_SLOTS * 2];
    uint8_t socd[NPH_SOCD_SLOTS * 4];
    uint8_t kbfunc[4];
    uint8_t sleepcfg[4];
    uint8_t appdef[NPH_APPDEF_BYTES];
    uint8_t fwcfg[16];                         /* firmware-private settings (not exposed over the protocol) - see NPH_FW_* */
    bool    keyupload;                         /* macro-recording mode: key events go to the host app instead of the OS */
    uint8_t firmware[8];
    uint8_t session_key;
    bool    have_key;
    uint8_t dirty;
    nph_hooks_t hooks;
} nph_t;

void nph_init(nph_t *s, const nph_hooks_t *hooks);
void nph_load_defaults(nph_t *s);

/* Handle one 64-byte host->device frame. Returns true if `out` holds a response to send. */
bool nph_handle(nph_t *s, const uint8_t in[NPH_FRAME], uint8_t out[NPH_FRAME]);

uint8_t nph_checksum(const uint8_t f[NPH_FRAME]);
