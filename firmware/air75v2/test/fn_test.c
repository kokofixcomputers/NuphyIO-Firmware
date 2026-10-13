#include <stdio.h>
#include <string.h>
#include "../src/core/fnkeys.h"
static int fails, marks;
#define CHECK(c, m) do { if (!(c)) { fails++; printf("FAIL %s\n", m); } else printf("OK   %s\n", m); } while (0)
static void mark(uint8_t seg, uint16_t off, uint16_t len) { (void)seg; (void)off; (void)len; marks++; }
int main(void) {
    static nph_t n; nph_hooks_t h = {0}; h.mark = mark; nph_init(&n, &h);
    uint8_t *L = n.light[0];
    L[1] = 50; fn_apply(&n, 0, 0x7827); CHECK(L[1] == 60, "FN+Up: back-light brightness +10 %");
    fn_apply(&n, 0, 0x7828); fn_apply(&n, 0, 0x7828); CHECK(L[1] == 40, "FN+Down: brightness -10 % twice");
    L[1] = 95; fn_apply(&n, 0, 0x7827); fn_apply(&n, 0, 0x7827); CHECK(L[1] == 100, "brightness clamps at 100 %");
    L[1] = 5; fn_apply(&n, 0, 0x7828); CHECK(L[1] == 0, "brightness clamps at 0 (off)");
    L[0] = 20; fn_apply(&n, 0, 0x7821); CHECK(L[0] == 1, "FN+Left: effect 20 wraps to 1");
    fn_apply(&n, 0, 0x7821); CHECK(L[0] == 2, "next effect");
    L[2] = 2; fn_apply(&n, 0, 0x7829); CHECK(L[2] == 1, "speed up lowers the raw value (gear = 4 - raw)");
    L[2] = 0; fn_apply(&n, 0, 0x7829); CHECK(L[2] == 0, "speed saturates");
    L[2] = 4; fn_apply(&n, 0, 0x782A); CHECK(L[2] == 4, "slowest stays slowest");
    L[4] = 1; L[6] = 255; L[7] = 0; L[8] = 0; fn_apply(&n, 0, 0x7823);
    CHECK(L[4] == 0 && L[7] > 0 && L[8] == 0, "FN+Right: switches to the custom colour and rotates its hue");
    for (int i = 0; i < 23; i++) fn_apply(&n, 0, 0x7823);
    CHECK(L[6] >= 245 && L[7] <= 20 && L[8] <= 20, "24 steps of 15 degrees come back to (about) red");

    uint8_t *S = L + 9;
    S[1] = 80; fn_apply(&n, 0, 0x7E0E); CHECK(S[1] == 70, "side brightness -10 %");
    S[0] = 4; fn_apply(&n, 0, 0x7E0F); CHECK(S[0] == 0, "side effect wraps 4 -> 0");
    S[1] = 70; fn_apply(&n, 0, 0x7E3C); CHECK(S[1] == 0, "side light off");
    fn_apply(&n, 0, 0x7E3C); CHECK(S[1] == 70, "side light back on at the old level");
    fn_apply(&n, 1, 0x7E0D); CHECK(n.light[1][9 + 1] == 0x60 || n.light[1][9 + 1] > 0, "the other profile is addressed separately");

    n.sleepcfg[0] = 1; fn_apply(&n, 0, 0x7E14); CHECK(n.sleepcfg[0] == 0, "auto sleep toggles off");
    fn_apply(&n, 0, 0x7E14); CHECK(n.sleepcfg[0] == 1, "... and on");
    fn_apply(&n, 0, 0x7E15); CHECK(n.fwcfg[NPH_FW_BAT_STRIP] == 1, "FN+\\ turns the battery strip on");
    fn_apply(&n, 0, 0x7E15); CHECK(n.fwcfg[NPH_FW_BAT_STRIP] == 0, "... and off");
    fn_apply(&n, 0, 0x7E3B); CHECK(n.fwcfg[NPH_FW_CAPS_OFF] == 1, "caps indicator toggles");
    CHECK(!fn_apply(&n, 0, 0x7E02) && !fn_apply(&n, 0, 0x0004), "link keys and plain keys are not settings functions");
    CHECK(marks > 20, "every change is marked for the flash store");
    printf(fails ? "\nFN: %d FAILED\n" : "\nFN: ALL PASS\n", fails);
    return fails != 0;
}
