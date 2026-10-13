/* Wear-levelled settings store (the same idea as QMK's wear_leveling driver).
 *
 * Two banks of 16 KB (8 flash pages each). A bank starts with a full snapshot of every settings region, followed by an append-only
 * log of small update records ("region 2, offset 1, these 3 bytes"). A change therefore costs a few halfword programs and no erase;
 * only when the log is full is the other bank erased and a fresh snapshot written (~every few hundred changes), so each page sees
 * a tiny fraction of the erases the old "rewrite everything on each save" scheme caused. Power cuts are safe: a record only counts
 * once its trailing CRC is written, the snapshot header (magic) is written last, and the older bank is only erased after the
 * newer one is complete.
 *
 * Portable: the board supplies read / program / erase on a flat 32 KB area.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define WL_PAGE        2048u
#define WL_BANK_BYTES  16384u
#define WL_MAX_SEG     12
#define WL_MAX_REC     120u            /* data bytes per log record */

typedef struct {
    uint16_t (*rd16)(void *ctx, uint32_t off);
    void     (*prog16)(void *ctx, uint32_t off, uint16_t v);     /* target halfword must be erased (0xFFFF) */
    void     (*erase)(void *ctx, uint32_t off);                  /* erase the 2 KB page containing off */
    void     *ctx;
} wl_ops_t;

typedef struct { uint8_t *p; uint16_t n; } wl_seg_t;                /* n must be even */

typedef struct {
    wl_ops_t ops;
    wl_seg_t seg[WL_MAX_SEG];
    uint8_t  nseg;
    uint8_t  bank;                     /* active bank (0/1) */
    uint32_t seq;
    uint32_t log_off;                  /* next free byte in the active bank */
    bool     need_compact;
    uint16_t lo[WL_MAX_SEG], hi[WL_MAX_SEG];     /* dirty span per segment: [lo, hi) */
    uint32_t stat_appends, stat_compactions, stat_fail;
    bool     loaded;                   /* wl_load found saved settings at boot */
} wl_t;

void wl_init(wl_t *w, const wl_ops_t *ops, const wl_seg_t *segs, uint8_t nseg);
bool wl_load(wl_t *w);                                           /* restore every segment from flash; false = nothing saved yet */
void wl_mark(wl_t *w, uint8_t seg, uint16_t off, uint16_t len);  /* a range of a segment changed */
void wl_mark_all(wl_t *w);
bool wl_dirty(const wl_t *w);
bool wl_would_compact(const wl_t *w);                            /* the next commit will erase a bank (slow: ~100+ ms) */
bool wl_commit(wl_t *w);                                         /* persist everything marked; true on success */
