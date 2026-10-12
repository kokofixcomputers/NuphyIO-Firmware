# NuphyIO Firmware

Firmware built from the ground up to bring NuPhyIO support to NuPhy keyboards that don’t currently support it.

## Why this project?

Some NuPhy keyboards can be configured with VIA, but getting started may require downloading and importing a keyboard-specific JSON file. This project aims to simplify setup by enabling direct configuration through [NuPhyIO](https://www.nuphy.io/).

## Goals

- **Works out of the box:** After installing compatible firmware, connect to NuPhyIO and configure your keyboard—no JSON files or manual configuration imports.
- **Use NuPhyIO directly:** Configure supported keyboards through NuPhy’s web-based tool.
- **Simpler setup:** Reduce the extra steps and troubleshooting involved in getting a keyboard recognized and configured.
- **Reliable model support:** Build and test firmware for each supported keyboard, starting with the NuPhy Air75 V2.

## Extra features

This firmware also aims to add features beyond the standard VIA configuration experience:

- **Sidelight control:** Configure the keyboard’s side lighting.
- **Mouse actions on keys:** Assign keys to mouse functions, including left, right, and middle click, as well as scrolling up, down, left, and right.
- **Configurable sleep behavior:** Get more control over when and how the keyboard enters sleep mode.

Features are under development and may vary by keyboard model.

## Project status

- **NuphyIO protocol reverse engineering:** Complete
- **NuPhy Air75 V2 support:** In progress
- **Additional keyboards:** Planned

## Roadmap

1. Reverse engineer the proprietary NuphyIO protocol. **Complete**
2. Develop and test firmware for supported NuPhy keyboards. **In progress**
3. Expand support to additional models.

> **Note:** This project is under development. Installing custom firmware may carry risks. Follow model-specific instructions and understand the recovery process before flashing.
