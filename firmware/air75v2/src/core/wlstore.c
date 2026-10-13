#include "wlstore.h"
#include <string.h>

#define MAGIC 0x3348504Eu              /* "NPH3" */
#define HDR   16u
#define TAG   0x5A00u

static uint32_t rd32(const wl_t *w, uint32_t off) { return (uint32_t)w->ops.rd16(w->ops.ctx, off) | ((uint32_t)w->ops.rd16(w->ops.ctx, off + 2) << 16); }
static uint32_t pad2(uint32_t n) { return (n + 1u) & ~1u; }

static uint32_t crc32_step(uint32_t c, uint8_t b) { c ^= b; for (int i = 0; i < 8; i++) c = (c >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(c & 1)); return c; }
static uint16_t crc16_step(uint16_t c, uint8_t b) { c ^= (uint16_t)(b << 8); for (int i = 0; i < 8; i++) c = (uint16_t)((c & 0x8000) ? (c << 1) ^ 0x1021 : (c << 1)); return c; }

static uint32_t payload_len(const wl_t *w) { uint32_t t = 0; for (uint8_t i = 0; i < w->nseg; i++) t += w->seg[i].n; return t; }

void wl_init(wl_t *w, const wl_ops_t *ops, const wl_seg_t *segs, uint8_t nseg) {
    memset(w, 0, sizeof *w);
    w->ops = *ops; w->nseg = nseg > WL_MAX_SEG ? WL_MAX_SEG : nseg;
    for (uint8_t i = 0; i < w->nseg; i++) { w->seg[i] = segs[i]; w->lo[i] = 0xFFFF; w->hi[i] = 0; }
    w->bank = 1; w->log_off = WL_BANK_BYTES;          /* nothing saved: the first commit snapshots into bank 0 */
}

/* ---- load ---- */

static bool bank_valid(const wl_t *w, uint8_t b, uint32_t *seq, uint32_t *len) {
    const uint32_t base = b * WL_BANK_BYTES;
    if (rd32(w, base) != MAGIC) return false;
    *seq = rd32(w, base + 4); *len = rd32(w, base + 8);
    if (*len == 0 || *len > WL_BANK_BYTES - HDR || (*len & 1)) return false;
    uint32_t c = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < *len; i += 2) { const uint16_t v = w->ops.rd16(w->ops.ctx, base + HDR + i); c = crc32_step(crc32_step(c, (uint8_t)v), (uint8_t)(v >> 8)); }
    return ~c == rd32(w, base + 12);
}

static uint8_t rd8(const wl_t *w, uint32_t off) { const uint16_t v = w->ops.rd16(w->ops.ctx, off & ~1u); return (uint8_t)((off & 1) ? v >> 8 : v); }

bool wl_load(wl_t *w) {
    uint32_t s0 = 0, l0 = 0, s1 = 0, l1 = 0;
    const bool a = bank_valid(w, 0, &s0, &l0), b = bank_valid(w, 1, &s1, &l1);
    if (!a && !b) return false;
    w->loaded = true;
    const uint8_t pick = (a && b) ? (s1 > s0 ? 1 : 0) : (b ? 1 : 0);
    const uint32_t base = pick * WL_BANK_BYTES, len = pick ? l1 : l0;
    w->bank = pick; w->seq = pick ? s1 : s0;

    uint32_t off = 0;                                                    /* snapshot (older images may lack trailing segments) */
    for (uint8_t i = 0; i < w->nseg && off + w->seg[i].n <= len; i++) {
        for (uint16_t j = 0; j < w->seg[i].n; j++) w->seg[i].p[j] = rd8(w, base + HDR + off + j);
        off += w->seg[i].n;
    }

    uint32_t p = HDR + pad2(len);                                        /* replay the log */
    w->need_compact = false;
    for (;;) {
        if (p + 8 > WL_BANK_BYTES) break;
        const uint16_t h0 = w->ops.rd16(w->ops.ctx, base + p);
        if (h0 == 0xFFFF) break;
        const uint16_t o = w->ops.rd16(w->ops.ctx, base + p + 2), n = w->ops.rd16(w->ops.ctx, base + p + 4), idx = h0 & 0xFF;
        const uint32_t end = p + 6 + pad2(n) + 2;
        if ((h0 & 0xFF00) != TAG || idx >= w->nseg || n == 0 || n > WL_MAX_REC || (uint32_t)o + n > w->seg[idx].n || end > WL_BANK_BYTES) { w->need_compact = true; break; }
        uint16_t c = 0xFFFF;
        for (uint32_t k = 0; k < 6 + pad2(n); k++) c = crc16_step(c, rd8(w, base + p + k));
        if (c != w->ops.rd16(w->ops.ctx, base + end - 2)) { w->need_compact = true; break; }
        for (uint16_t k = 0; k < n; k++) w->seg[idx].p[o + k] = rd8(w, base + p + 6 + k);
        p = end;
    }
    w->log_off = p;
    return true;
}

/* ---- marking ---- */

void wl_mark(wl_t *w, uint8_t seg, uint16_t off, uint16_t len) {
    if (seg >= w->nseg || len == 0 || off >= w->seg[seg].n) return;
    uint32_t end = (uint32_t)off + len; if (end > w->seg[seg].n) end = w->seg[seg].n;
    if (off < w->lo[seg]) w->lo[seg] = off;
    if (end > w->hi[seg]) w->hi[seg] = (uint16_t)end;
}
void wl_mark_all(wl_t *w) { for (uint8_t i = 0; i < w->nseg; i++) wl_mark(w, i, 0, w->seg[i].n); }
bool wl_dirty(const wl_t *w) { if (w->need_compact) return true; for (uint8_t i = 0; i < w->nseg; i++) if (w->hi[i] > w->lo[i]) return true; return false; }

static void clear_marks(wl_t *w) { for (uint8_t i = 0; i < w->nseg; i++) { w->lo[i] = 0xFFFF; w->hi[i] = 0; } }

static uint32_t log_needed(const wl_t *w) {
    uint32_t t = 0;
    for (uint8_t i = 0; i < w->nseg; i++) if (w->hi[i] > w->lo[i]) {
        for (uint32_t o = w->lo[i]; o < w->hi[i]; o += WL_MAX_REC) { uint32_t n = w->hi[i] - o; if (n > WL_MAX_REC) n = WL_MAX_REC; t += 6 + pad2(n) + 2; }
    }
    return t;
}
bool wl_would_compact(const wl_t *w) { return wl_dirty(w) && (w->need_compact || w->log_off + log_needed(w) > WL_BANK_BYTES); }

/* ---- commit ---- */

static bool compact(wl_t *w) {
    const uint8_t tb = (uint8_t)(1 - w->bank);
    const uint32_t base = tb * WL_BANK_BYTES, len = payload_len(w);
    for (uint32_t p = 0; p < WL_BANK_BYTES; p += WL_PAGE) w->ops.erase(w->ops.ctx, base + p);

    uint32_t off = HDR, c = 0xFFFFFFFFu; uint8_t lo = 0; bool have_lo = false;
    for (uint8_t i = 0; i < w->nseg; i++) for (uint16_t j = 0; j < w->seg[i].n; j++) {
        const uint8_t b = w->seg[i].p[j]; c = crc32_step(c, b);
        if (!have_lo) { lo = b; have_lo = true; }
        else { w->ops.prog16(w->ops.ctx, base + off, (uint16_t)(lo | (b << 8))); off += 2; have_lo = false; }
    }
    const uint32_t seq = w->seq + 1, crc = ~c;
    w->ops.prog16(w->ops.ctx, base + 4, (uint16_t)seq);  w->ops.prog16(w->ops.ctx, base + 6, (uint16_t)(seq >> 16));
    w->ops.prog16(w->ops.ctx, base + 8, (uint16_t)len);  w->ops.prog16(w->ops.ctx, base + 10, (uint16_t)(len >> 16));
    w->ops.prog16(w->ops.ctx, base + 12, (uint16_t)crc); w->ops.prog16(w->ops.ctx, base + 14, (uint16_t)(crc >> 16));
    w->ops.prog16(w->ops.ctx, base + 0, (uint16_t)MAGIC); w->ops.prog16(w->ops.ctx, base + 2, (uint16_t)(MAGIC >> 16));   /* magic last */

    uint32_t vs, vl;
    const uint8_t keep = w->bank; w->bank = tb;
    if (!bank_valid(w, tb, &vs, &vl) || vs != seq) { w->bank = keep; return false; }
    w->seq = seq; w->log_off = HDR + pad2(len); w->need_compact = false; w->stat_compactions++;
    clear_marks(w);
    return true;
}

static bool append(wl_t *w) {
    const uint32_t base = w->bank * WL_BANK_BYTES;
    for (uint8_t i = 0; i < w->nseg; i++) if (w->hi[i] > w->lo[i]) {
        for (uint32_t o = w->lo[i]; o < w->hi[i]; o += WL_MAX_REC) {
            uint32_t n = w->hi[i] - o; if (n > WL_MAX_REC) n = WL_MAX_REC;
            uint8_t rec[6 + WL_MAX_REC + 2]; const uint32_t body = 6 + pad2(n);
            rec[0] = i; rec[1] = TAG >> 8; rec[2] = (uint8_t)o; rec[3] = (uint8_t)(o >> 8); rec[4] = (uint8_t)n; rec[5] = (uint8_t)(n >> 8);
            memcpy(&rec[6], &w->seg[i].p[o], n); if (n & 1) rec[6 + n] = 0;
            uint16_t c = 0xFFFF; for (uint32_t k = 0; k < body; k++) c = crc16_step(c, rec[k]);
            rec[body] = (uint8_t)c; rec[body + 1] = (uint8_t)(c >> 8);
            for (uint32_t k = 0; k < body + 2; k += 2) w->ops.prog16(w->ops.ctx, base + w->log_off + k, (uint16_t)(rec[k] | (rec[k + 1] << 8)));   /* CRC last */
            for (uint32_t k = 0; k < body + 2; k += 2) if (w->ops.rd16(w->ops.ctx, base + w->log_off + k) != (uint16_t)(rec[k] | (rec[k + 1] << 8))) { w->need_compact = true; return false; }
            w->log_off += body + 2; w->stat_appends++;
        }
    }
    clear_marks(w);
    return true;
}

bool wl_commit(wl_t *w) {
    if (!wl_dirty(w) && !w->need_compact) return true;
    if (!w->need_compact && w->log_off + log_needed(w) <= WL_BANK_BYTES && append(w)) return true;
    if (compact(w) || compact(w)) return true;                          /* a failed snapshot is retried once */
    w->stat_fail++;
    return false;
}
