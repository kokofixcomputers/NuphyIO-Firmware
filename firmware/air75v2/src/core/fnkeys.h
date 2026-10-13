/* On-keyboard FN functions that only change settings (lighting hotkeys, sleep toggle, indicators). Portable and host-tested;
 * everything they touch is persisted through nph_hooks_t.mark. The V3 keymap entries they answer to were read off the V3's own FN layers:
 *   0x7827/0x7828 back-light brightness +/-     0x7821 next effect      0x7823 hue +      0x7829/0x782A speed +/-
 *   0x7E0D/0x7E0E side brightness +/-           0x7E0F next effect      0x7E10 hue +      0x7E11/0x7E12 speed +/-     0x7E3C side light on/off
 *   0x7E14 auto-sleep on/off                    0x7E15 battery on the left side light     0x7E3B caps-lock indicator on/off */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "nph_proto.h"

/* returns true if the entry was a settings function (and was applied) */
bool fn_apply(nph_t *n, uint8_t profile, uint16_t entry);
