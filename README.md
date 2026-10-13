# NuphyIO Firmware

Firmware built from the ground up to bring NuPhyIO support to NuPhy keyboards that don’t currently support it — plus **Air Deck**, a web app that talks the same protocol.

It is not QMK and not a fork of NuPhy's firmware: a small bare-metal firmware (STM32F072, TinyUSB) that speaks the same HID protocol as the Air75 V3, so [NuPhyIO](https://www.nuphy.io/) (and Air Deck) can configure a V2-generation board as if it were one. The protocol was reverse-engineered from the NuPhyIO site and captures of a real V3; see [`nuphyioprotocolspec.md`](nuphyioprotocolspec.md).

> **Unofficial.** Not affiliated with NuPhy. You flash this at your own risk — see [Flashing](#flashing-read-this-first). The chip's ROM bootloader cannot be overwritten, so the keyboard can always be recovered.

## Why this project?

Some NuPhy keyboards can be configured with VIA, but getting started may require downloading and importing a keyboard-specific JSON file. This project aims to simplify setup by enabling direct configuration through [NuPhyIO](https://www.nuphy.io/).

## Goals

- **Works out of the box:** After installing compatible firmware, connect to NuPhyIO and configure your keyboard—no JSON files or manual configuration imports.
- **Use NuPhyIO directly:** Configure supported keyboards through NuPhy’s web-based tool.
- **Simpler setup:** Reduce the extra steps and troubleshooting involved in getting a keyboard recognized and configured.
- **Reliable model support:** Build and test firmware for each supported keyboard.

## What's in the repo

| | |
|---|---|
| [`firmware/air75v2/`](firmware/air75v2) | the firmware (one source tree, two boards) — protocol core, key engine, lighting engine, flash store, nRF52 radio link, host tests |
| [`src/`](src) | **Air Deck**, a React + TypeScript WebHID app: key remapping with a visual board, tap dance / toggle keys, macros (recorded on the keyboard), lighting with a live LED view, sleep, mode settings, several keyboards at once |
| [`tools/`](tools) | the reverse-engineering workbench: recordings of a real V3's LEDs, the scripts that fit them into the firmware's effect tables, and `tools/harness` (replays the real nuphy.io against the firmware's protocol code without a keyboard) |

## Supported keyboards

| Keyboard | Status |
|---|---|
| **NuPhy Air75 V2** | In use. Keys, FN layers, remapping, macros, tap dance, mouse keys, lighting (all 20 back-light effects of the V3 and its 5 side effects), sleep, button, mode settings, persistent settings with wear levelling |
| **NuPhy Halo75 V2** | Builds and passes host tests, **not yet tested on hardware** (the IS31FL3733 LED driver is the riskiest part). The V3's side light drives the 45-LED halo ring |
| Others | Planned |

Radio (Bluetooth 1-3 and the 2.4 GHz dongle via the on-board nRF52) is implemented from NuPhy's UART protocol and host-tested against a simulated radio, but has not been tried on real hardware.

## Extra features

Beyond what the stock configuration offers:

- **Sidelight control:** Configure the keyboard’s side lighting — independent of the back light.
- **Mouse actions on keys:** Assign keys to mouse functions, including left, right, and middle click, as well as scrolling up, down, left, and right.
- **Configurable sleep behavior:** Auto-sleep on/off and both timeouts, stored on the keyboard.
- **FN controls on the board:** Bluetooth 1-3 / dongle channels, brightness, effect, speed and hue, battery on one side light (FN+\), key lock-outs (Disable Win / Alt+F4 / Alt+Tab), RGB test.
- **Macro recording on the keyboard:** the keyboard stops typing and reports keys to the app, exactly like nuphy.io.
- **Key test:** see which key (and which matrix slot) the keyboard reports.

Not done yet: SOCD (stored but not acted on), Globe key, F-row reverse, touch bar.

## Quick start

**Air Deck** (needs Chrome, Edge or Brave — WebHID):

```bash
npm install        # or pnpm install
npm run dev        # http://localhost:5173  (add ?demo for a simulated keyboard)
```

**Firmware** (needs `arm-none-eabi-gcc`; `dfu-util` to flash):

```bash
cd firmware/air75v2
make                   # Air75 V2  -> build/air75v2/air75v2.bin
make BOARD=halo75v2    # Halo75 V2 -> build/halo75v2/halo75v2.bin
make test              # protocol replay of a real V3 capture, key engine, lighting, radio, flash store, board data
make e2e               # Air Deck's real device client <-> the firmware's protocol code
```

## Flashing (read this first)

1. **Back up what is there now**: put the board in DFU (unplug, hold the button under the Caps Lock keycap, plug in) and run `make backup`.
2. `make flash` (or `make BOARD=halo75v2 flash`) writes the firmware to `0x08000000` and resets.
3. To go back: DFU again, then `dfu-util -a 0 -d 0483:df11 -s 0x08000000:leave -D backup-original.bin`.

Recovery if something is wrong: hold **Esc** while plugging in, or the BOOT0 button, to reach the ROM bootloader. More in [`firmware/air75v2/README.md`](firmware/air75v2/README.md) and [`HARDWARE.md`](firmware/air75v2/HARDWARE.md).

## How it works, briefly

- 64-byte HID reports; frames are `[0x55 host / 0xAA device, cmd, 0, checksum, len, addr, handle, data…]` with a per-session XOR key chosen by the host in the handshake. Settings regions (keymap, macros, lighting, tap dance, sleep, …) are read and written by address.
- The firmware reports itself as a V3 (same USB IDs and firmware info), so NuPhyIO accepts it; Air Deck tells the boards apart by the USB product string.
- Lighting replays the V3's effects from tables fitted to recordings of a real V3 (`tools/gen_effects.py`).
- Settings live in a wear-levelled snapshot + log in the top 32 KB of flash and survive power cycles.

## Project status

- **NuphyIO protocol reverse engineering:** Complete for everything the firmware implements
- **NuPhy Air75 V2 support:** Working
- **NuPhy Halo75 V2 support:** Built, awaiting hardware testing
- **Additional keyboards:** Planned

## Licensing

Parts of the firmware are derived from NuPhy's QMK sources (GPL-2.0-or-later) — the radio UART protocol, key ordering and hardware tables were read from them — and TinyUSB (MIT) is vendored under `firmware/air75v2/third_party/`. A licence for this repository has not been chosen yet; given the GPL-derived parts, GPL-2.0-or-later is the likely fit.
