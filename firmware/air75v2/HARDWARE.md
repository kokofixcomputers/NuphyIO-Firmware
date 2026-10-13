# Air75 V2 hardware map

Everything here was read from the QMK tree (`keyboards/nuphy/air75v2/ansi/`, `keyboards/nuphy/common/`), not guessed.
`tools/gen_board.py` regenerates `src/core/board_data.h` from `keyboard.json` so the tables can't drift.

| Item | Value |
|---|---|
| MCU | STM32F072RBT6 (Cortex-M0, 128 KB flash, 16 KB RAM, USB FS) |
| Clock | no crystal: PLL = HSI/2 x 12 = 48 MHz; USB from HSI48, trimmed by the CRS from USB SOF |
| Bootloader | ST ROM DFU (`0483:df11`). Not erasable by user firmware, so the board always stays recoverable |
| USB (stock) | VID `0x19F5`, PID `0x3246` |
| Matrix | 6 rows x 17 cols, `COL2ROW` |
| Row pins | C14, C15, A0, A1, A2, A3 (driven low one at a time, idle high) |
| Col pins | A4, A5, A6, B9, B0, B1, B10, B11, B12, B13, B14, B15, A8, A9, A10, A15, B3 (input, pull-up, pressed = low) |
| Key LEDs | 84 x WS2812-class, data **A7** (= SPI1_MOSI, AF0), snake wiring: row 0 L->R, row 1 R->L, ... |
| Side LEDs | 12 x WS2812-class, data **C8** (stock firmware bit-bangs it with 0.15 us / 0.7 us pulses) |
| LED power | `DC_BOOST` **C2** high; LED enables **C6** (keys) and **C9** (side) driven low. Off = floating inputs |
| Slide switches | **C0** device (high = USB), **C1** OS (high = Mac, low = Windows); both input pull-up |
| Radio | nRF52 on USART1 (B6 TX / B7 RX), reset **B4** (active low), wake **B8**, test **B5** |
| Recovery | button under the Caps Lock keycap (BOOT0) while plugging in; the open firmware also enters DFU if **Esc** is held at power-up |

## Matrix -> V3 slot map
The V3 protocol numbers keys by slot = `row*16 + col` of the V3's matrix. All five main rows of the V2 line up 1:1 with
the V3's rows; the top row differs (V2: Esc, F1-F12, Print, Ins, Del - V3: Esc, F1-F12, function key, Ins, knob), so:

| V2 key | V3 slot |
|---|---|
| Esc, F1-F12 | 0-12 |
| Print / Ins / Del | 13 / 14 / **15** (slot 15 is empty on the V3; used for the V2's extra Del) |
| rows 1-5 | same slot numbers as the V3 (16..95) |

The generated table is `BD_SLOT[row][col]` in `board_data.h`. The V3's knob slots (60, 96, 97) are unused on the V2.

## LEDs as the V3 app sees them
`GET_LED_COLOR` returns 104 RGB triples: the 84 key LEDs in ascending V3-slot order (the V3's knob-press slot 60 is
dark, and the V2's extra Del key has no entry because the V3 layout has no key there), then 20 side LEDs. The V2 has 12,
so the 20 are synthesised from them.
