/* Host test for the key engine. */
#include <stdio.h>
#include <string.h>
#include "../src/core/keyeng.h"

static int fails;
#define CHECK(c, msg) do { if (!(c)) { fails++; printf("FAIL %s\n", msg); } else printf("OK   %s\n", msg); } while (0)
static void set(nph_t *n, int layer, int slot, uint16_t e) { n->keymap[(layer * NPH_SLOTS + slot) * 2] = e >> 8; n->keymap[(layer * NPH_SLOTS + slot) * 2 + 1] = e & 0xFF; }

int main(void) {
    static nph_t n; static keyeng_t k; ke_report_t r;
    nph_init(&n, NULL); keyeng_init(&k, &n);

    keyeng_event(&k, 33, true, 0);                       /* Q */
    CHECK(keyeng_report(&k, &r) && r.keys[0] == 0x14, "plain key press reports usage 0x14");
    keyeng_event(&k, 33, false, 10);
    CHECK(keyeng_report(&k, &r) && r.keys[0] == 0, "release clears the key");

    keyeng_event(&k, 48 /*caps*/, true, 0); keyeng_event(&k, 80 /*ctrl*/, true, 1);
    keyeng_report(&k, &r);
    CHECK(r.mods == 0x01 && r.keys[0] == 0x39, "modifier + key in one report");
    keyeng_release_all(&k);

    /* MO(1) is at slot 91 on layer 0 (Fn); on layer 1 the F row becomes F1..F12 -> hold Fn and press slot 1 */
    keyeng_event(&k, 91, true, 0); keyeng_event(&k, 1, true, 1); keyeng_report(&k, &r);
    CHECK(r.keys[0] == 0x3A, "MO(1) layer exposes F1 on the F-row key");
    keyeng_event(&k, 1, false, 5); keyeng_event(&k, 91, false, 6); keyeng_report(&k, &r);
    keyeng_event(&k, 7, true, 10); keyeng_report(&k, &r);
    CHECK(r.keys[0] == 0x00 && r.consumer == 0x00B6, "without Fn, F-row slot 7 sends media (Prev on the Mac layer)");
    keyeng_release_all(&k);

    keyeng_set_mode(&k, 1);                              /* Windows */
    keyeng_event(&k, 1, true, 0); keyeng_report(&k, &r);
    CHECK(r.keys[0] == 0x3A, "Windows base layer: F1 straight away");
    keyeng_release_all(&k); keyeng_set_mode(&k, 0);

    /* knob-like consumer key */
    set(&n, 0, 20, 0x00AA); keyeng_event(&k, 20, true, 0); keyeng_report(&k, &r);
    CHECK(r.consumer == 0x00EA, "0xAA -> consumer volume down");
    keyeng_release_all(&k);

    /* macro: write a 2-event macro 'a' into slot 0 and trigger it with entry 0x7700 */
    uint8_t *m = n.macro; const uint8_t ev[8] = {5, 0, 0x40, 0x04, 5, 0, 0x80, 0x04};
    memcpy(&m[0x40], ev, 8); for (int i = 0; i < 32; i++) { uint16_t v = i == 0 ? 0x40 : 0x48; m[i * 2] = v & 255; m[i * 2 + 1] = v >> 8; }
    set(&n, 0, 21, 0x7700); keyeng_event(&k, 21, true, 100); keyeng_event(&k, 21, false, 101);
    keyeng_tick(&k, 106); CHECK(keyeng_report(&k, &r) && r.keys[0] == 0x04, "macro presses 'a' after its delay");
    keyeng_tick(&k, 112); CHECK(keyeng_report(&k, &r) && r.keys[0] == 0, "macro releases 'a' and ends");
    keyeng_release_all(&k);

    /* tap dance slot 3: tap=Q hold=W double=E, entry 0x7F43 */
    const uint8_t td[8] = {0, 0x14, 0, 0x08, 0, 0x1A, 0, 170}; memcpy(&n.td[3 * 8], td, 8);
    set(&n, 0, 22, 0x7F43);
    keyeng_event(&k, 22, true, 0); keyeng_event(&k, 22, false, 50); keyeng_tick(&k, 300); keyeng_report(&k, &r);
    CHECK(r.keys[0] == 0x14, "tap dance: single tap -> Q after the window");
    keyeng_tick(&k, 320); keyeng_report(&k, &r); keyeng_release_all(&k);
    keyeng_event(&k, 22, true, 1000); keyeng_tick(&k, 1200); keyeng_report(&k, &r);
    CHECK(r.keys[0] == 0x1A, "tap dance: hold -> W while held");
    keyeng_event(&k, 22, false, 1300); keyeng_report(&k, &r);
    CHECK(r.keys[0] == 0, "tap dance: hold released");
    keyeng_release_all(&k);
    keyeng_event(&k, 22, true, 2000); keyeng_event(&k, 22, false, 2040); keyeng_event(&k, 22, true, 2080); keyeng_report(&k, &r);
    CHECK(r.keys[0] == 0x08, "tap dance: double tap -> E");
    keyeng_event(&k, 22, false, 2120); keyeng_release_all(&k);

    /* toggle slot 0 on Space: tap locks, tap again unlocks */
    const uint8_t tg[2] = {0x00, 0x2C}; memcpy(&n.tgl[0], tg, 2); set(&n, 0, 23, 0x7F80);
    keyeng_event(&k, 23, true, 0); keyeng_event(&k, 23, false, 40); keyeng_report(&k, &r);
    CHECK(r.keys[0] == 0x2C, "toggle: tap locks the key down");
    keyeng_event(&k, 23, true, 100); keyeng_event(&k, 23, false, 130); keyeng_report(&k, &r);
    CHECK(r.keys[0] == 0, "toggle: second tap releases it");

    keyeng_release_all(&k);
    set(&n, 0, 20, 0x00D1); set(&n, 0, 21, 0x00D3); set(&n, 0, 22, 0x00D9); set(&n, 0, 24, 0x00DB);
    keyeng_event(&k, 20, true, 0); keyeng_event(&k, 21, true, 1); keyeng_report(&k, &r);
    CHECK(r.buttons == 0x05 && r.keys[0] == 0, "mouse: left + middle click report as buttons 1 and 3, not as keys");
    keyeng_event(&k, 20, false, 5); keyeng_event(&k, 21, false, 6); keyeng_report(&k, &r);
    CHECK(r.buttons == 0, "mouse: buttons release");
    keyeng_event(&k, 22, true, 10); keyeng_report(&k, &r);
    CHECK(r.wheel == 1 && r.hwheel == 0, "scroll up key reports a held up direction");
    keyeng_event(&k, 22, false, 20); keyeng_event(&k, 24, true, 21); keyeng_report(&k, &r);
    CHECK(r.wheel == 0 && r.hwheel == -1, "scroll left key reports a held left direction");
    keyeng_release_all(&k);

    /* the V2's top-right button is slot 60: Del by default, rebindable like any key (this is the slot nuphy.io and Air Deck write) */
    keyeng_event(&k, 60, true, 0); keyeng_report(&k, &r);
    CHECK(r.keys[0] == 0x4C, "button (slot 60) defaults to Delete");
    keyeng_event(&k, 60, false, 5); keyeng_report(&k, &r);
    set(&n, 0, 60, 0x0004);
    keyeng_event(&k, 60, true, 10); keyeng_report(&k, &r);
    CHECK(r.keys[0] == 0x04, "rebinding slot 60 to A takes effect");
    keyeng_event(&k, 60, false, 15); keyeng_report(&k, &r);
    set(&n, 0, 60, 0x00D1);
    keyeng_event(&k, 60, true, 20); keyeng_report(&k, &r);
    CHECK(r.buttons == 0x01, "slot 60 can be a mouse click");
    keyeng_release_all(&k);

    n.appdef[NPH_APPDEF_ACCESSORY] = 1;
    CHECK(n.appdef[168] == 1, "app define byte 168 = accessory (V2 default: button)");

    keyeng_release_all(&k);
    set(&n, 0, 3, 0x7E06); set(&n, 0, 4, 0x7E07); set(&n, 0, 5, 0x7E08); set(&n, 0, 6, 0x7E16); set(&n, 0, 13, 0x7E0B);
    keyeng_event(&k, 4, true, 0); keyeng_report(&k, &r);
    CHECK(r.mods == 0x08 && r.keys[0] == 0x2C, "Spotlight key sends Cmd+Space");
    keyeng_event(&k, 4, false, 5); keyeng_report(&k, &r);
    CHECK(r.mods == 0 && r.keys[0] == 0, "... and lets go on release");
    keyeng_event(&k, 3, true, 10); keyeng_report(&k, &r);
    CHECK(r.consumer == 0x029F, "Mission Control sends consumer 0x029F");
    keyeng_event(&k, 3, false, 15); keyeng_event(&k, 5, true, 16); keyeng_report(&k, &r);
    CHECK(r.consumer == 0x00CF, "dictation sends consumer 0x00CF");
    keyeng_event(&k, 5, false, 20); keyeng_event(&k, 6, true, 21); keyeng_report(&k, &r);
    CHECK(r.system == 0x9B, "Do Not Disturb sends system usage 0x9B");
    keyeng_event(&k, 6, false, 25); keyeng_event(&k, 13, true, 26); keyeng_report(&k, &r);
    CHECK(r.mods == 0x0A && r.keys[0] == 0x21, "area screenshot sends Cmd+Shift+4");
    keyeng_release_all(&k);
    keyeng_event(&k, 13, true, 30); keyeng_report(&k, &r);
    CHECK(r.mods == 0x0A && r.keys[0] == 0x21, "default slot 13 on the Mac layer = area screenshot (Cmd+Shift+4), not Print Screen");
    keyeng_event(&k, 13, false, 35); keyeng_set_mode(&k, 1);
    keyeng_event(&k, 13, true, 40); keyeng_report(&k, &r);
    CHECK(r.mods == 0x0A && r.keys[0] == 0x16, "... and Win+Shift+S on the Windows layer");
    keyeng_event(&k, 13, false, 45); keyeng_set_mode(&k, 0);
    keyeng_release_all(&k);

    /* nuphy.io Mode Settings: kbfunc = [stabilization, disable Win, disable Alt+F4, disable Alt+Tab] */
    keyeng_release_all(&k); keyeng_set_mode(&k, 1);
    set(&n, 4, 82, 0x00E3); set(&n, 4, 81, 0x00E2); set(&n, 4, 25, 0x003D); set(&n, 4, 26, 0x002B);     /* Win, Alt, F4, Tab on layer 4 (Windows) */
    n.kbfunc[1] = 1; keyeng_event(&k, 82, true, 0); keyeng_report(&k, &r);
    CHECK(r.mods == 0, "Disable Win: the Windows key sends nothing");
    keyeng_event(&k, 82, false, 5); n.kbfunc[1] = 0; keyeng_event(&k, 82, true, 6); keyeng_report(&k, &r);
    CHECK(r.mods == 0x08, "... and works again once the switch is off");
    keyeng_event(&k, 82, false, 10); keyeng_report(&k, &r); keyeng_release_all(&k);
    n.kbfunc[2] = 1; keyeng_event(&k, 81, true, 20); keyeng_event(&k, 25, true, 21); keyeng_report(&k, &r);
    CHECK(r.mods == 0x04 && r.keys[0] == 0, "Disable Alt+F4: F4 is dropped while Alt is held");
    keyeng_event(&k, 25, false, 25); keyeng_event(&k, 81, false, 26); keyeng_report(&k, &r);
    keyeng_event(&k, 25, true, 30); keyeng_report(&k, &r);
    CHECK(r.keys[0] == 0x3D, "... but F4 on its own still works");
    keyeng_event(&k, 25, false, 35); keyeng_release_all(&k); n.kbfunc[2] = 0;
    n.kbfunc[3] = 1; keyeng_event(&k, 81, true, 40); keyeng_event(&k, 26, true, 41); keyeng_report(&k, &r);
    CHECK(r.mods == 0x04 && r.keys[0] == 0, "Disable Alt+Tab: Tab is dropped while Alt is held");
    keyeng_event(&k, 26, false, 45); keyeng_event(&k, 81, false, 46); keyeng_report(&k, &r); keyeng_release_all(&k);
    n.kbfunc[3] = 0; keyeng_set_mode(&k, 0);

    printf(fails ? "\nKEYENG: %d FAILED\n" : "\nKEYENG: ALL PASS\n", fails);
    return fails != 0;
}
