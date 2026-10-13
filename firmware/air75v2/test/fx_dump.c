/* usage: fx_dump <effect> <gear> <auto 1|0> <r> <g> <b> <brightness> < times_ms   -> 312 bytes per time (V3 snapshot order) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/core/light.h"
int main(int argc, char **argv) {
    (void)argc;
    static nph_t n; static light_t l;
    nph_init(&n, NULL); light_init(&l, &n);
    uint8_t *b = n.light[0];
    b[0] = (uint8_t)atoi(argv[1]); b[2] = (uint8_t)(4 - atoi(argv[2])); b[4] = (uint8_t)atoi(argv[3]); b[6] = (uint8_t)atoi(argv[4]); b[7] = (uint8_t)atoi(argv[5]); b[8] = (uint8_t)atoi(argv[6]); b[1] = (uint8_t)atoi(argv[7]);
    b[10] = 0;                                              /* side off */
    unsigned long t; uint8_t snap[NPH_LED_COUNT_V3 * 3];
    while (scanf("%lu", &t) == 1) { light_render(&l, (uint32_t)t, false); light_v3_snapshot(&l, snap); fwrite(snap, 1, sizeof snap, stdout); }
    return 0;
}
