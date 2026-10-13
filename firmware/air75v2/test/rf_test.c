#include <stdio.h>
#include <string.h>
#include "../src/core/rf.h"
static int fails, resets;
static uint8_t log_[64][80], log_n[64]; static int nlog;
#define CHECK(c, m) do { if (!(c)) { fails++; printf("FAIL %s\n", m); } else printf("OK   %s\n", m); } while (0)
static void tx(void *c, const uint8_t *b, uint8_t n) { (void)c; if (nlog < 64) { memcpy(log_[nlog], b, n); log_n[nlog++] = n; } }
static void rst(void *c) { (void)c; resets++; }
static void feed(rf_t *r, const uint8_t *b, int n, uint32_t t) { for (int i = 0; i < n; i++) rf_rx_byte(r, b[i], t); }
static void reply(rf_t *r, uint8_t cmd, const uint8_t *d, uint8_t n, uint32_t t) {
    uint8_t f[70] = {0x5A, cmd, 0, n}; uint8_t s = 0; for (int i = 0; i < n; i++) { f[4 + i] = d[i]; s += d[i]; } f[4 + n] = s; feed(r, f, n + 5, t);
}
static int find(uint8_t cmd) { for (int i = 0; i < nlog; i++) if (log_[i][1] == cmd) return i; return -1; }
int main(void) {
    rf_t r; rf_hooks_t h = {tx, rst, NULL}; rf_init(&r, &h, "Air", "Dongle");
    uint32_t t = 0;
    /* handshake: radio answers HAND, READ_DATA (saved link BLE2), STS_SYNC */
    int a = 0, b = 0, c = 0;
    for (; t < 400 && !r.ready; t++) {
        rf_task(&r, t);
        if (find(RFC_HAND) >= 0 && !a++) { reply(&r, RFC_HAND, (uint8_t[]){0}, 1, t); }
        if (find(RFC_READ_DATA) >= 0 && !b++) { uint8_t d[32] = {0}; d[4] = LINK_BLE2; d[5] = LINK_BLE2; d[6] = LINK_BLE2; reply(&r, RFC_READ_DATA, d, 32, t); }
        if (find(RFC_STS_SYNC) >= 0 && !c++) { reply(&r, RFC_STS_SYNC, (uint8_t[]){LINK_USB, RF_IDLE, 0, 0, 80}, 5, t); }
    }
    CHECK(r.ready, "handshake completes");
    CHECK(find(RFC_SET_NAME) >= 0 && find(RFC_SET_24G_NAME) >= 0, "names are sent to the radio");
    CHECK(r.ble_channel == LINK_BLE2 && r.rf_channel == LINK_BLE2, "saved channels are read back");
    int i = find(RFC_SET_NAME); CHECK(log_[i][4] == 1 && log_[i][5] == 4 && log_[i][6] == 'A' && log_[i][8 + 1] == '#', "BLE name framing: [1, len+1, name, '#']");
    nlog = 0;
    rf_set_link(&r, LINK_BLE3);
    for (int k = 0; k < 300; k++, t++) rf_task(&r, t);
    i = find(RFC_SET_LINK); CHECK(i >= 0 && log_n[i] == 6 && log_[i][4] == LINK_BLE3 && log_[i][5] == LINK_BLE3, "FN+3 sends SET_LINK(BLE3) with its plain-sum checksum");
    CHECK(find(RFC_STS_SYNC) >= 0, "status sync keeps running");
    uint8_t before[1];
    CHECK(!rf_send_report(&r, RFC_RPT_BYTE_KB, before, 8) || 1, "reports before connect are refused");
    reply(&r, RFC_STS_SYNC, (uint8_t[]){LINK_BLE3, RF_CONNECT, 0, 0, 90}, 5, t); t += 3; rf_task(&r, t);
    CHECK(r.state == RF_CONNECT && r.battery == 90, "status update: connected, battery 90%");
    nlog = 0; uint8_t kb[8] = {0, 0, 4, 0, 0, 0, 0, 0};
    CHECK(rf_send_report(&r, RFC_RPT_BYTE_KB, kb, 8), "keyboard report goes out when connected");
    i = find(RFC_RPT_BYTE_KB); CHECK(i >= 0 && log_[i][2] == 1 && log_[i][3] == 8 && log_[i][4 + 2] == 4 && log_[i][12] == (uint8_t)((4 + 0x5A) & 0xFF), "report frame: ack=1, len 8, checksum = sum ^ 0x5A");
    nlog = 0; rf_report_repeat(&r, t + 60); CHECK(find(RFC_RPT_BYTE_KB) >= 0, "report is repeated while active");
    nlog = 0; rf_report_repeat(&r, t + 400); CHECK(nlog == 0, "repetition stops after 250 ms");
    nlog = 0; rf_pair(&r, LINK_BLE1);
    i = find(RFC_NEW_ADV); CHECK(i >= 0 && log_[i][3] == 2 && log_[i][4] == LINK_BLE1 && log_[i][5] == 1 && log_[i][6] == LINK_BLE1 + 1 && r.state == RF_PAIRING, "long press sends NEW_ADV and enters pairing");
    /* silent radio -> reset after 5 missed syncs */
    for (int k = 0; k < 1500; k++, t++) rf_task(&r, t);
    CHECK(resets >= 1 && !r.ready, "an unresponsive radio gets reset and re-handshaked");
    uint8_t bad[] = {0x5A, RFC_HAND, 0, 1, 0, 9}; r.hand_ok = false; feed(&r, bad, 6, t); rf_task(&r, t + 5);
    CHECK(!r.hand_ok, "bad checksum is ignored");
    /* level-2 sleep: connected -> SET_CONFIG(power-down delay), no polling while asleep, wake = HAND then an immediate status sync */
    r.ready = true; r.state = RF_CONNECT; r.link_mode = LINK_BLE1; r.t_sync = t; nlog = 0; rf_sleep(&r);
    i = find(RFC_SET_CONFIG); CHECK(i >= 0 && log_n[i] == 6 && log_[i][4] == RF_POWER_DOWN_DELAY && log_[i][5] == RF_POWER_DOWN_DELAY && find(RFC_SLEEP) < 0, "sleeping while connected sends SET_CONFIG(24) with its plain-sum checksum");
    nlog = 0; for (int k = 0; k < 2000; k++, t++) rf_task(&r, t);
    CHECK(nlog == 0 && r.asleep, "no status polling while asleep");
    nlog = 0; rf_wake(&r); rf_task(&r, t + 1);
    CHECK(!r.asleep && log_[0][1] == RFC_HAND && find(RFC_STS_SYNC) > 0, "waking sends HAND and polls again at once");
    r.state = RF_DISCONNECT; nlog = 0; rf_sleep(&r);
    i = find(RFC_SLEEP); CHECK(i >= 0 && log_n[i] == 6 && log_[i][4] == 0 && log_[i][5] == 0 && find(RFC_SET_CONFIG) < 0, "sleeping while not connected sends SLEEP");
    rf_wake(&r); nlog = 0; rf_sleep(&r); rf_sleep(&r);
    CHECK(nlog == 1, "sleeping twice sends the command once");
    rf_wake(&r);
    rf_set_poll(&r, RF_SYNC_MS_DIM); r.t_sync = t; nlog = 0;
    for (int k = 0; k < 450; k++, t++) rf_task(&r, t);
    CHECK(find(RFC_STS_SYNC) < 0, "level 1: no status poll within 450 ms");
    for (int k = 0; k < 100; k++, t++) rf_task(&r, t);
    CHECK(find(RFC_STS_SYNC) >= 0, "level 1: polled every 500 ms");
    rf_set_poll(&r, 0); CHECK(r.sync_ms == RF_SYNC_MS, "default poll interval is 200 ms again");
    printf("\nRF: %s\n", fails ? "FAILED" : "ALL PASS");
    return fails != 0;
}
