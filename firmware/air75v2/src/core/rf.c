#include "rf.h"
#include <string.h>

static uint8_t sum8(const uint8_t *d, uint8_t n) { uint8_t s = 0; while (n--) s += *d++; return s; }
static uint8_t xsum8(const uint8_t *d, uint8_t n) { return sum8(d, n) ^ RF_HEAD; }

static void frame(rf_t *r, uint8_t cmd, uint8_t ack, const uint8_t *d, uint8_t n, uint8_t chk) {
    uint8_t f[RF_MAX_LEN + 5];
    if (n > RF_MAX_LEN - 1) return;
    f[0] = RF_HEAD; f[1] = cmd; f[2] = ack; f[3] = n;
    memcpy(&f[4], d, n);
    f[4 + n] = chk;
    r->h.tx(r->h.ctx, f, (uint8_t)(n + 5));
}

/* Short commands carry a plain-sum checksum, exactly as the stock firmware sends them; names and key reports use sum ^ 0x5A. */
static void cmd_simple(rf_t *r, uint8_t cmd) { const uint8_t d[1] = {0}; frame(r, cmd, 0, d, 1, 0); }
static void cmd_mode(rf_t *r, uint8_t cmd) { const uint8_t d[1] = {r->link_mode}; frame(r, cmd, 0, d, 1, r->link_mode); }

static void send_ble_name(rf_t *r) {
    uint8_t d[RF_MAX_LEN]; const uint8_t n = (uint8_t)strlen(r->ble_name);
    if (n > 40) return;
    d[0] = 1; d[1] = (uint8_t)(n + 1); memcpy(&d[2], r->ble_name, n); d[2 + n] = '#';
    frame(r, RFC_SET_NAME, 0, d, (uint8_t)(n + 3), xsum8(d, (uint8_t)(n + 3)));
}

static void send_dongle_name(rf_t *r) {
    uint8_t d[RF_MAX_LEN]; const uint8_t n = (uint8_t)strlen(r->dongle_name);
    if (n > 28) return;
    d[0] = (uint8_t)(n * 2 + 2); d[1] = 3;
    for (uint8_t i = 0; i < n; i++) { d[2 + i * 2] = (uint8_t)r->dongle_name[i]; d[3 + i * 2] = 0; }
    frame(r, RFC_SET_24G_NAME, 0, d, d[0], xsum8(d, d[0]));
}

static void send_write_data(rf_t *r) {
    uint8_t d[RF_FUNC_LEN + 2];
    r->func[4] = r->link_mode; r->func[5] = r->rf_channel; r->func[6] = r->ble_channel;
    d[0] = 0; d[1] = RF_FUNC_LEN; memcpy(&d[2], r->func, RF_FUNC_LEN);
    frame(r, RFC_WRITE_DATA, 0, d, RF_FUNC_LEN + 2, (uint8_t)(xsum8(r->func, RF_FUNC_LEN) + RF_FUNC_LEN));
}

void rf_init(rf_t *r, const rf_hooks_t *h, const char *ble_name, const char *dongle_name) {
    memset(r, 0, sizeof *r);
    r->h = *h; r->ble_name = ble_name; r->dongle_name = dongle_name;
    r->link_mode = LINK_USB; r->rf_channel = LINK_BLE1; r->ble_channel = LINK_BLE1;
    r->battery = 100; r->state = RF_IDLE;
}

void rf_rx_byte(rf_t *r, uint8_t b, uint32_t now) {
    if (r->rx_n == 0 && b != RF_HEAD) return;
    if (r->rx_n < RF_MAX_LEN) r->rx[r->rx_n++] = b;
    r->rx_t = now;
}

static void process(rf_t *r) {
    const uint8_t *b = r->rx; const uint8_t n = r->rx_n, len = b[3];
    if (n >= 5) {
        if (len >= RF_MAX_LEN - 4 || n < len + 5 || sum8(&b[4], len) != b[4 + len]) return;
    } else if (!(n == 3 && b[2] == 0xA0)) {
        return;
    }
    r->sync_lost = 0; r->t_last_rx = r->now;
    switch (b[1]) {
        case RFC_HAND: r->hand_ok = true; break;
        case RFC_NEW_ADV: r->adv_ok = true; break;
        case RFC_READ_DATA:
            if (len >= RF_FUNC_LEN) {
                memcpy(r->func, &b[4], RF_FUNC_LEN);
                if (r->func[4] <= LINK_USB && !r->ready) r->link_mode = r->func[4];
                if (r->func[5] < LINK_USB) r->rf_channel = r->func[5];
                if (r->func[6] >= LINK_BLE1 && r->func[6] <= LINK_BLE3) r->ble_channel = r->func[6];
                r->read_ok = true;
            }
            break;
        case RFC_STS_SYNC:
            if (n < 9) break;
            if (b[4] == r->link_mode) {
                r->mismatch = 0;
                r->state = b[5];
                if (r->state == RF_CONNECT && (b[6] & 0xF8) == 0) r->led = b[6];
                if ((b[7] & 0xFC) == 0) r->charge = b[7];
                if (b[8] <= 100) r->battery = b[8];
            } else if (b[5] != 0xFE) {
                if (++r->mismatch >= 5) { r->mismatch = 0; r->need_link = true; }
            }
            r->sts_ok = true;
            break;
        default: break;
    }
}

void rf_task(rf_t *r, uint32_t now) {
    r->now = now;
    if (r->rx_n && now - r->rx_t >= 2) { process(r); r->rx_n = 0; }

    if (!r->ready) {                                   /* handshake: hand -> read config -> sync -> names */
        if (now < r->t_next) return;
        r->t_next = now + 25;
        switch (r->step) {
            case 0: if (r->hand_ok || r->tries >= 10) { r->step = 1; r->tries = 0; } else { r->tries++; cmd_simple(r, RFC_HAND); } break;
            case 1: if (r->read_ok || r->tries >= 10) { r->step = 2; r->tries = 0; } else { r->tries++; const uint8_t d[2] = {0, RF_FUNC_LEN}; frame(r, RFC_READ_DATA, 0, d, 2, RF_FUNC_LEN); } break;
            case 2: if (r->sts_ok || r->tries >= 10) { r->step = 3; r->tries = 0; } else { r->tries++; cmd_mode(r, RFC_STS_SYNC); } break;
            case 3: send_ble_name(r); r->step = 4; break;
            case 4: send_dongle_name(r); r->step = 5; break;
            default: r->ready = true; r->need_link = r->link_mode != LINK_USB; r->t_sync = now; break;
        }
        return;
    }

    if (now - r->t_sync >= 200) {                      /* status sync */
        r->t_sync = now;
        if (r->need_link) { r->need_link = false; r->state = RF_LINKING; cmd_mode(r, RFC_SET_LINK); }
        cmd_mode(r, RFC_STS_SYNC);
        if (r->link_mode != LINK_USB && ++r->sync_lost >= 5) {   /* radio stopped answering: reset it and redo the handshake */
            r->sync_lost = 0; r->h.reset(r->h.ctx);
            r->ready = false; r->step = 0; r->tries = 0; r->hand_ok = r->read_ok = r->sts_ok = false; r->t_next = now + 100;
        }
    }
    if (r->cfg_dirty && now - r->t_cfg > 2000) { r->cfg_dirty = false; send_write_data(r); }
}

void rf_set_link(rf_t *r, uint8_t mode) {
    if (mode > LINK_USB) return;
    r->link_mode = mode;
    if (mode != LINK_USB) { r->rf_channel = mode; r->ble_channel = mode; }
    r->state = RF_IDLE; r->need_link = true; r->cfg_dirty = true; r->t_cfg = r->now; r->t_sync = 0;
    rf_report_clear(r);
}

void rf_pair(rf_t *r, uint8_t mode) {
    if (mode >= LINK_USB) return;
    r->link_mode = r->rf_channel = r->ble_channel = mode;
    const uint8_t d[2] = {mode, 1};
    r->state = RF_PAIRING; r->adv_ok = false;
    frame(r, RFC_NEW_ADV, 0, d, 2, (uint8_t)(mode + 1));
    r->cfg_dirty = true; r->t_cfg = r->now;
}

void rf_clear_devices(rf_t *r) { cmd_mode(r, RFC_SET_LINK); cmd_simple(r, RFC_CLR_DEVICE); }

bool rf_send_report(rf_t *r, uint8_t cmd, const uint8_t *d, uint8_t n) {
    if (r->link_mode == LINK_USB || r->state != RF_CONNECT || n > 16) return false;
    frame(r, cmd, 1, d, n, xsum8(d, n));
    if (cmd == RFC_RPT_BYTE_KB) {
        r->rep_cmd[0] = cmd; r->rep_len[0] = n; memcpy(r->rep_buf[0], d, n);
        r->rep_t = r->now; r->rep_until = r->now + 250;
    }
    return true;
}

void rf_report_repeat(rf_t *r, uint32_t now) {
    if (r->state != RF_CONNECT || r->link_mode == LINK_USB || !r->rep_cmd[0] || now >= r->rep_until || now - r->rep_t < 50) return;
    r->rep_t = now;
    frame(r, r->rep_cmd[0], 1, r->rep_buf[0], r->rep_len[0], xsum8(r->rep_buf[0], r->rep_len[0]));
}

void rf_report_clear(rf_t *r) { memset(r->rep_cmd, 0, sizeof r->rep_cmd); r->rep_until = 0; }
