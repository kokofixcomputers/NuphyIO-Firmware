#include <string.h>
#include "board.h"
#include "board_data.h"
#include "keyeng.h"
#include "leds.h"
#include "light.h"
#include "matrix.h"
#include "fnkeys.h"
#include "nph_proto.h"
#include "rf.h"
#include "power.h"
#include "rfio.h"
#include "store.h"
#include "usb.h"

static nph_t    dev;
static keyeng_t eng;
static light_t  lit;
static rf_t     rf;
static uint8_t  os_mode;                 /* 0 = Mac, 1 = Windows, from the slide switch */

/* ---- output routing: USB when the device switch says so, otherwise the nRF52 radio ---- */
static uint8_t wl_mode = LINK_BLE1;      /* last wireless channel (what the device switch returns to) */
static bool    rf_route;                 /* reports currently go to the radio */
static ke_report_t rf_prev;

static void route_report(const ke_report_t *r) {
    if (!rf_route) { usb_send(r); return; }
    if (memcmp(rf_prev.keys, r->keys, KE_MAX_KEYS) || rf_prev.mods != r->mods) {
        const uint8_t b[8] = {r->mods, 0, r->keys[0], r->keys[1], r->keys[2], r->keys[3], r->keys[4], r->keys[5]};
        rf_send_report(&rf, RFC_RPT_BYTE_KB, b, 8);
    }
    if (rf_prev.consumer != r->consumer) { const uint8_t b[2] = {(uint8_t)r->consumer, (uint8_t)(r->consumer >> 8)}; rf_send_report(&rf, RFC_RPT_CONSUME, b, 2); }
    if (rf_prev.system != r->system) { const uint8_t b[2] = {r->system, 0}; rf_send_report(&rf, RFC_RPT_SYS, b, 2); }
    rf_prev = *r;
}

/* ---- mouse buttons and scroll (held scroll keys repeat detents) ---- */
static uint8_t m_btn; static int8_t m_wheel, m_hwheel; static uint32_t m_next;
static void send_mouse(int8_t wheel, int8_t pan) {
    if (rf_route) { const uint8_t b[5] = {m_btn, 0, 0, (uint8_t)wheel, (uint8_t)pan}; rf_send_report(&rf, RFC_RPT_MS, b, 5); }
    else usb_mouse(m_btn, 0, 0, wheel, pan);
}
static void mouse_change(const ke_report_t *r, uint32_t now) {
    if (r->buttons != m_btn) { m_btn = r->buttons; send_mouse(0, 0); }
    if (r->wheel != m_wheel || r->hwheel != m_hwheel) {
        m_wheel = r->wheel; m_hwheel = r->hwheel;
        if (m_wheel || m_hwheel) { send_mouse(m_wheel, m_hwheel); m_next = now + 350; }
    }
}
static void mouse_tick(uint32_t now) {
    if ((m_wheel || m_hwheel) && now >= m_next) { send_mouse(m_wheel, m_hwheel); m_next = now + 70; }
}

/* hand the keyboard to a different transport: release everything on the one being left */
static void route_switch(bool to_rf) {
    if (to_rf == rf_route) return;
    const ke_report_t none = {0};
    route_report(&none);
    keyeng_release_all(&eng);
    rf_route = to_rf; rf_prev = none; rf_report_clear(&rf);
    route_report(&none);
}

/* FN+1/2/3 = Bluetooth 1-3, FN+4 = 2.4 GHz dongle: tap to switch, hold 0.3 s to pair a new device. FN+Up/Down = backlight brightness. */
/* Unsolicited reports to the host app, exactly as the V3 sends them (unframed, on the config channel):
 *   A2 [base layer 0|4] [mode 0|1]   the OS slide switch moved
 *   D7 + 17 bytes                    light state of the active profile (after a mode switch, and after FN lighting keys) */
static uint8_t last_layer;
static void notify_layer(uint8_t layer, uint8_t mode) { const uint8_t a2[3] = {0xA2, layer, mode}; usb_send_raw(a2, 3); last_layer = layer; }   /* the V3 reports the active layer (FN0..FN3) the same way as the mode */
static void notify_mode(uint8_t mode) { notify_layer(mode ? 4 : 0, mode); }
static void notify_light(uint8_t mode) { uint8_t d7[1 + NPH_LIGHT_BYTES]; d7[0] = 0xD7; memcpy(&d7[1], dev.light[mode & 1], NPH_LIGHT_BYTES); usb_send_raw(d7, sizeof d7); }

static uint32_t fn_reset_t, fn_test_t;
static uint8_t  fn_link = 0xFF;
static uint32_t fn_t;
static bool     fn_paired;

static void on_fn(void *ctx, uint16_t e, bool pressed) {
    (void)ctx;
    uint8_t mode = 0xFF;
    switch (e) { case 0x7E02: mode = LINK_RF24; break; case 0x7E03: mode = LINK_BLE1; break; case 0x7E04: mode = LINK_BLE2; break; case 0x7E05: mode = LINK_BLE3; break; default: break; }
    if (mode != 0xFF) {
        if (board_device_switch_is_usb()) return;                  /* the slide switch must be on a wireless position */
        if (pressed) { fn_link = mode; fn_t = millis(); fn_paired = false; }
        else if (fn_link == mode) { if (!fn_paired) { wl_mode = mode; rf_set_link(&rf, mode); } fn_link = 0xFF; }
        return;
    }
    if (e == 0x7E13) { fn_reset_t = pressed ? millis() : 0; return; }                 /* FN+[ held 3 s: factory reset (like DEV_RESET) */
    if (e == 0x7E19) { fn_test_t = pressed ? millis() : 0; lit.test = pressed ? 1 : 0; return; }   /* FN+C held: RGB test */
    if (pressed && fn_apply(&dev, lit.mode & 1, e)) notify_light(lit.mode);           /* brightness / effect / speed / hue / side light / sleep / battery / caps */
}

/* blink the channel's key (1/2/3/4) while the radio is not connected: fast = pairing, slow = linking */
static void rf_indicator(uint32_t now) {
    static uint32_t t_conn; static uint8_t last_state = 0xFF;
    if (rf.link_mode == LINK_USB) return;
    if (rf.state != last_state) { last_state = rf.state; if (rf.state == RF_CONNECT) t_conn = now; }
    bool on;
    if (rf.state == RF_CONNECT) on = now - t_conn < 2000;
    else if (rf.state == RF_PAIRING) on = (now / 250) & 1;
    else on = (now / 500) & 1;
    if (!on) return;
    const uint8_t slot = rf.link_mode == LINK_RF24 ? 20 : (uint8_t)(16 + rf.link_mode);
    const rgb_t col = rf.link_mode == LINK_RF24 ? (rgb_t){0, 200, 0} : (rgb_t){0, 60, 255};
    for (int i = 0; i < BD_KEY_LEDS; i++) if (BD_LED_V3[i] == slot) lit.key[i] = col;
}

/* Auto sleep (F3/F5 config [enabled, level-1 min, level-2 min, -]): level 1 turns the lighting off and cuts its supply (boost converter and driver
 * enables) and polls the radio less often. Level 2 additionally tells the radio to sleep and stops polling it, and - on battery with no USB host - puts the STM32 into STOP mode (power.c), which
 * wakes on a key or every STOP_WAKE_MS to look for keys on shared EXTI lines. Any key press wakes the board (and is still delivered to the host). */
#define STOP_WAKE_MS 50
/* Wireless link lost: sleep after 10 s without a connection (stock: 5 s), or after 2 minutes while pairing (FN+1/2/3 held). Only with auto-sleep enabled. */
#define DISCONNECT_SLEEP_MS 10000u
#define PAIRING_SLEEP_MS    120000u
static uint32_t bad_t;                  /* when the current not-connected stretch started */
static bool     bad;
static uint8_t  bad_state;
static uint32_t last_activity;
static uint8_t  sleep_level;             /* 0 awake, 1 lights off, 2 LED rails off */

static void sleep_poll(uint32_t now) {
    const uint8_t *c = dev.sleepcfg;
    uint8_t want = 0;
    static uint8_t suspended_polls;
    if (c[0]) {
        const uint32_t idle = now - last_activity;
        if (c[1] && idle >= (uint32_t)c[1] * 60000u) want = 1;
        if (c[2] && c[2] >= c[1] && idle >= (uint32_t)c[2] * 60000u) want = 2;
        if (usb_suspended()) { if (suspended_polls < 2) suspended_polls++; } else suspended_polls = 0;
        if (suspended_polls >= 2) want = 2;                          /* the host went to sleep (USB suspend for a second): sleep with it, like the stock firmware */
        if (rf.link_mode == LINK_USB || !rf.ready || rf.state == RF_CONNECT) bad = false;
        else {
            if (!bad || (bad_state == RF_PAIRING) != (rf.state == RF_PAIRING)) { bad = true; bad_t = now; }       /* a new stretch, or pairing started / ended: restart the clock */
            bad_state = rf.state;
            if (now - bad_t >= (rf.state == RF_PAIRING ? PAIRING_SLEEP_MS : DISCONNECT_SLEEP_MS)) want = 2;
        }
    }
    if (want == sleep_level) return;
    const uint8_t prev = sleep_level;
    sleep_level = want;
    leds_power(want == 0);                                              /* any sleep level: boost converter and LED drivers off */
    rf_set_poll(&rf, want == 1 ? RF_SYNC_MS_DIM : RF_SYNC_MS);          /* level 1: poll the radio every 500 ms instead of 200 */
    if (want == 2) rf_sleep(&rf); else if (prev == 2) rf_wake(&rf);
}

static void wake_up(void) {
    if (!sleep_level) return;
    sleep_level = 0; leds_power(true); rf_set_poll(&rf, 0); rf_wake(&rf); last_activity = millis(); bad = false;
}

static uint32_t ku_t;
static void on_key(uint8_t slot, bool pressed) {
    last_activity = millis();
    if (dev.keyupload) {                                   /* macro recording: report to the app, do not type into the OS */
        const uint8_t f[4] = {0xC1, (uint8_t)(slot / NPH_SLOT_COLS), (uint8_t)(slot % NPH_SLOT_COLS), pressed};
        usb_send_raw(f, 4); ku_t = millis();
        light_key_event(&lit, slot, pressed, millis());
        return;
    }
    if (usb_suspended()) usb_remote_wakeup();                /* a key press wakes a sleeping host (when it allows that) */
    wake_up();
    keyeng_event(&eng, slot, pressed, millis());
    light_key_event(&lit, slot, pressed, millis());
}

/* ---- hooks the protocol core calls ---- */
static void hook_led_read(uint16_t off, uint8_t *dst, uint8_t len) {
    static uint8_t snap[NPH_LED_COUNT_V3 * 3];
    light_v3_snapshot(&lit, snap);
    for (uint8_t i = 0; i < len; i++) dst[i] = (off + i) < sizeof snap ? snap[off + i] : 0;
}
static uint8_t hook_mode(void) { return os_mode; }
/* Restore-factory (command F1 from nuphy.io's reset, or FN+[ held): every setting goes back to its default in one go. The V3 then sends the unsolicited
 * report F2 ("keyboard reset") shortly after its acknowledgement, which makes the site reload and re-read everything. */
static uint32_t reset_event_t;
static void hook_factory_reset(void) { nph_load_defaults(&dev); dev.dirty = 0xFF; store_mark_all(); keyeng_release_all(&eng); reset_event_t = millis() + 200; if (!reset_event_t) reset_event_t = 1; }

static bool any_key_down(void) {
    for (int r = 0; r < BD_ROWS; r++) for (int c = 0; c < BD_COLS; c++) if (BD_SLOT[r][c] != 0xFF && matrix_key_down((uint8_t)r, (uint8_t)c)) return true;
    return false;
}

/* May the MCU stop its clocks now? Only on a wireless link with no USB host, the radio asleep and nothing in flight. */
static bool stop_ok(void) {
    return sleep_level == 2 && rf.link_mode != LINK_USB && !usb_mounted() && (rf.asleep || !rf.ready) && !dev.keyupload && !eng.mac_active
        && !store_dirty() && fn_link == 0xFF && !any_key_down();
}

int main(void) {
    board_init();
    matrix_init();
    delay_us(1000);

    /* Recovery: hold Esc while plugging in to drop into the ROM bootloader (the hardware BOOT0 button also works). */
    for (int i = 0; i < 20; i++) { matrix_task(NULL); delay_us(1000); }
    if (matrix_key_down(0, 0)) board_enter_dfu();

    nph_hooks_t hooks = { .led_read = hook_led_read, .enter_dfu = board_enter_dfu, .factory_reset = hook_factory_reset, .current_mode = hook_mode, .mark = store_mark, .status = store_status };
    nph_init(&dev, &hooks);
    store_init(&dev);
    store_load(&dev);
    if (dev.fwcfg[NPH_FW_BRIGHT_REV] != 1) {   /* the LEDs now top out at 43 % of their old output: what used to be 43 % is the new 100 % */
        for (int p = 0; p < 2; p++) for (int off = 1; off <= 10; off += 9) {                     /* back-light and side-light brightness bytes */
            uint8_t *b = &dev.light[p][off]; const uint32_t v = (uint32_t)(*b > 100 ? 100 : *b) * 100u / 43u; *b = (uint8_t)(v > 100 ? 100 : v);
        }
        dev.fwcfg[NPH_FW_BRIGHT_REV] = 1;
        store_mark(NPH_R_LIGHT, 0, sizeof dev.light); store_mark(NPH_R_FW, NPH_FW_BRIGHT_REV, 1);
    }
#if BD_SLOT_COLS == 16
    {   /* settings saved by older builds kept the top-right button on slot 15; nuphy.io addresses it as slot 60 (the knob-press slot) */
        bool moved = false;
        for (int l = 0; l < NPH_LAYERS; l++) {
            uint8_t *e60 = &dev.keymap[(l * NPH_SLOTS + 60) * 2], *e15 = &dev.keymap[(l * NPH_SLOTS + 15) * 2];
            if (e60[0] == 0 && e60[1] == 0 && (e15[0] || e15[1])) { e60[0] = e15[0]; e60[1] = e15[1]; e15[0] = e15[1] = 0; moved = true; }
        }
        /* earlier builds shipped plain Print Screen (does nothing on macOS) on slot 13; swap exactly that old default for the V3's screenshot functions */
        {
            uint8_t *s13[4] = {&dev.keymap[(0 * NPH_SLOTS + 13) * 2], &dev.keymap[(4 * NPH_SLOTS + 13) * 2], &dev.keymap[(1 * NPH_SLOTS + 13) * 2], &dev.keymap[(5 * NPH_SLOTS + 13) * 2]};
            if (s13[0][1] == 0x46 && !s13[0][0] && s13[1][1] == 0x46 && !s13[1][0] && s13[2][1] == 0x01 && !s13[2][0] && s13[3][1] == 0x01 && !s13[3][0]) {
                s13[0][0] = s13[1][0] = 0x7E; s13[0][1] = s13[1][1] = 0x0B; s13[2][0] = s13[3][0] = 0x7E; s13[2][1] = s13[3][1] = 0x0A; moved = true;
            }
        }
        if (moved) store_mark(NPH_R_KEYMAP, 0, NPH_KEYMAP_BYTES);
    }
#endif
    keyeng_init(&eng, &dev);
    light_init(&lit, &dev);
    leds_init();
    usb_init(&dev);
    eng.fn_cb = on_fn;
    {
        static const rf_hooks_t rh = { .tx = rfio_tx, .reset = rfio_reset, .ctx = NULL };
        rf_init(&rf, &rh, BD_BLE_NAME, BD_DONGLE_NAME);
        rfio_init();
    }

    uint32_t t_matrix = 0, t_light = 0, t_side = 0, t_os = 0, t_dirty = 0, t_sleep = 0;
    uint8_t os_cand = 0, os_stable = 0, dial_cand = 1, dial_n = 0;
    bool dial_usb = true, wl_init = false;
    uint32_t t_dial = 0;
    os_mode = board_os_switch_is_mac() ? 0 : 1;
    keyeng_set_mode(&eng, os_mode); lit.mode = os_mode;

    last_activity = millis();
    for (;;) {
        usb_task();
        leds_task();
        const uint32_t now = millis();

        if (now != t_matrix) {                                   /* 1 kHz */
            t_matrix = now;
            matrix_task(on_key);
            keyeng_tick(&eng, now);
            ke_report_t r;
            if (keyeng_report(&eng, &r)) { route_report(&r); mouse_change(&r, now); }
            mouse_tick(now);
            if (fn_reset_t && now - fn_reset_t >= 3000) { fn_reset_t = 0; hook_factory_reset(); if (rf.link_mode != LINK_USB) rf_clear_devices(&rf); lit.test = 4; fn_test_t = 0; t_light = now + 600; }
            if (fn_test_t) lit.test = (uint8_t)(1 + ((now - fn_test_t) / 400) % 4);
            else if (lit.test && !fn_reset_t && now >= t_light) lit.test = 0;
            {
                static bool ku;
                if (dev.keyupload != ku) { ku = dev.keyupload; ku_t = now; if (ku) keyeng_release_all(&eng); }
                if (ku && (now - ku_t > 300000u || !usb_mounted())) dev.keyupload = false;      /* never leave the board muted */
            }

            int c;                                               /* radio link */
            while ((c = rfio_getc()) >= 0) rf_rx_byte(&rf, (uint8_t)c, now);
            rf_task(&rf, now);
            rf_report_repeat(&rf, now);
            if (fn_link != 0xFF && !fn_paired && now - fn_t >= 300) { fn_paired = true; wl_mode = fn_link; rf_pair(&rf, fn_link); }
        }

        {   /* nuphy.io "stabilization": index 0/1 = low, 2 = intermediate (default), 3 = high -> key debounce */
            static uint8_t lvl = 0xFF;
            if (dev.kbfunc[0] != lvl) {
                lvl = dev.kbfunc[0];
                if (lvl <= 1) matrix_set_debounce(2, 3); else if (lvl == 2) matrix_set_debounce(3, 5); else matrix_set_debounce(6, 10);
            }
        }

        if (now - t_dial >= 20) {                                /* USB / wireless slide switch, debounced */
            t_dial = now;
            const uint8_t u = board_device_switch_is_usb();
            if (u == dial_cand) { if (dial_n < 5) dial_n++; } else { dial_cand = u; dial_n = 0; }
            if (dial_n >= 3 && dial_usb != dial_cand) wake_up();         /* flipping the USB / wireless switch wakes the board */
            if (dial_n >= 3) dial_usb = dial_cand;
            if (rf.ready) {
                if (!wl_init) { wl_init = true; wl_mode = rf.func[4] < LINK_USB ? rf.func[4] : rf.rf_channel; }
                const uint8_t want = dial_usb ? LINK_USB : wl_mode;
                if (rf.link_mode != want) rf_set_link(&rf, want);
                route_switch(rf.link_mode != LINK_USB);
                usb_enable(want == LINK_USB);                    /* wireless: the USB peripheral and its 48 MHz clock are off; the slide switch brings them back */
            }
        }

        if (now - t_os >= 20) {                                  /* OS slide switch, debounced */
            t_os = now;
            const uint8_t m = board_os_switch_is_mac() ? 0 : 1;
            if (m == os_cand) { if (os_stable < 5) os_stable++; } else { os_cand = m; os_stable = 0; }
            if (os_stable >= 3 && os_cand != os_mode) { wake_up(); os_mode = os_cand; keyeng_set_mode(&eng, os_mode); lit.mode = os_mode; notify_mode(os_mode); notify_light(os_mode); }
        }

        if (reset_event_t && (int32_t)(now - reset_event_t) >= 0) { reset_event_t = 0; const uint8_t f2[1] = {0xF2}; usb_send_raw(f2, 1); }
        if (usb_take_resume()) wake_up();                         /* the host woke up */
        if (keyeng_layer(&eng) != last_layer) notify_layer(keyeng_layer(&eng), os_mode);

        if (store_dirty()) {                                      /* save a moment after the last change; appends are tiny, a bank swap waits for idle hands */
            if (!t_dirty) t_dirty = now;
            else if (now - t_dirty >= 400 && (!store_would_erase() || !any_key_down())) { store_commit(); dev.dirty = 0; t_dirty = 0; }
        } else t_dirty = 0;

        if (now - t_sleep >= 1000) { t_sleep = now; sleep_poll(now); }

        if (now - t_light >= 16 && !sleep_level) {               /* ~60 fps key lighting (the LEDs are unpowered while asleep) */
            t_light = now;
            lit.bat_on = dev.fwcfg[NPH_FW_BAT_STRIP] != 0; lit.bat_pct = rf.battery; lit.bat_charging = (rf.charge & 1) != 0;
            light_render(&lit, now, !dev.fwcfg[NPH_FW_CAPS_OFF] && (rf_route ? (rf.led & 2) != 0 : (usb_leds() & 2) != 0));
            rf_indicator(now);
            if (!leds_keys_busy()) leds_show_keys(lit.key);
        }
        if (now - t_side >= 33 && !leds_keys_busy() && !sleep_level) {           /* ~30 fps side lighting */
            t_side = now;
            leds_show_side(lit.side);
        }

        if (stop_ok()) { if (power_stop(STOP_WAKE_MS)) wake_up(); }    /* STOP until a key / the next wake-up tick */
        else power_idle();                                             /* otherwise sleep until the next 1 kHz tick */
    }
}
