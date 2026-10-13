#include <stdio.h>
#include <string.h>
#include "../src/core/light.h"
static int fails;
#define CHECK(c, msg) do { if (!(c)) { fails++; printf("FAIL %s\n", msg); } else printf("OK   %s\n", msg); } while (0)
static int lit(const light_t *l) { int n = 0; for (int i = 0; i < 84; i++) n += l->key[i].r || l->key[i].g || l->key[i].b; return n; }
int main(void) {
    static nph_t n; static light_t l;
    nph_init(&n, NULL); light_init(&l, &n);
    int dark_effects = 0;
    for (int e = 0; e <= 22; e++) {                        /* every effect id, incl. out-of-range: must render without crashing */
        n.light[0][0] = (uint8_t)e;
        for (uint32_t t = 0; t < 3000; t += 100) light_render(&l, t, false);
        if (e >= 1 && e <= 20 && e != 11 && e != 12 && e != 13 && e != 15 && lit(&l) == 0) { dark_effects++; printf("  effect %d rendered fully dark\n", e); }
    }
    CHECK(dark_effects == 0, "all 20 back-light effects produce light (reactive ones excluded)");
    n.light[0][0] = 3; n.light[0][4] = 0; n.light[0][6] = 0; n.light[0][7] = 0x95; n.light[0][8] = 0xFF; n.light[0][1] = 100;
    light_render(&l, 1000, false);
    CHECK(l.key[0].r == 0 && l.key[0].b > 60 && l.key[0].b < 100, "Static + custom colour paints every key with the chosen colour (100 % = 43 % of the old 200 cap)");
    n.light[0][1] = 0; light_render(&l, 1100, false);
    CHECK(lit(&l) == 0, "brightness 0 = off");
    n.light[0][1] = 50; n.light[0][0] = 12; light_render(&l, 2000, false); int before = lit(&l);
    light_key_event(&l, 49, true, 2005); light_render(&l, 2010, false);
    CHECK(before == 0 && lit(&l) == 1, "reactive 'Point' lights only the pressed key");
    light_render(&l, 2000, true); n.light[0][0] = 3; light_render(&l, 2100, true);
    int caps_led = -1; for (int i = 0; i < 84; i++) if (l.key[i].r >= 12 && l.key[i].r == l.key[i].g && l.key[i].g == l.key[i].b) caps_led = i;   /* the only grey key among coloured ones */
    CHECK(caps_led >= 0, "caps-lock indicator turns the Caps key white");
    uint8_t snap[312]; n.light[0][0] = 3; n.light[0][4] = 1; n.light[0][1] = 100; light_render(&l, 3000, false); light_v3_snapshot(&l, snap);
    CHECK(snap[0] == snap[3] && (snap[0] || snap[1] || snap[2]), "V3 snapshot: first key LED (Esc) populated");
    /* LED 83 is the V3's knob LED (last in the list): the V2 puts its top-right button there. Slot 61 (Enter) follows slot 59 directly. */
    CHECK((snap[83 * 3] || snap[83 * 3 + 1] || snap[83 * 3 + 2]) && (snap[82 * 3] || snap[82 * 3 + 1] || snap[82 * 3 + 2]), "V3 snapshot: the button LED (index 83, slot 60) is present, the key before it is lit");
    CHECK(snap[84 * 3] || snap[84 * 3 + 1] || snap[84 * 3 + 2], "V3 snapshot: 20 synthesised side LEDs present");
    /* battery bar: left strip shows 90 % = five green LEDs, right strip is untouched; charging makes the next LED breathe */
    n.light[0][9 + 1] = 100; n.light[0][9] = 2; n.light[0][9 + 3] = 0; n.light[0][14] = 0; n.light[0][15] = 0; n.light[0][16] = 255;
    l.bat_on = false; light_render(&l, 5000, false); rgb_t right[6]; memcpy(right, &l.side[6], sizeof right);
    l.bat_on = true; l.bat_pct = 90; l.bat_charging = false; light_render(&l, 5000, false);
    CHECK(l.side[0].g > 0 && l.side[4].g > 0 && l.side[5].g == 0 && l.side[0].r == 0, "battery 90 %: five green LEDs on the left strip");
    CHECK(memcmp(right, &l.side[6], sizeof right) == 0, "the right side light keeps following the software-controlled state");
    l.bat_pct = 15; light_render(&l, 5000, false);
    CHECK(l.side[0].r > 0 && l.side[1].r == 0 && l.side[1].g == 0, "battery 15 %: one red LED");
    l.bat_pct = 50; l.bat_charging = true; light_render(&l, 5400, false);
    CHECK(l.side[2].g > 0 && (l.side[3].r || l.side[3].g), "charging: the next LED breathes");
    /* every colour-capable effect honours the custom colour: pure blue in, only blue pixels out (reactive effects get a few presses first) */
    {
        const int effects[] = {1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15};   /* not 9: the real V3's Galaxy stays rainbow even with a custom colour */
        for (unsigned q = 0; q < sizeof effects / sizeof effects[0]; q++) {
            static light_t lc; light_init(&lc, &n);
            const uint8_t st[9] = {(uint8_t)effects[q], 100, 2, 0, 0, 0, 0, 0, 255};
            memcpy(n.light[0], st, 9);
            int wrong = 0, lit_px = 0;
            for (uint32_t t2 = 0; t2 < 6000; t2 += 30) {
                if (t2 % 600 == 0) { light_key_event(&lc, 49, true, t2); light_key_event(&lc, 49, false, t2 + 20); light_key_event(&lc, 70, true, t2 + 40); light_key_event(&lc, 70, false, t2 + 60); }
                light_render(&lc, t2, false);
                for (int i = 0; i < BD_KEY_LEDS; i++) { const rgb_t c = lc.key[i]; if (c.b > 10) lit_px++; if (c.r > 2 || c.g > 2) wrong++; }
            }
            char msg[80]; snprintf(msg, sizeof msg, "effect %d keeps to the custom colour (blue)", effects[q]);
            CHECK(wrong == 0 && lit_px > 0, msg);
        }
    }
    /* Ripple with a custom colour has to show its travelling dip (the real V3 dips to ~80/200 on ~19 % of key-frames), not a solid colour */
    {
        static light_t lr; light_init(&lr, &n);
        const uint8_t st[9] = {7, 100, 2, 0, 0, 0, 255, 0, 0}; memcpy(n.light[0], st, 9);
        int tot = 0, dipped = 0, deep = 0;
        for (uint32_t t2 = 0; t2 < 8000; t2 += 13) { light_render(&lr, t2, false); for (int i = 0; i < BD_KEY_LEDS; i++) { tot++; if (lr.key[i].r < 77) dipped++; if (lr.key[i].r < 52) deep++; } }
        CHECK(dipped * 100 / tot >= 12 && dipped * 100 / tot <= 28 && deep * 100 / tot >= 5, "Ripple (custom colour): the dip is deep and covers ~20 % of key-frames, like the V3");
    }
    /* direction: b3 = 1 reverses a sweeping effect */
    {
        static light_t da, db; light_init(&da, &n); light_init(&db, &n);
        uint8_t s0[9] = {6, 100, 2, 0, 0, 0, 255, 0, 0}, s1[9] = {6, 100, 2, 1, 0, 0, 255, 0, 0};
        memcpy(n.light[0], s0, 9); light_render(&da, 1500, false);
        memcpy(n.light[0], s1, 9); light_render(&db, 1500, false);
        CHECK(memcmp(da.key, db.key, sizeof da.key) != 0, "Wave: the Direction flag (b3) changes the sweep");
    }
    /* brightness: 100 % tops out at 43 % of the old output, 1 % steps are fine, and the live view reports full scale */
    {
        static light_t lb; light_init(&lb, &n);
        uint8_t st[9] = {3, 100, 2, 0, 0, 0, 255, 255, 255};   /* Static, custom white */
        memcpy(n.light[0], st, 9); light_render(&lb, 100, false);
        const int top = lb.key[3].r;
        CHECK(top >= 84 && top <= 87, "100 % = 43 % of the old maximum (200 -> 86)");
        int distinct = 0, prev = -1;
        for (int pct = 1; pct <= 100; pct++) { st[1] = (uint8_t)pct; memcpy(n.light[0], st, 9); light_render(&lb, 100, false); if (lb.key[3].r != prev) { distinct++; prev = lb.key[3].r; } }
        CHECK(distinct >= 80, "the 100 brightness steps give at least 80 distinct output levels (finer control than before)");
        st[1] = 100; memcpy(n.light[0], st, 9); light_render(&lb, 100, false); uint8_t sn[312]; light_v3_snapshot(&lb, sn);
        CHECK(sn[0] >= 195 && sn[0] <= 205, "the live LED view reports a board at its new maximum as 100 % (~200)");
    }
    /* Windmill: white background, blades of the stored colour (here pure red): R stays at the brightness cap, G/B swing between ~0 and the cap */
    n.light[0][0] = 19; n.light[0][1] = 100; n.light[0][4] = 0; n.light[0][6] = 255; n.light[0][7] = 0; n.light[0][8] = 0; l.bat_on = false; l.test = 0;
    int gmin = 255, gmax = 0, rmin = 255;
    for (uint32_t t2 = 0; t2 < 2000; t2 += 20) { light_render(&l, t2, false); for (int i = 0; i < 84; i++) { if (l.key[i].g < gmin) gmin = l.key[i].g; if (l.key[i].g > gmax) gmax = l.key[i].g; if (l.key[i].r < rmin) rmin = l.key[i].r; } }
    CHECK(rmin >= 82 && gmax >= 82 && gmin <= 13, "Windmill: red blades over a white background (R fixed, G/B sweep from ~0 to white)");
    l.bat_on = false; l.test = 2; light_render(&l, 6000, false);
    CHECK(l.key[10].g == 255 && l.key[10].r == 0 && l.side[7].g == 255, "RGB test paints every LED the test colour");
    l.test = 0;
    printf(fails ? "\nLIGHT: %d FAILED\n" : "\nLIGHT: ALL PASS\n", fails);
    return fails != 0;
}
