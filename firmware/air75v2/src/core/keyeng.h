/* Key engine: matrix events -> HID state, driven by the V3-format keymap in nph_t. Portable (host-testable). */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "nph_proto.h"

#define KE_MAX_KEYS 6          /* 6KRO boot-protocol report */

typedef struct {
    uint8_t  mods;
    uint8_t  keys[KE_MAX_KEYS];
    uint16_t consumer;
    uint8_t  system;
    uint8_t  buttons;          /* mouse buttons 1-5 (bit 0 = left, 1 = right, 2 = middle) */
    int8_t   wheel, hwheel;    /* held scroll direction: the board repeats detents while it is held */
} ke_report_t;

typedef struct {
    uint16_t entry;            /* resolved keymap entry for the current press */
    uint32_t t;                /* press time (ms) */
    uint32_t t_rel;            /* release time of the first tap (tap dance) */
    uint8_t  state;
    uint8_t  aux;
} ke_slot_t;

typedef struct {
    nph_t     *nph;
    uint8_t    mode;                       /* 0 = Mac (layers 0-3), 1 = Windows (layers 4-7) */
    ke_slot_t  slot[NPH_SLOTS];
    uint8_t    layer_cnt[NPH_LAYERS];
    uint8_t    ucnt[256];                  /* reference counts per HID usage */
    uint8_t    order[16];                  /* non-modifier usages, in press order */
    uint8_t    order_n;
    uint16_t   cons_cnt_usage;             /* currently held consumer usage (last wins) */
    uint8_t    sys_usage;
    /* macro player */
    const uint8_t *mac;                    /* event stream */
    uint16_t   mac_len, mac_pos;
    uint32_t   mac_next;
    bool       mac_active;
    uint8_t    mac_down[16], mac_down_n;   /* usages the macro currently holds */
    bool       tgl_on[64];
    bool       socd_held[32][2], socd_out[32][2];   /* per SOCD pair: key physically held / usage currently sent */
    uint8_t    socd_last[32];                       /* side (0/1) pressed most recently */
    bool       dirty;
    void     (*fn_cb)(void *ctx, uint16_t entry, bool pressed);   /* firmware functions (0x7Exx / 0x78xx): radio, brightness, ... */
    void      *fn_ctx;
} keyeng_t;

void keyeng_init(keyeng_t *k, nph_t *n);
void keyeng_set_mode(keyeng_t *k, uint8_t mode);
void keyeng_event(keyeng_t *k, uint8_t slot, bool pressed, uint32_t now);
void keyeng_tick(keyeng_t *k, uint32_t now);
void keyeng_release_all(keyeng_t *k);
bool keyeng_report(keyeng_t *k, ke_report_t *out);   /* true if the report changed since the last call */
uint8_t keyeng_layer(const keyeng_t *k);           /* active layer index: FN0..FN3 -> 0..3 in Mac mode, 4..7 in Windows mode (what the V3 reports in A2) */
uint16_t keyeng_usage_to_consumer(uint8_t usage);    /* QMK-numbered media keys 0xA8.. -> HID consumer usage (0 = none) */
