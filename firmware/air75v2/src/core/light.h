/* Lighting engine: replays the Air75 V3's effects (tables compiled from recordings of a real V3) on the V2's LEDs. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "board_cfg.h"
#include "nph_proto.h"

typedef struct { uint8_t r, g, b; } rgb_t;

#define LT_MAX_PRESS 6

typedef struct {
    nph_t   *nph;
    uint8_t  mode;                 /* active profile (OS switch) */
    uint8_t  heat[BD_KEY_LEDS];             /* per-LED pulse (Point / Ripple / Grid / Rain), 255 = just pressed */
    uint8_t  held[BD_KEY_LEDS];             /* key currently down */
    struct { uint8_t led; uint32_t t; } press[LT_MAX_PRESS];
    uint8_t  press_n;
    uint32_t seed;
    uint32_t last_ms;
    /* random effects (Flower, Ribbon) */
    uint8_t  flower_v[BD_KEY_LEDS], flower_h[BD_KEY_LEDS]; uint32_t flower_t[BD_KEY_LEDS]; uint32_t ribbon_step;
    rgb_t    key[BD_KEY_LEDS];              /* LED chain (wiring) order */
    rgb_t    side[BD_SIDE_LEDS];   /* Air75 V2: 6 per strip (left then right); Halo75 V2: the 45-LED ring in order round the board */
    /* overlays set by the board each frame */
    bool     bat_on;               /* left side light shows the battery; the right one keeps following the software-controlled state */
    uint8_t  bat_pct;
    bool     bat_charging;
    uint8_t  test;                 /* RGB test (FN+C held): 0 off, 1..4 = red, green, blue, white on every LED */
} light_t;

void light_init(light_t *l, nph_t *n);
void light_key_event(light_t *l, uint8_t slot, bool pressed, uint32_t now_ms);
void light_render(light_t *l, uint32_t now_ms, bool caps_on);
/* 312 bytes in the order the V3 app expects: 83 key LEDs ascending by slot, the knob LED (dark), then 2 x 10 side LEDs */
void light_v3_snapshot(const light_t *l, uint8_t out[NPH_LED_COUNT_V3 * 3]);
