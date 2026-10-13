/* Persistent settings on the STM32F072's internal flash: wear-levelled log store (src/core/wlstore.c) over the top 32 KB. */
#include "store.h"
#include "stm32f0xx.h"
#include "wlstore.h"

extern const uint8_t __config_base[];
#define BASE ((uint32_t)__config_base)

static wl_t wl;

static void fl_wait(void) { while (FLASH->SR & FLASH_SR_BSY) {} }
static void fl_clear(void) { FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPERR; }
static void fl_unlock(void) { if (FLASH->CR & FLASH_CR_LOCK) { FLASH->KEYR = 0x45670123u; FLASH->KEYR = 0xCDEF89ABu; } }

static uint16_t hw_rd16(void *c, uint32_t off) { (void)c; return *(volatile const uint16_t *)(BASE + off); }
static void hw_prog16(void *c, uint32_t off, uint16_t v) {
    (void)c; fl_unlock(); fl_wait(); fl_clear();
    FLASH->CR |= FLASH_CR_PG; *(volatile uint16_t *)(BASE + off) = v; fl_wait();
    FLASH->CR &= ~FLASH_CR_PG; fl_clear(); FLASH->CR |= FLASH_CR_LOCK;
}
static void hw_erase(void *c, uint32_t off) {
    (void)c; fl_unlock(); fl_wait(); fl_clear();
    FLASH->CR |= FLASH_CR_PER; FLASH->AR = BASE + off; FLASH->CR |= FLASH_CR_STRT; fl_wait();
    FLASH->CR &= ~FLASH_CR_PER; fl_clear(); FLASH->CR |= FLASH_CR_LOCK;
}

void store_init(nph_t *n) {
    const wl_ops_t ops = { hw_rd16, hw_prog16, hw_erase, 0 };
    const wl_seg_t s[NPH_R_COUNT] = {
        [NPH_R_KEYMAP] = {n->keymap, sizeof n->keymap}, [NPH_R_MACRO] = {n->macro, sizeof n->macro}, [NPH_R_LIGHT] = {&n->light[0][0], sizeof n->light},
        [NPH_R_TD] = {n->td, sizeof n->td}, [NPH_R_TGL] = {n->tgl, sizeof n->tgl}, [NPH_R_SOCD] = {n->socd, sizeof n->socd},
        [NPH_R_KBFUNC] = {n->kbfunc, sizeof n->kbfunc}, [NPH_R_SLEEP] = {n->sleepcfg, sizeof n->sleepcfg}, [NPH_R_APPDEF] = {n->appdef, sizeof n->appdef}, [NPH_R_FW] = {n->fwcfg, sizeof n->fwcfg},
    };
    wl_init(&wl, &ops, s, NPH_R_COUNT);
}
bool store_load(nph_t *n) { (void)n; return wl_load(&wl); }
void store_mark(uint8_t seg, uint16_t off, uint16_t len) { wl_mark(&wl, seg, off, len); }
void store_mark_all(void) { wl_mark_all(&wl); }
bool store_dirty(void) { return wl_dirty(&wl); }
bool store_would_erase(void) { return wl_would_compact(&wl); }
bool store_commit(void) { return wl_commit(&wl); }
void store_status(uint8_t out[32]) {
    out[0] = 1; out[1] = wl.bank; out[2] = (uint8_t)wl.stat_appends; out[3] = (uint8_t)(wl.stat_appends >> 8);
    out[4] = wl.stat_compactions > 255 ? 255 : (uint8_t)wl.stat_compactions; out[5] = wl.stat_fail > 255 ? 255 : (uint8_t)wl.stat_fail;
    out[6] = (uint8_t)wl.seq; out[7] = (uint8_t)(wl.seq >> 8); out[8] = wl.loaded;
    out[11] = NPH_FW_REV;
    { const char *b = __DATE__ " " __TIME__; for (int i = 0; i < 20 && b[i]; i++) out[12 + i] = (uint8_t)b[i]; }      /* build time, ASCII */
    out[9] = wl_dirty(&wl); out[10] = (uint8_t)((WL_BANK_BYTES - wl.log_off) >> 8);        /* free log space, in 256-byte units */
}
void store_erase(void) { for (uint32_t p = 0; p < 2 * WL_BANK_BYTES; p += WL_PAGE) hw_erase(0, p); }
