/* Host build of the firmware's protocol core, for replaying nuphy.io against it (tools/harness/server.py). */
#include <string.h>
#include "nph_proto.h"
static nph_t dev;
static void hook_led(uint16_t off, uint8_t *dst, uint8_t len) { (void)off; memset(dst, 0x20, len); }
static uint8_t hook_mode(void) { return 0; }
void sim_init(void) { nph_hooks_t h = {0}; h.led_read = hook_led; h.current_mode = hook_mode; nph_init(&dev, &h); }
int sim_frame(const uint8_t *in, uint8_t *out) { return nph_handle(&dev, in, out); }

void sim_poke_keymap(int layer, int slot, int entry) { dev.keymap[(layer * NPH_SLOTS + slot) * 2] = (uint8_t)(entry >> 8); dev.keymap[(layer * NPH_SLOTS + slot) * 2 + 1] = (uint8_t)entry; }
void sim_poke_appdef(int off, int v) { dev.appdef[off] = (uint8_t)v; }
void sim_poke_kbfunc(int off, int v) { dev.kbfunc[off] = (uint8_t)v; }

extern uint8_t sim_defkeys[];
void sim_poke_default(int layer, int slot, int entry) { sim_defkeys[(layer * NPH_SLOTS + slot) * 2] = (uint8_t)(entry >> 8); sim_defkeys[(layer * NPH_SLOTS + slot) * 2 + 1] = (uint8_t)entry; }
