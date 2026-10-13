/* Host test for the Air96 V2 board data: IS31FL3733 wiring, numpad extension slots and their keymap window (built with -Isrc/boards/air96v2). */
#include <stdio.h>
#include <string.h>
#include "../src/core/keyeng.h"
#include "../src/core/light.h"
#include "board_data.h"

static int fails;
#define CHECK(c, msg) do { if (!(c)) { fails++; printf("FAIL %s\n", msg); } else printf("OK   %s\n", msg); } while (0)

static const uint8_t K = 0x5A;
static void frame(uint8_t f[NPH_FRAME], uint8_t cmd, uint16_t addr, uint8_t len, const uint8_t *data) {
    memset(f, 0, NPH_FRAME);
    f[0] = 0x55; f[1] = cmd; f[4] = len ^ K; f[5] = (addr & 0xFF) ^ K; f[6] = (addr >> 8) ^ K; f[7] = K;
    for (uint8_t i = 0; i < len; i++) f[NPH_DATA_OFF + i] = data[i] ^ K;
    f[3] = nph_checksum(f);
}
static bool has_default(uint16_t usage) {            /* nuphy.io finds a key's slot by its usage in the factory defaults (command B1) */
    for (int sl = 0; sl < NPH_LAYERS * NPH_SLOTS; sl++) if (BD_DEFAULT_KEYMAP[sl * 2] == (usage >> 8) && BD_DEFAULT_KEYMAP[sl * 2 + 1] == (usage & 0xFF)) return true;
    return false;
}

int main(void) {
#ifndef BD_AIR96V2
    printf("not an Air96 build\n"); return 1;
#endif
    static uint8_t used[2][192]; int clash = 0, range = 0;
    for (int i = 0; i < BD_IS31_N; i++) {
        const uint8_t reg[3] = {BD_IS31_R[i], BD_IS31_G[i], BD_IS31_B[i]};
        for (int k = 0; k < 3; k++) { if (reg[k] >= 192 || BD_IS31_CHIP[i] > 1) range++; else if (used[BD_IS31_CHIP[i]][reg[k]]++) clash++; }
    }
    CHECK(BD_IS31_N == 110 && range == 0 && clash == 0, "110 LEDs (100 keys + 2 x 5 side) map to 330 distinct PWM registers on 2 chips");

    uint8_t slots[NPH_SLOTS] = {0}; int dup = 0, keys = 0, wrong = 0;
    for (int r = 0; r < BD_ROWS; r++) for (int c = 0; c < BD_COLS; c++) if (BD_SLOT[r][c] != 0xFF) { keys++; if (slots[BD_SLOT[r][c]]++) dup++; if (BD_SLOT[r][c] != r * 21 + c) wrong++; }
    CHECK(BD_COLS == 21 && NPH_SLOTS == 128 && keys == 100 && dup == 0 && wrong == 0, "100 keys on 100 distinct slots = row * 21 + column; 128 slots per layer");
    int led_ok = 1; for (int i = 0; i < BD_KEY_LEDS; i++) if (!slots[BD_LED_SLOT[i]]) led_ok = 0;
    CHECK(led_ok, "every key LED belongs to a key");
    int ok = 1;                                       /* every key in nuphy.io's Air100 V3 table must be findable in the factory defaults */
    static const uint16_t need[] = {0x29, 0x3A, 0x45, 0x4A, 0x4B, 0x4D, 0x4E, 0x35, 0x2A, 0x53, 0x54, 0x55, 0x56, 0x5F, 0x60, 0x61, 0x57, 0x5C, 0x5D, 0x5E,
                                    0x59, 0x5A, 0x5B, 0x58, 0x62, 0x63, 0x2B, 0x39, 0x28, 0xE1, 0xE5, 0x52, 0xE0, 0xE2, 0xE3, 0x2C, 0x50, 0x51, 0x4F, 0x4C, 0x14};
    for (unsigned i = 0; i < sizeof need / sizeof need[0]; i++) if (!has_default(need[i])) { ok = 0; printf("     missing default %02x\n", need[i]); }
    CHECK(ok, "factory defaults contain Esc, F-row, nav keys, the whole numpad, Del (the corner key) and the main block");
    CHECK(BD_DEFAULT_KEYMAP[(2 * 21 + 14) * 2 + 1] == 0x4C && BD_BUTTON_SLOT == 56, "the corner key (slot 56) defaults to Del");

    static nph_t n; static keyeng_t k; ke_report_t r;
    nph_init(&n, NULL); keyeng_init(&k, &n);
    keyeng_event(&k, 2 * 21 + 17, true, 0); keyeng_report(&k, &r);       /* numpad 7 is matrix (2, 17) */
    CHECK(r.keys[0] == 0x5F, "numpad 7 reports keypad 7 (0x5F)");
    keyeng_event(&k, 2 * 21 + 17, false, 5); keyeng_event(&k, 4 * 21 + 20, true, 6); keyeng_report(&k, &r);
    CHECK(r.keys[0] == 0x58, "numpad Enter reports keypad Enter (0x58)");
    keyeng_release_all(&k);

    uint8_t f[NPH_FRAME], o[NPH_FRAME], hs[NPH_FRAME] = {0};
    hs[0] = 0x55; hs[1] = NPH_SET_SECRET; hs[NPH_DATA_OFF + 20] = K; hs[3] = nph_checksum(hs);
    nph_handle(&n, hs, o);
    frame(f, NPH_GET_BASE, 0, 8, o); nph_handle(&n, f, o);
    CHECK((o[NPH_DATA_OFF + 3] ^ K) == 6 && (o[NPH_DATA_OFF + 4] ^ K) == 21, "A0 reports 6 rows x 21 columns, so the host reads 8 x 128 x 2 keymap bytes");
    frame(f, NPH_GET_LIGHT_COUNT, 0, 1, o); nph_handle(&n, f, o);
    CHECK((o[NPH_DATA_OFF] ^ K) == 119, "D1 reports 99 key LEDs + 20 side LEDs");
    const uint8_t v[2] = {0x00, 0x2A};
    frame(f, NPH_SET_KEYS, 2 * (2 * 21 + 17), 2, v); nph_handle(&n, f, o);
    frame(f, NPH_GET_KEYS, 2 * (2 * 21 + 17), 2, v); nph_handle(&n, f, o);
    CHECK((o[NPH_DATA_OFF + 1] ^ K) == 0x2A, "B3 / B2 address slots as row * 21 + column");
    frame(f, NPH_GET_KEYS, (4 * NPH_SLOTS + NPH_SLOTS - 1) * 2, 2, v); nph_handle(&n, f, o);
    CHECK((o[NPH_DATA_OFF + 1] ^ K) == 0xA9, "the last slot of a layer is readable (hidden volume-up entry for the Air100 V3 knob view)");

    /* lighting: numpad LEDs follow an effect like their neighbours; side strips fill; the V3 view skips the numpad */
    static light_t l; light_init(&l, &n);
    light_render(&l, 1000, false);
    int lit = 0; for (int i = 0; i < BD_KEY_LEDS; i++) lit += (l.key[i].r | l.key[i].g | l.key[i].b) != 0;
    CHECK(lit > 90, "the default effect lights (almost) every key including the numpad");
    uint8_t snap[NPH_LED_COUNT_V3 * 3]; light_v3_snapshot(&l, snap);
    int keylit = 0; for (int i = 0; i < 99; i++) keylit += (snap[i * 3] | snap[i * 3 + 1] | snap[i * 3 + 2]) != 0;
    int sidelit = 0; for (int i = 99; i < 119; i++) sidelit += (snap[i * 3] | snap[i * 3 + 1] | snap[i * 3 + 2]) != 0;
    CHECK(keylit > 90 && sidelit == 20, "live LED list: 99 key LEDs (98 keys + Del) then the 2 x 5 side LEDs sampled into 2 x 10");
    printf(fails ? "\nAIR96: %d FAILED\n" : "\nAIR96: ALL PASS\n", fails);
    return fails != 0;
}
