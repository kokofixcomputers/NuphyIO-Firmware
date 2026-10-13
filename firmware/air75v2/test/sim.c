/* Native simulator: reads 64-byte frames from stdin, writes 64-byte responses to stdout (for protocol tests). */
#include <stdio.h>
#include <string.h>
#include "../src/core/nph_proto.h"

static uint8_t fake_led(void) { return 0; }
static void led_read(uint16_t off, uint8_t *dst, uint8_t len) { for (uint8_t i = 0; i < len; i++) dst[i] = (uint8_t)((off + i) * 3); }
static uint8_t mode_fn(void) { return 1; }

int main(void) {
    (void)fake_led;
    static nph_t dev;
    nph_hooks_t h = { .led_read = led_read, .current_mode = mode_fn };
    nph_init(&dev, &h);
    uint8_t in[NPH_FRAME], out[NPH_FRAME];
    while (fread(in, 1, NPH_FRAME, stdin) == NPH_FRAME) {
        if (nph_handle(&dev, in, out)) { fwrite(out, 1, NPH_FRAME, stdout); }
        else { memset(out, 0, NPH_FRAME); fwrite(out, 1, NPH_FRAME, stdout); }   /* all-zero = "no response" marker */
        fflush(stdout);
    }
    return 0;
}
