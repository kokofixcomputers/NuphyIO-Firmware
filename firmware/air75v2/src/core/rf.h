/* nRF52 radio co-processor link (BLE x3 + 2.4 GHz dongle), UART protocol ported from NuPhy's QMK `common/wireless`.
 * Portable: the board supplies tx / reset hooks, the caller feeds received bytes and ticks rf_task(). */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define RF_HEAD 0x5A
#define RF_MAX_LEN 64
#define RF_FUNC_LEN 32

enum { LINK_RF24 = 0, LINK_BLE1 = 1, LINK_BLE2 = 2, LINK_BLE3 = 3, LINK_USB = 4 };
enum { RF_IDLE = 0, RF_PAIRING = 1, RF_LINKING = 2, RF_CONNECT = 3, RF_DISCONNECT = 4, RF_SLEEP = 5, RF_SNIF = 6 };

/* radio commands (host -> nRF, and the replies that echo them) */
enum {
    RFC_POWER_UP = 0xF0, RFC_SLEEP = 0xF1, RFC_HAND = 0xF2, RFC_SNIF = 0xF3, RFC_24G_SUSPEND = 0xF4, RFC_IDLE_EXIT = 0xFE,
    RFC_RPT_MS = 0xE0, RFC_RPT_BYTE_KB = 0xE1, RFC_RPT_BIT_KB = 0xE2, RFC_RPT_CONSUME = 0xE3, RFC_RPT_SYS = 0xE4,
    RFC_SET_LINK = 0xC0, RFC_SET_CONFIG = 0xC1, RFC_GET_CONFIG = 0xC2, RFC_SET_NAME = 0xC3, RFC_GET_NAME = 0xC4, RFC_CLR_DEVICE = 0xC5,
    RFC_NEW_ADV = 0xC7, RFC_STS_SYNC = 0xC9, RFC_SET_24G_NAME = 0xCA, RFC_GO_TEST = 0xCF, RFC_DFU = 0xB1,
    RFC_WRITE_DATA = 0x80, RFC_READ_DATA = 0x81,
};

typedef struct {
    void (*tx)(void *ctx, const uint8_t *b, uint8_t n);   /* send one frame (the board toggles the wake pin around it) */
    void (*reset)(void *ctx);                             /* pulse the nRF reset line */
    void *ctx;
} rf_hooks_t;

typedef struct {
    rf_hooks_t h;
    const char *ble_name, *dongle_name;
    /* link */
    uint8_t link_mode, rf_channel, ble_channel;           /* what the keyboard wants */
    uint8_t state, charge, led, battery;                  /* what the radio reports */
    bool    ready;                                        /* init handshake finished */
    bool    cfg_dirty;                                    /* link choice changed: persist it in the nRF once things settle */
    uint32_t t_cfg;
    /* rx */
    uint8_t  rx[RF_MAX_LEN];
    uint8_t  rx_n;
    uint32_t rx_t;
    /* init + sync */
    uint8_t  step, tries;
    uint32_t t_next, t_sync;
    bool     hand_ok, read_ok, sts_ok, adv_ok, need_link;
    uint8_t  func[RF_FUNC_LEN];
    uint8_t  sync_lost, mismatch;
    uint32_t t_last_rx;
    /* key reports are repeated for a short while to survive dropped frames */
    uint8_t  rep_cmd[2], rep_len[2], rep_buf[2][16];
    uint32_t rep_t, rep_until, now;
} rf_t;

void rf_init(rf_t *r, const rf_hooks_t *h, const char *ble_name, const char *dongle_name);
void rf_rx_byte(rf_t *r, uint8_t b, uint32_t now);
void rf_task(rf_t *r, uint32_t now);                       /* call every ms or so */
void rf_set_link(rf_t *r, uint8_t mode);                   /* switch to a channel (0 = 2.4G, 1-3 = BLE, 4 = USB) */
void rf_pair(rf_t *r, uint8_t mode);                       /* start advertising / pairing on that channel */
void rf_clear_devices(rf_t *r);
bool rf_send_report(rf_t *r, uint8_t cmd, const uint8_t *d, uint8_t n);   /* false if not connected */
void rf_report_repeat(rf_t *r, uint32_t now);              /* re-send the last key reports while keys are active */
void rf_report_clear(rf_t *r);
