#pragma once
#include <stdbool.h>
#include "nph_proto.h"
void store_init(nph_t *n);                 /* bind the settings segments to the flash store (call before store_load) */
bool store_load(nph_t *n);                 /* restore saved settings; false if nothing was saved yet (defaults stay) */
void store_mark(uint8_t seg, uint16_t off, uint16_t len);   /* range of a settings segment changed (nph_hooks_t.mark) */
void store_mark_all(void);
bool store_dirty(void);
bool store_would_erase(void);              /* the next store_commit erases a bank (slow) */
bool store_commit(void);                   /* persist what changed; appends a few halfwords, rarely compacts */
void store_status(uint8_t out[32]);        /* diagnostics for the host: [1, bank, appends lo/hi, compactions, failures, seq lo/hi, loaded-at-boot] */
void store_erase(void);                    /* wipe all saved settings */
