/*
 * USB: two HID interfaces.
 *   0  keyboard (report 1, 6KRO) + consumer control (report 2) + system control (report 3)
 *   1  configuration channel: the exact descriptor the Air75 V3 exposes (usage page 0x01 / usage 0x00, 64-byte
 *      in/out), carrying the V3 protocol that nuphy.io and our web app speak.
 * VID is NuPhy's 0x19F5. PID defaults to the Air75 V3's (0x1028) so nuphy.io recognises the device; override with
 * -DUSB_PID=0x3246 to keep the V2's own identity (our web app accepts both).
 */
#include "usb.h"
#include <string.h>
#include "tusb.h"
#include "device/dcd.h"
#include "stm32f0xx.h"
#include "board.h"
#include "board_cfg.h"

#ifndef USB_PID
#define USB_PID 0x1028
#endif

static nph_t *proto;
static uint8_t leds_from_host;

/* ---------------- report descriptors ---------------- */
static const uint8_t kbd_report[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,                                 /* keyboard, report 1 */
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,   /* modifiers */
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,                                              /* reserved */
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x95, 0x05, 0x75, 0x01, 0x91, 0x02,          /* LEDs */
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0x05, 0x07, 0x19, 0x00, 0x29, 0xE7, 0x15, 0x00, 0x25, 0xE7, 0x95, 0x06, 0x75, 0x08, 0x81, 0x00,   /* 6 keys */
    0xC0,
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x02,                                 /* consumer, report 2 */
    0x19, 0x00, 0x2A, 0x3C, 0x03, 0x15, 0x00, 0x26, 0x3C, 0x03, 0x75, 0x10, 0x95, 0x01, 0x81, 0x00,
    0xC0,
    0x05, 0x01, 0x09, 0x80, 0xA1, 0x01, 0x85, 0x03,                                 /* system control, report 3 */
    0x19, 0x01, 0x2A, 0xB7, 0x00, 0x15, 0x01, 0x26, 0xB7, 0x00, 0x75, 0x10, 0x95, 0x01, 0x81, 0x00,                /* one 16-bit system usage (power 0x81, sleep, wake, do-not-disturb 0x9B ...) */
    0xC0,
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x04, 0x09, 0x01, 0xA1, 0x00,         /* mouse, report 4 */
    0x05, 0x09, 0x19, 0x01, 0x29, 0x05, 0x15, 0x00, 0x25, 0x01, 0x95, 0x05, 0x75, 0x01, 0x81, 0x02,   /* 5 buttons */
    0x95, 0x01, 0x75, 0x03, 0x81, 0x01,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06,   /* x, y, wheel */
    0x05, 0x0C, 0x0A, 0x38, 0x02, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x01, 0x81, 0x06,         /* horizontal wheel */
    0xC0, 0xC0,
};
/* byte-for-byte the V3's config interface descriptor (read from the real board with ioreg) */
static const uint8_t cfg_report[] = {
    0x06, 0x01, 0x00, 0x09, 0x00, 0xA1, 0x01,
    0x09, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x95, 0x40, 0x75, 0x08, 0x81, 0x02,
    0x09, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x95, 0x40, 0x75, 0x08, 0x91, 0x02,
    0xC0,
};

/* ---------------- device / configuration ---------------- */
static const tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE, .bcdUSB = 0x0200,
    .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0, .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x19F5, .idProduct = USB_PID, .bcdDevice = 0x0100,
    .iManufacturer = 1, .iProduct = 2, .iSerialNumber = 3, .bNumConfigurations = 1,
};
uint8_t const *tud_descriptor_device_cb(void) { return (uint8_t const *)&desc_device; }

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_INOUT_DESC_LEN)
static const uint8_t desc_config[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(kbd_report), 0x81, 16, 1),
    TUD_HID_INOUT_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, sizeof(cfg_report), 0x02, 0x82, 64, 1),
};
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) { (void)index; return desc_config; }
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) { return instance == 0 ? kbd_report : cfg_report; }

static const char *const strs[] = {NULL, "NuPhy", BD_USB_NAME, NULL};
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t buf[33];
    uint8_t n;
    if (index == 0) { buf[1] = 0x0409; n = 1; }
    else if (index == 3) {                                         /* serial from the chip's unique ID */
        uint8_t uid[12]; board_uid(uid);
        static const char hex[] = "0123456789ABCDEF";
        for (n = 0; n < 24; n++) buf[1 + n] = (uint16_t)hex[(n & 1) ? (uint8_t)(uid[n / 2] & 15) : (uint8_t)(uid[n / 2] >> 4)];
    } else {
        if (index >= 4 || !strs[index]) return NULL;
        for (n = 0; strs[index][n] && n < 31; n++) buf[1 + n] = (uint16_t)(uint8_t)strs[index][n];
    }
    buf[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * n + 2));
    return buf;
}

/* ---------------- HID callbacks ---------------- */
#define Q_DEPTH 8
static uint8_t q[Q_DEPTH][NPH_FRAME];
static uint8_t q_head, q_tail;

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
    if (instance == 0) {
        /* keyboard LEDs: with a report ID the payload may or may not repeat it; the LED bits are the last byte */
        if (report_type == HID_REPORT_TYPE_OUTPUT && bufsize) leds_from_host = buffer[bufsize - 1];
        (void)report_id;
        return;
    }
    uint8_t in[NPH_FRAME] = {0};
    memcpy(in, buffer, bufsize > NPH_FRAME ? NPH_FRAME : bufsize);
    uint8_t *out = q[q_head % Q_DEPTH];
    if ((uint8_t)(q_head - q_tail) < Q_DEPTH && nph_handle(proto, in, out)) q_head++;
}

/* ---------------- sending ---------------- */
static ke_report_t want, sent;
static bool want_dirty = true;

#define MQ 8
static uint8_t mq[MQ][5], mq_head, mq_tail;
void usb_mouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel, int8_t pan) {
    if ((uint8_t)(mq_head - mq_tail) >= MQ) return;
    uint8_t *m = mq[mq_head % MQ]; m[0] = buttons; m[1] = (uint8_t)x; m[2] = (uint8_t)y; m[3] = (uint8_t)wheel; m[4] = (uint8_t)pan; mq_head++;
}
void usb_send_raw(const uint8_t *frame, uint8_t len) {                /* an unsolicited 64-byte report on the config channel */
    if ((uint8_t)(q_head - q_tail) >= Q_DEPTH) return;
    uint8_t *o = q[q_head % Q_DEPTH]; memset(o, 0, NPH_FRAME); memcpy(o, frame, len > NPH_FRAME ? NPH_FRAME : len); q_head++;
}
void usb_send(const ke_report_t *r) { want = *r; want_dirty = true; }
uint8_t usb_leds(void) { return leds_from_host; }
bool usb_mounted(void) { return tud_mounted(); }
bool usb_suspended(void) { return tud_suspended(); }
bool usb_remote_wakeup(void) { return tud_remote_wakeup(); }
static volatile bool resumed;
void tud_resume_cb(void) { resumed = true; }
bool usb_take_resume(void) { const bool r = resumed; resumed = false; return r; }

static bool usb_on = true;
void usb_enable(bool on) {
    if (on == usb_on) return;
    usb_on = on;
    if (!on) {
        USB->BCDR &= ~USB_BCDR_DPPU;                                       /* detach: the host sees the keyboard unplugged */
        dcd_event_bus_signal(0, DCD_EVENT_UNPLUGGED, false);                /* the stack forgets the session */
        tud_task();
        NVIC_DisableIRQ(USB_IRQn);
        USB->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;                          /* analog front end off */
        board_usb_clocks(false);
        resumed = false;
    } else {
        board_usb_clocks(true);
        q_head = q_tail = 0; mq_head = mq_tail = 0; memset(&sent, 0, sizeof sent); want_dirty = true;
        dcd_init(0, NULL);                                                  /* resets the peripheral and attaches (pull-up) */
        NVIC_EnableIRQ(USB_IRQn);
    }
}

void USB_IRQHandler(void) { tud_int_handler(0); }
uint32_t tusb_time_millis_api(void) { return millis(); }

void usb_init(nph_t *p) { proto = p; tusb_init(); }

void usb_task(void) {
    tud_task();
    if (!tud_mounted()) return;

    if (q_tail != q_head && tud_hid_n_ready(1)) {
        if (tud_hid_n_report(1, 0, q[q_tail % Q_DEPTH], NPH_FRAME)) q_tail++;
    }
    if (mq_tail != mq_head && tud_hid_n_ready(0)) {
        if (tud_hid_n_report(0, 4, mq[mq_tail % MQ], 5)) mq_tail++;
    } else if (want_dirty && tud_hid_n_ready(0)) {
        /* one report per call so each gets its own endpoint slot */
        if (memcmp(want.keys, sent.keys, sizeof want.keys) || want.mods != sent.mods) {
            uint8_t k[8] = {want.mods, 0, want.keys[0], want.keys[1], want.keys[2], want.keys[3], want.keys[4], want.keys[5]};
            if (tud_hid_n_report(0, 1, k, 8)) { memcpy(sent.keys, want.keys, sizeof sent.keys); sent.mods = want.mods; }
        } else if (want.consumer != sent.consumer) {
            const uint16_t c = want.consumer;
            if (tud_hid_n_report(0, 2, &c, 2)) sent.consumer = want.consumer;
        } else if (want.system != sent.system) {
            const uint16_t s = want.system;
            if (tud_hid_n_report(0, 3, &s, 2)) sent.system = want.system;
        } else want_dirty = false;
    }
}
