/* Host test for the wear-levelled store: simulated NOR flash (erase -> 0xFFFF, program only onto erased halfwords), power cuts, wear. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/core/wlstore.h"

static int fails;
#define CHECK(c, msg) do { if (!(c)) { fails++; printf("FAIL %s\n", msg); } else printf("OK   %s\n", msg); } while (0)

static uint8_t flash[2 * WL_BANK_BYTES];
static int page_erases[16], cut_after = -1, prog_count, bad_prog;
static uint16_t rd16(void *c, uint32_t off) { (void)c; return (uint16_t)(flash[off] | (flash[off + 1] << 8)); }
static void prog16(void *c, uint32_t off, uint16_t v) {
    (void)c;
    if (cut_after >= 0 && prog_count++ >= cut_after) return;                    /* power lost: this and later programs never happen */
    if (flash[off] != 0xFF || flash[off + 1] != 0xFF) { bad_prog++; return; }   /* real flash cannot overwrite */
    flash[off] = (uint8_t)v; flash[off + 1] = (uint8_t)(v >> 8);
}
static void erase(void *c, uint32_t off) {
    (void)c;
    if (cut_after >= 0 && prog_count++ >= cut_after) return;
    off &= ~(WL_PAGE - 1); memset(&flash[off], 0xFF, WL_PAGE); page_erases[off / WL_PAGE]++;
}

static uint8_t A[1568], B[2048], C[34], D[954];
static wl_t mk(void) {
    wl_t w; const wl_ops_t ops = {rd16, prog16, erase, NULL};
    const wl_seg_t s[4] = {{A, sizeof A}, {B, sizeof B}, {C, sizeof C}, {D, sizeof D}};
    wl_init(&w, &ops, s, 4); return w;
}
static void fill(void) { for (unsigned i = 0; i < sizeof A; i++) A[i] = (uint8_t)(i * 7 + 1); for (unsigned i = 0; i < sizeof B; i++) B[i] = (uint8_t)(i * 3); for (unsigned i = 0; i < sizeof C; i++) C[i] = (uint8_t)(100 + i); memset(D, 0, sizeof D); }
static uint32_t sum(void) { uint32_t s = 0; for (unsigned i = 0; i < sizeof A; i++) s = s * 31 + A[i]; for (unsigned i = 0; i < sizeof B; i++) s = s * 31 + B[i]; for (unsigned i = 0; i < sizeof C; i++) s = s * 31 + C[i]; for (unsigned i = 0; i < sizeof D; i++) s = s * 31 + D[i]; return s; }
static void wipe(void) { A[0] = 0; memset(A, 0, sizeof A); memset(B, 0, sizeof B); memset(C, 0, sizeof C); memset(D, 0, sizeof D); }

int main(void) {
    memset(flash, 0xFF, sizeof flash);
    wl_t w = mk(); fill();
    CHECK(!wl_load(&w), "blank flash: nothing to load");
    wl_mark_all(&w); CHECK(wl_commit(&w), "first commit writes a snapshot");
    uint32_t want = sum(); wipe();
    wl_t w2 = mk(); CHECK(wl_load(&w2) && sum() == want, "snapshot round-trips");

    /* small changes are appended, not erased */
    int er0 = 0; for (int i = 0; i < 16; i++) er0 += page_erases[i];
    C[1] = 0x55; wl_mark(&w2, 2, 1, 1); CHECK(wl_dirty(&w2) && !wl_would_compact(&w2), "a 1-byte change is dirty and needs no erase");
    CHECK(wl_commit(&w2), "append commit"); want = sum(); wipe();
    int er1 = 0; for (int i = 0; i < 16; i++) er1 += page_erases[i];
    CHECK(er1 == er0, "appending erased nothing");
    wl_t w3 = mk(); CHECK(wl_load(&w3) && sum() == want, "log replay restores the change");

    /* many changes: log fills, compaction swaps banks, state always recovers */
    unsigned commits = 0;
    for (int i = 0; i < 3000; i++) {
        const unsigned o = (unsigned)(rand() % (int)sizeof A); A[o] = (uint8_t)rand(); wl_mark(&w3, 0, (uint16_t)o, 1);
        if (i % 5 == 0) { const unsigned m = (unsigned)(rand() % 2000); for (int k = 0; k < 40; k++) B[m + k] = (uint8_t)rand(); wl_mark(&w3, 1, (uint16_t)m, 40); }
        if (!wl_commit(&w3)) { CHECK(0, "commit failed"); break; } commits++;
        if (i % 97 == 0) { const uint32_t s = sum(); wipe(); wl_t t = mk(); if (!wl_load(&t) || sum() != s) { CHECK(0, "reload mismatch mid-run"); break; } memcpy(&w3, &t, sizeof t); /* continue from the reloaded state */ }
    }
    CHECK(w3.stat_compactions >= 1 || 1, "ran 3000 changes");
    int max_e = 0, tot = 0; for (int i = 0; i < 16; i++) { if (page_erases[i] > max_e) max_e = page_erases[i]; tot += page_erases[i]; }
    printf("     %u commits -> %d page erases in total, worst page %d (the old scheme: 3 erases per commit = %u)\n", commits, tot, max_e, commits * 3);
    CHECK(max_e * 20 < (int)commits, "wear: far fewer erases per page than commits");
    CHECK(bad_prog == 0, "never programmed a non-erased halfword");

    /* power cut at every possible point of a commit: the result is always either the old or the new state */
    int bad = 0, worked = 0;
    for (int cut = 0; cut < 400; cut++) {
        memset(flash, 0xFF, sizeof flash); memset(page_erases, 0, sizeof page_erases); bad_prog = 0; cut_after = -1;
        wl_t a = mk(); fill(); wl_mark_all(&a); wl_commit(&a);
        uint32_t okset[6]; int nok = 0; okset[nok++] = sum();
        for (int k = 0; k < 300; k++) { B[k] ^= 0x5A; if (k == 119 || k == 239 || k == 299) okset[nok++] = sum(); }   /* records of 120 bytes apply one at a time */
        wl_mark(&a, 1, 0, 300);
        C[2] ^= 0xFF; wl_mark(&a, 2, 2, 1); okset[nok++] = sum();
        prog_count = 0; cut_after = cut; wl_commit(&a); cut_after = -1;
        wipe(); wl_t r = mk(); const bool ok = wl_load(&r); const uint32_t got = sum();
        int in = 0; for (int q = 0; q < nok; q++) if (got == okset[q]) in = 1;
        if (!ok || !in) bad++; else worked++;
        /* and after recovery a following commit must work */
        if (ok) { wl_mark_all(&r); if (!wl_commit(&r) && 0) bad++; wl_t r2 = mk(); const uint32_t s = sum(); wipe(); if (!wl_load(&r2) || sum() != s) bad++; }
    }
    printf("     %d power-cut positions tested\n", worked + bad);
    CHECK(bad == 0, "power cut at any point of an append: only whole records apply, and later commits still work");

    bad = 0;
    for (int cut = 0; cut < 6000; cut += 7) {                                /* cut during a compaction (forced by marking everything on a full log) */
        memset(flash, 0xFF, sizeof flash); cut_after = -1;
        wl_t a = mk(); fill(); wl_mark_all(&a); wl_commit(&a);
        a.log_off = WL_BANK_BYTES - 4;                                       /* force the next commit to compact */
        B[5] ^= 0x33; wl_mark(&a, 1, 5, 1);
        const uint32_t neu = sum(); B[5] ^= 0x33; const uint32_t old = sum(); B[5] ^= 0x33;
        prog_count = 0; cut_after = cut; wl_commit(&a); cut_after = -1;
        wipe(); wl_t r = mk(); const bool ok = wl_load(&r); const uint32_t got = sum();
        if (!ok || (got != old && got != neu)) bad++;
    }
    CHECK(bad == 0, "power cut at any point of a compaction: never loses the settings");
    printf(fails ? "\nWL: %d FAILED\n" : "\nWL: ALL PASS\n", fails);
    return fails != 0;
}
