/* Host test for the Halo75 V2 board data and its halo-ring lighting (built with -Isrc/boards/halo75v2). */
#include <stdio.h>
#include <string.h>
#include "../src/core/light.h"
#include "board_data.h"

static int fails;
#define CHECK(c, msg) do { if (!(c)) { fails++; printf("FAIL %s\n", msg); } else printf("OK   %s\n", msg); } while (0)

int main(void) {
#ifndef BD_HALO75V2
    printf("not a Halo build\n"); return 1;
#endif
    /* IS31FL3733 wiring: every dot is used by exactly one LED colour channel, inside the 12 x 16 register space */
    static uint8_t used[2][192]; int clash = 0, range = 0;
    for (int i = 0; i < 128; i++) {
        const uint8_t reg[3] = {BD_IS31_R[i], BD_IS31_G[i], BD_IS31_B[i]};
        for (int k = 0; k < 3; k++) { if (reg[k] >= 192 || BD_IS31_CHIP[i] > 1) range++; else if (used[BD_IS31_CHIP[i]][reg[k]]++) clash++; }
    }
    CHECK(range == 0 && clash == 0, "128 LEDs map to 384 distinct PWM registers on 2 chips");
    int n = 0; for (int c = 0; c < 2; c++) for (int r = 0; r < 192; r++) n += used[c][r];
    CHECK(n == 384, "all 384 channels are wired");

    uint8_t seen[45] = {0}; int perm = 1;
    for (int p = 0; p < 45; p++) { if (BD_RING[p] >= 45 || seen[BD_RING[p]]++) perm = 0; }
    CHECK(perm, "the halo ring order is a permutation of the 45 halo LEDs");

    /* keys: unique V3 slots, the Del key is the top-right button (slot 60), no key on slot 15 / 92 */
    uint8_t slots[98] = {0}; int dup = 0, keys = 0;
    for (int r = 0; r < BD_ROWS; r++) for (int c = 0; c < BD_COLS; c++) if (BD_SLOT[r][c] != 0xFF) { keys++; if (slots[BD_SLOT[r][c]]++) dup++; }
    CHECK(keys == 83 && dup == 0, "83 keys on 83 distinct V3 slots");
    CHECK(slots[60] == 1 && slots[15] == 0 && slots[92] == 0 && slots[31] == 1 && slots[79] == 1, "Del = slot 60; the right column keeps slots 31/47/63/79; no right Ctrl");
    int led_ok = 1; for (int i = 0; i < BD_KEY_LEDS; i++) if (!slots[BD_LED_SLOT[i]]) led_ok = 0;
    CHECK(led_ok, "every key LED belongs to a key");
    CHECK(BD_DEFAULT_KEYMAP[(0 * 98 + 31) * 2 + 1] == 0x4A && BD_DEFAULT_KEYMAP[(0 * 98 + 63) * 2 + 1] == 0x4B, "top-right column defaults: Home on slot 31, PgUp on slot 63");

    /* lighting on the ring */
    static nph_t d; static light_t l; nph_init(&d, NULL); light_init(&l, &d);
    d.light[0][9] = 2; d.light[0][9 + 1] = 100; d.light[0][9 + 3] = 0; d.light[0][14] = 0; d.light[0][15] = 0; d.light[0][16] = 255;      /* static blue side light */
    light_render(&l, 1000, false);
    int lit = 0; for (int i = 0; i < BD_SIDE_LEDS; i++) lit += l.side[i].b > 0;
    CHECK(lit == 45, "the side-light state paints the whole 45-LED ring");
    rgb_t before[45]; memcpy(before, l.side, sizeof before);
    l.bat_on = true; l.bat_pct = 90; l.bat_charging = false; light_render(&l, 1000, false);
    int changed = 0, in_bar = 1;
    for (int i = 0; i < 45; i++) if (memcmp(&before[i], &l.side[i], sizeof(rgb_t))) { changed++; if (i < BD_BAT_FIRST || i >= BD_BAT_FIRST + BD_BAT_COUNT) in_bar = 0; }
    CHECK(in_bar && changed > 0 && l.side[BD_BAT_FIRST].g > 0 && l.side[BD_BAT_FIRST + 4].g > 0, "battery: five green LEDs in the five-LED segment of the ring, the other 40 untouched");
    l.bat_charging = true; light_render(&l, 1200, false);
    CHECK(l.side[BD_BAT_FIRST].g > 0 && l.side[BD_BAT_FIRST + 4].g > 0, "a full bar breathes while charging");
    l.bat_on = false;
    uint8_t snap[NPH_LED_COUNT_V3 * 3]; light_render(&l, 2000, false); light_v3_snapshot(&l, snap);
    int sidelit = 0; for (int i = 84; i < 104; i++) sidelit += snap[i * 3 + 2] > 0;
    CHECK(sidelit == 20, "V3 snapshot: the ring is sampled into the V3's 2 x 10 side LEDs");
    d.light[0][0] = 3; d.light[0][4] = 0; d.light[0][1] = 100; d.light[0][6] = 255; d.light[0][7] = 255; d.light[0][8] = 255; light_render(&l, 3000, false); light_v3_snapshot(&l, snap);
    CHECK(snap[83 * 3] > 0, "V3 snapshot: the Del button's LED is reported where the V3 has its knob LED (index 83)");
    printf(fails ? "\nHALO: %d FAILED\n" : "\nHALO: ALL PASS\n", fails);
    return fails != 0;
}
