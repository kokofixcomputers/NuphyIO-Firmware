# Air75 V2 open firmware

A from-scratch bare-metal firmware for the NuPhy Air75 V2 (STM32F072) that speaks the **same configuration protocol as the Air75 V3**,
so nuphy.io and the Air Deck web app in this repo can configure a V2 like a V3. No QMK.

```
src/core/     portable C, no hardware dependencies, unit-tested on the host
  nph_proto   the V3 wire protocol (handshake, XOR cipher, checksum, memory regions)
  keyeng      keymap -> HID: layers, transparent keys, macros, Tap Dance, Toggle
  light       back-light + side-light effects, V3-format light state, 104-LED snapshot for the live preview
  board_data  GENERATED from the QMK tree (matrix pins, LED chain, matrix->V3 slot map, default keymap)
src/hw/       STM32F072 drivers: startup, clocks, GPIO/matrix, key LEDs (SPI1 + DMA), side LEDs, flash settings
src/usb/      TinyUSB descriptors: keyboard/media/system HID + the V3's config HID interface
test/         host tests + end-to-end test against the web app's real client
```

## Build / test
```
make          # -> build/air75v2/air75v2.bin   (BOARD=halo75v2 for the Halo75 V2; needs arm-none-eabi-gcc)
make test     # protocol replay of a real V3 capture, key engine, lighting engine
make e2e      # the web app's real device client <-> the C protocol core, writes included
```

## Flashing (read this first)
The ST ROM bootloader cannot be overwritten, so the keyboard can always be recovered. Still:

1. **Back up what is there now**: put the board in DFU (unplug, hold the button under the Caps Lock keycap, plug in) and run `make backup`.
   (If the chip is read-protected the read-out fails; then you simply restore stock firmware from NuPhy's site.)
2. `make flash` (or `make BOARD=halo75v2 flash`) writes `build/<board>/<board>.bin` to `0x08000000` and resets.
3. To go back: DFU again, then `dfu-util -a 0 -d 0483:df11 -s 0x08000000:leave -D backup-original.bin`.

USB identity: VID `0x19F5`; PID defaults to `0x1028` (the V3's, so nuphy.io's device filter matches). Build with `make USB_PID=0x3246`
to keep the V2's own PID (Air Deck accepts both, nuphy.io will not list it).

## Status - what is and isn't verified
**Verified on a host (no hardware):** protocol core answers all 4,723 frames of a real V3 capture the way the real board did;
the web app's own client passes a full read/write end-to-end run against it (keymap, per-profile lighting, macros, Tap Dance, Toggle);
key engine and lighting engine unit tests; the image builds warning-free (20 KB flash, 10 KB of 16 KB RAM, 6 KB stack, linker-asserted).

**Never run on real hardware yet** (so expect first-flash surprises): USB enumeration, matrix scan timing, LED output timing (SPI1/DMA key
LEDs, timer-bit-banged side LEDs), flash settings save/restore, the software DFU jump, the slide-switch polarity, LED power gating.

**Not implemented:** Bluetooth / 2.4 GHz (the nRF52 is held in reset; wired only), NKRO (6KRO for now), firmware hot-key functions
(`0x7Exx`/`0x78xx` entries such as lighting shortcuts, radio selection, screenshot shortcuts), SOCD, sleep/battery handling,
audio-reactive "Rhythm", exact stock effect look (the 20 effects are original approximations).

## Boards: Air75 V2 and Halo75 V2

One source tree, two boards. `make` builds the Air75 V2 (`build/air75v2/air75v2.bin`), `make BOARD=halo75v2` builds the Halo75 V2 (`build/halo75v2/halo75v2.bin`).
`python3 tools/gen_board.py <board>` regenerates `src/boards/<board>/` (matrix pins, key -> V3 slot map, LED wiring, names) from the QMK tree.

| | Air75 V2 | Halo75 V2 |
|---|---|---|
| Key LEDs | 84 WS2812 (SPI + DMA) | 83 RGB dots on two IS31FL3733 chips (I2C1 B8/B9, DMA) |
| Side lights | 2 strips x 6 WS2812 (C8, bit-banged) | the 45-LED halo ring on the same two chips |
| LED power | C6 / C9 low = on | SDB1 C6 / SDB2 C7 high = on |
| OS switch (C1) | high = Mac | high = Windows |
| nRF wake pin | B8 | C4 |
| Right-hand keys | PgUp PgDn Home End | Home End PgUp PgDn (V3 slots keep their positions: slot 31/47/63/79) |

**The halo.** The Halo has no side strips, so the V3's *side light* (effect, brightness, speed, colour - the second light on nuphy.io / Air Deck) drives the whole 45-LED ring, laid out
in order round the board; on nuphy.io's 20-LED preview the ring is sampled evenly. FN+\ turns the five-LED segment of the ring that the stock firmware uses for its battery indicator
into a battery bar (red -> green, breathing while charging) and leaves the other 40 LEDs under software control, exactly like the left strip on the Air75 V2.
The V3's own FN2 side-light keys (brightness / effect / hue / speed / on-off) act on the ring too.

**Untested on a real Halo.** The I2C / IS31FL3733 driver (`src/hw/leds_is31.c`: page-register init, 400 kHz, one 193-byte DMA write per chip) follows the QMK driver and the datasheet
but has only been compile- and host-tested. If a key LED stays dark or flickers, the chip address (`0x50` / `0x53`), the 400 kHz timing and the global-current byte are the first things to check.
Recovery works as on the Air75 V2: hold Esc while plugging in (or the BOOT0 button) for the ROM bootloader.

## Wireless (nRF52 radio) — new, untested on hardware

The V2's nRF52 is now released from reset and driven over USART1 (460800 8E1, DMA receive) with the UART protocol from NuPhy's QMK
`common/wireless` (`src/core/rf.c`, host-tested in `test/rf_test.c` against a fake radio).

| Keys | Action |
|---|---|
| FN+1 / FN+2 / FN+3 | Bluetooth 1 / 2 / 3 (tap = switch, hold 0.3 s = pair a new device) |
| FN+4 | 2.4 GHz dongle (same tap / hold behaviour) |
| FN+↑ / FN+↓ | back-light brightness ±10 % (saved) |

These are the V3 keymap's own entries (`0x7E03-05`, `0x7E02`, `0x7827/8`), so remapping them in Air Deck / nuphy.io works.
The channel's key blinks blue (BLE) or green (dongle) while linking (slow) or pairing (fast), and stays lit for 2 s once connected.
The USB / wireless slide switch decides the transport: *USB* position = USB HID, anything else = radio (the link keys are ignored on USB).
The radio's last channel is stored in the nRF52 itself. Battery %, charge state and link state are polled every 200 ms (`rf_t.battery/charge/state`).

## Sleep

`Auto sleep` (F3/F5: `[on, level-1 min, level-2 min, reserved]`, default `01 06 18 04`) is implemented: level 1 blanks all lighting, level 2 also cuts the LED
power rails; any key wakes the board and still reaches the host. Configure it from the **Sleep** card in Air Deck's Advanced tab (it uses the same F5 write
nuphy.io does — verified on the V3: write → read back → restore).

## Button, mouse keys, macro recording, persistence

- **Top-right button.** The V2 has a button where the V3 has a knob. It reports this the way the V3 stores the choice: byte 168 of the app-define blob
  (`0xFB` read / `0xFC` write, size `0xFA` = `03 BA`) is `1` = button, `0` = knob; the V2 defaults to `1`. The button is keymap slot 15 (the V2's Del key),
  its LED is index 83 of the live LED list (where the V3 reports its knob LED), and Air Deck draws it in the top-right corner of the board.
- **Mouse keys.** Keymap entries `0x00D1` left, `D2` right, `D3` middle, `D4` back, `D5` forward, `D9/DA` scroll up/down, `DB/DC` scroll left/right
  (captured from nuphy.io). USB report 4 is a 5-button mouse with wheel + pan; held scroll keys repeat a detent every 70 ms. Over the radio they use `E0`.
- **Macro recording.** `0xC1` write `01` = stop typing into the OS and send every key event to the app as an unframed report `[C1, row, col, pressed]`
  (slot = row*16 + col); `C1` write `00` ends it. The board never stays muted: it self-cancels after 5 minutes without events or when USB is unplugged.
- **Everything is saved** in flash with wear levelling (`src/core/wlstore.c`, the same idea as QMK's wear_leveling driver): keymap, macros, tap dance,
  toggle, SOCD, lighting + brightness for both profiles, key-function/sleep settings and the app-define blob. The top 32 KB of flash is two 16 KB banks;
  a bank is one full snapshot followed by an append-only log of tiny update records (a change = a few halfword writes, no erase). When the log fills
  (~hundreds of changes) the other bank is erased and a fresh snapshot written, so a page is erased once per few hundred changes instead of three times
  per change. Records are CRC'd and the snapshot's magic is written last, so a power cut at any point leaves the old or the new state (`test/wl_test.c`
  cuts power at every program/erase step). Changes are saved ~0.4 s after the last one. Air Deck's Advanced tab shows the store's health (command `0xF8`).
  Flashing this version starts from defaults once: the store moved from `0x0801D000` to `0x08018000`.

## FN keys (on-keyboard controls)

The V2 runs the V3 keymap's own FN layers, so these work out of the box and can be remapped in Air Deck / nuphy.io (codes in `src/core/fnkeys.h`):

| Keys | Action |
|---|---|
| FN+1 / 2 / 3 / 4 | Bluetooth 1-3 / 2.4 GHz dongle (tap = switch, hold = pair) |
| FN+↑ / ↓ | back-light brightness ±10 % |
| FN+← | next back-light effect (1-20) · FN+→ rotates the hue (switches to custom colour) |
| FN+, / FN+. | back-light speed down / up |
| **FN+\\** | **battery on the left side light** (five LEDs: red ≤20 %, orange, yellow, purple, green >80 %; breathing while charging; blinks under 10 %). The right side light stays under software control. Press again to hand it back |
| FN+P | caps-lock indicator on/off |
| FN+] | auto sleep on/off |
| FN+[ (hold 3 s) | factory reset (settings and radio pairings) |
| FN+C (hold) | RGB test: all LEDs red → green → blue → white |
| FN2 layer (hold FN+M): ↑ ↓ ← → , . / | side light brightness ± / next effect / hue / speed ∓ / on-off |

Not ported: the QMK-only debounce / USB-sleep / deep-sleep / sleep-timeout hotkeys, which have no code in the V3 protocol (sleep minutes are set from Air Deck instead).

## Shortcut keys (0x7Exx)

Implemented in `src/core/keyeng.c` as combos / media / system usages, held while the key is held: Mission Control (`7E06`, consumer 0x029F), Spotlight
(`7E07`, Cmd+Space), dictation (`7E08`, consumer 0xCF), Launchpad (`7E09`, consumer 0x2A0), screenshot / area (`7E0A` / `7E0B`), Do Not Disturb (`7E16`, system
usage 0x9B - the system-control report is now one 16-bit usage), Mac screenshot-record, lock screens, Finder search, force quit, emoji, desktop left/right,
Windows task manager / control centre / show desktop / dictation (Win+H), language switch. Not done: Globe key, F-row reverse, touch bar, Flexo's long-press Shift.

## nuphy.io compatibility note (top-right button)

nuphy.io only lets you select / bind the top-right key when the *factory default* it reads from the keyboard (command `B1`) at slot 60 is volume-mute,
as on the V3. The V2 therefore reports `0x00A8` as the default for slot 60 on every layer, while its real initial binding stays Delete
(so "reset" in nuphy.io puts mute there). Found with `tools/harness` (nuphy.io replayed against this code).
