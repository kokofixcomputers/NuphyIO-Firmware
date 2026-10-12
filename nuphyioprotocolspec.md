# NuPhy Proprietary Protocol (V3) - Technical Specification

## Document Information
- **Version**: 1.0
- **Date**: 2026-10-12
- **Source**: Reverse-engineered from NuPhy Air75 V3 firmware
- **Compatibility**: NuPhy Air75 V3, Air75 V2 (open firmware)

---

## Table of Contents
1. [Overview](#overview)
2. [Transport Layer](#transport-layer)
3. [Frame Format](#frame-format)
4. [Session Management](#session-management)
5. [Command Set](#command-set)
6. [Memory Regions](#memory-regions)
7. [Data Structures](#data-structures)
8. [Configuration Parameters](#configuration-parameters)
9. [Lighting System](#lighting-system)
10. [Key Mapping](#key-mapping)
11. [Advanced Features](#advanced-features)
12. [Error Handling](#error-handling)
13. [Implementation Notes](#implementation-notes)

---

## Overview

The NuPhy Proprietary Protocol (referred to as "NPH" or "V3 protocol") is a binary protocol used for configuration and control of NuPhy keyboards, particularly the Air75 V3. The protocol operates over USB HID interface 1 with 64-byte reports.

### Key Features
- **Bidirectional communication**: Host-to-device and device-to-host
- **Session-based encryption**: XOR-based obfuscation with session key
- **Memory-mapped configuration**: All settings stored as readable/writable regions
- **64-byte fixed frame size**: Consistent packet format
- **Checksum validation**: Simple 8-bit checksum for data integrity

### Protocol Versions
- **V3 Protocol**: Used by Air75 V3 (primary focus of this document)
- **V2 Protocol**: Legacy, referenced but not detailed here

---

## Transport Layer

### USB Configuration

```
Vendor ID (VID):  0x19F5 (NuPhy)
Product ID (PID): 0x1028 (Air75 V3 default)
                 0x3246 (Air75 V2 alternative)

Interfaces:
  Interface 0: Keyboard HID
    - Report 1: Keyboard (6KRO boot protocol)
    - Report 2: Consumer Control
    - Report 3: System Control
  
  Interface 1: Configuration Channel
    - Usage Page: 0x01 (Generic Desktop)
    - Usage: 0x00 (Undefined)
    - Report Size: 64 bytes (in/out)
    - Report ID: None (raw reports)
```

### Report Descriptors

#### Configuration Interface Descriptor (Byte-for-byte from Air75 V3):
```c
0x06, 0x01, 0x00,       // Usage Page (Generic Desktop)
0x09, 0x00,           // Usage (Undefined)
0xA1, 0x01,           // Collection (Application)
0x09, 0x02,           //   Usage (Mouse)
0x15, 0x00,           //   Logical Minimum (0)
0x26, 0xFF, 0x00,     //   Logical Maximum (255)
0x95, 0x40,           //   Report Size (64 bytes)
0x75, 0x08,           //   Report Count (1)
0x81, 0x02,           //   Input (Data,Var,Abs)
0x09, 0x02,           //   Usage (Mouse)
0x15, 0x00,           //   Logical Minimum (0)
0x26, 0xFF, 0x00,     //   Logical Maximum (255)
0x95, 0x40,           //   Report Size (64 bytes)
0x75, 0x08,           //   Report Count (1)
0x91, 0x02,           //   Output (Data,Var,Abs)
0xC0                  // End Collection
```

---

## Frame Format

### Physical Frame Structure (64 bytes)

```
Offset  Field        Size    Description
------  -----------  ------  -----------
0       Magic        1 byte  0x55 (host->dev) / 0xAA (dev->host)
1       Command      1 byte  Command ID
2       Reserved     1 byte  Always 0
3       Checksum     1 byte  Sum of bytes [4..63] mod 256
4       Length       1 byte  Data length (XOR with session key)
5       Address Low  1 byte  Memory address low byte (XOR with session key)
6       Address High 1 byte  Memory address high byte (XOR with session key)
7       Handle       1 byte  Context/handle (XOR with session key)
8-63    Data         56 bytes Payload data (XOR with session key if encrypted)
```

### Logical Frame Structure (After Decryption)

```
Offset  Field        Size    Description
------  -----------  ------  -----------
0       Magic        1 byte  0x55 / 0xAA
1       Command      1 byte  Command ID (cleartext)
2       Reserved     1 byte  0
3       Checksum     1 byte  Computed from decrypted bytes [4..63]
4       Length       1 byte  Actual payload length (0-56)
5       Address Low  1 byte  Memory region offset (low)
6       Address High 1 byte  Memory region offset (high)
7       Handle       1 byte  Profile/channel selector
8-63    Data         N bytes Actual payload (N = Length)
```

### Frame Constants

```c
// Frame constants
#define NPH_FRAME        64      // Total frame size
#define NPH_DATA_OFF     8      // Data offset in frame
#define NPH_MAX_DATA     56     // Maximum payload size
```

---

## Session Management

### Handshake Protocol

The protocol requires a session key to be established before any configuration commands can be executed.

**Handshake Sequence:**

1. **Host** sends `NPH_SET_SECRET` (0xEE) command with session key in payload
2. **Device** echoes the payload XORed with the session key
3. All subsequent commands use this session key for encryption

**Session Key Selection:**
- Default: 170 (0xAA) if payload[20] is 0
- Custom: payload[20] from handshake command
- Stored in: `nph_t.session_key` (1 byte)

### Encryption/Decryption

All fields from offset 4 onward are XORed with the session key `K`:

```c
// Encryption (host -> device)
out[4] = in[4] ^ K;
out[5] = in[5] ^ K;
out[6] = in[6] ^ K;
out[7] = in[7] ^ K;
for (i = 8; i < 64; i++) out[i] = in[i] ^ K;

// Decryption (device receives)
len     = in[4] ^ K;
addr    = ((in[6] ^ K) << 8) | (in[5] ^ K);
handle  = in[7] ^ K;
data[i] = in[8+i] ^ K;
```

### Checksum Calculation

```c
uint8_t nph_checksum(const uint8_t f[NPH_FRAME]) {
    uint8_t s = 0;
    for (int i = 4; i < NPH_FRAME; i++) s += f[i];
    return s;
}
```

**Note**: Checksum is computed on the **encrypted** frame bytes [4..63].

---

## Command Set

### Command Categories

#### System Commands (0xA0-0xBF)
| Command | Value | Direction | Description |
|---------|-------|-----------|-------------|
| NPH_GET_BASE | 0xA0 | Dev→Host | Get base device information |
| NPH_GET_FIRMWARE | 0xA1 | Dev→Host | Get firmware version |

#### Keymap Commands (0xB0-0xBF)
| Command | Value | Direction | Description |
|---------|-------|-----------|-------------|
| NPH_GET_DEFAULT_KEYS | 0xB1 | Dev→Host | Get default keymap |
| NPH_GET_KEYS | 0xB2 | Dev→Host | Get current keymap |
| NPH_SET_KEYS | 0xB3 | Host→Dev | Set current keymap |
| NPH_RESTORE_KEYS | 0xB4 | Host→Dev | Restore default keymap |

#### Special Key Commands (0xB5-0xBF)
| Command | Value | Direction | Description |
|---------|-------|-----------|-------------|
| NPH_GET_SOCD | 0xB5 | Dev→Host | Get SOCD (Same-Key Combined) config |
| NPH_SET_SOCD | 0xB6 | Host→Dev | Set SOCD config |
| NPH_RESET_SOCD | 0xB7 | Host→Dev | Reset SOCD to defaults |
| NPH_GET_TAPDANCE | 0xB8 | Dev→Host | Get Tap Dance config |
| NPH_SET_TAPDANCE | 0xB9 | Host→Dev | Set Tap Dance config |
| NPH_RESET_TAPDANCE | 0xBA | Host→Dev | Reset Tap Dance |
| NPH_GET_TGL | 0xBB | Dev→Host | Get Toggle config |
| NPH_SET_TGL | 0xBC | Host→Dev | Set Toggle config |
| NPH_RESET_TGL | 0xBD | Host→Dev | Reset Toggle config |

#### Macro Commands (0xC0-0xCF)
| Command | Value | Direction | Description |
|---------|-------|-----------|-------------|
| NPH_SET_KEYUPLOAD | 0xC1 | Host→Dev | Set key upload mode |
| NPH_GET_MACRO | 0xC2 | Dev→Host | Get macro data |
| NPH_SET_MACRO | 0xC3 | Host→Dev | Set macro data |

#### Lighting Commands (0xD0-0xDF)
| Command | Value | Direction | Description |
|---------|-------|-----------|-------------|
| NPH_GET_LIGHT_COUNT | 0xD1 | Dev→Host | Get LED count (returns 104) |
| NPH_GET_LED_COLOR | 0xD2 | Dev→Host | Get individual LED color (requires led_read hook) |
| NPH_GET_LIGHT | 0xD5 | Dev→Host | Get lighting configuration |
| NPH_SET_LIGHT | 0xD6 | Host→Dev | Set lighting configuration |

#### Keyboard Function Commands (0xE0-0xEF)
| Command | Value | Direction | Description |
|---------|-------|-----------|-------------|
| NPH_GET_KBFUNC | 0xE1 | Dev→Host | Get keyboard function config |
| NPH_SET_KBFUNC | 0xE2 | Host→Dev | Set keyboard function config |
| NPH_SET_DEBOUNCE | 0xE3 | Host→Dev | Set debounce settings |

#### System Control Commands (0xF0-0xFF)
| Command | Value | Direction | Description |
|---------|-------|-----------|-------------|
| NPH_RESTORE_FACTORY | 0xF1 | Host→Dev | Factory reset |
| NPH_GET_SLEEP | 0xF3 | Dev→Host | Get sleep configuration |
| NPH_SET_SLEEP | 0xF5 | Host→Dev | Set sleep configuration |
| NPH_GET_APPDEF_SIZE | 0xFA | Dev→Host | Get app-defined data size |
| NPH_GET_APPDEF | 0xFB | Dev→Host | Get app-defined data |
| NPH_SET_APPDEF | 0xFC | Host→Dev | Set app-defined data |
| NPH_SET_IAP | 0xEF | Host→Dev | Enter DFU mode (firmware update) |

#### Session Command
| Command | Value | Direction | Description |
|---------|-------|-----------|-------------|
| NPH_SET_SECRET | 0xEE | Host→Dev | Establish session key (handshake) |

---

## Memory Regions

All configuration data is organized into memory-mapped regions that can be read or written using the appropriate commands.

### Region Definitions

```c
typedef struct {
    uint8_t *data;       // Pointer to region data
    uint16_t size;       // Region size in bytes
    bool rw;             // Read/Write flag
    uint8_t dirty;       // Dirty flag for persistence
} region_t;
```

### Region Mapping

| Region | Command (Get) | Command (Set) | Size | Description |
|--------|---------------|---------------|------|-------------|
| Keymap | NPH_GET_KEYS | NPH_SET_KEYS | 1568 bytes | 8 layers × 98 slots × 2 bytes |
| Macro | NPH_GET_MACRO | NPH_SET_MACRO | 2048 bytes | Macro storage |
| Light | NPH_GET_LIGHT | NPH_SET_LIGHT | 17 bytes | Lighting config per profile |
| Tap Dance | NPH_GET_TAPDANCE | NPH_SET_TAPDANCE | 512 bytes | Tap dance configurations |
| Toggle | NPH_GET_TGL | NPH_SET_TGL | 128 bytes | Toggle key configurations |
| SOCD | NPH_GET_SOCD | NPH_SET_SOCD | 256 bytes | Same-key combined settings |
| KBFunc | NPH_GET_KBFUNC | NPH_SET_KBFUNC | 4 bytes | Keyboard function settings |
| Sleep | NPH_GET_SLEEP | NPH_SET_SLEEP | 4 bytes | Sleep configuration |
| Firmware | NPH_GET_FIRMWARE | N/A | 8 bytes | Firmware version (read-only) |

---

## Data Structures

### Main Protocol State

```c
typedef struct {
    uint8_t keymap[NPH_KEYMAP_BYTES];      // 1568 bytes (8 layers × 98 slots × 2)
    uint8_t macro[NPH_MACRO_BYTES];        // 2048 bytes
    uint8_t light[2][NPH_LIGHT_BYTES];    // 2 profiles (Mac/Windows) × 17 bytes
    uint8_t td[NPH_TD_SLOTS * 8];          // 64 slots × 8 bytes = 512 bytes
    uint8_t tgl[NPH_TGL_SLOTS * 2];        // 64 slots × 2 bytes = 128 bytes
    uint8_t socd[NPH_SOCD_SLOTS * 4];      // 64 slots × 4 bytes = 256 bytes
    uint8_t kbfunc[4];                     // Keyboard function config
    uint8_t sleepcfg[4];                  // Sleep configuration
    uint8_t firmware[8];                   // Firmware version info
    uint8_t session_key;                   // Current session key
    bool    have_key;                      // Session established flag
    uint8_t dirty;                         // Dirty flags (bitmask)
    nph_hooks_t hooks;                    // Platform-specific hooks
} nph_t;
```

### Dirty Flags

```c
#define NPH_D_KEYMAP  1    // Keymap modified
#define NPH_D_MACRO   2    // Macros modified
#define NPH_D_LIGHT   4    // Lighting modified
#define NPH_D_TD      8    // Tap Dance modified
#define NPH_D_TGL     16   // Toggle modified
#define NPH_D_SOCD    32   // SOCD modified
#define NPH_D_MISC    64   // Miscellaneous (kbfunc, sleep) modified
```

### Platform Hooks

```c
typedef struct {
    void (*led_read)(uint16_t byte_off, uint8_t *dst, uint8_t len);  // Read live LED colors
    void (*enter_dfu)(void);                                   // Enter DFU mode
    void (*factory_reset)(void);                              // Factory reset
    uint8_t (*current_mode)(void);                            // Get OS mode (0=Mac, 1=Windows)
} nph_hooks_t;
```

---

## Configuration Parameters

### Keymap Layout

- **Layers**: 8 total
  - Layers 0-3: Mac mode
  - Layers 4-7: Windows mode
- **Slots per layer**: 98
  - Slots 0-95: Standard keys
  - Slots 96-97: Knob turns (rotary encoder)
- **Keymap entry format**: Big-endian 16-bit values
- **Total keymap size**: 1568 bytes (8 × 98 × 2)

**Keycode Encoding:**
- Standard HID usages: 0x04-0x29, 0x2B-0x53, etc.
- Modifiers: 0xE0-0xE7 (Left Control, Left Shift, Left Alt, Left GUI, Right Control, Right Shift, Right Alt, Right GUI)
- Media keys: 0x7Exx (Firmware functions: radio, brightness, etc.)
- Special keys: 0x78xx (Special functions)
- Empty/transparent: 0x0001

### Macro Configuration

- **Total macro storage**: 2048 bytes
- **Number of macros**: 32 (empty macros start at address 0x40)
- **Macro format**: Event stream (see keyeng for interpretation)
- **Empty macro marker**: Address 0x40 with length 0

---

## Lighting System

### LED Configuration

- **Key LEDs**: 84 physical LEDs (BD_KEY_LEDS)
- **Side LEDs**: 12 physical LEDs (BD_SIDE_LEDS)
- **V3 expected count**: 104 (84 key + 20 side) - the device synthesizes 20 side entries from the 12 physical LEDs
- **Light data per profile**: 17 bytes

### Light Data Structure (17 bytes per profile)

```
Backlight Configuration (9 bytes):
  [0]  effect_id       // Lighting effect ID (0x00-0x15)
  [1]  brightness      // Brightness percentage (0-100)
  [2]  speedRaw        // Raw speed value (gear = 4 - speedRaw)
  [3]  reserved1       // Reserved
  [4]  auto_color      // 0 = manual color, 1 = automatic color
  [5]  reserved2       // Reserved
  [6]  R              // Red component (0-255)
  [7]  G              // Green component (0-255)
  [8]  B              // Blue component (0-255)

Side Light Configuration (8 bytes):
  [9]   effect_id      // Side lighting effect ID
  [10]  brightness     // Brightness percentage (0-100)
  [11]  speedRaw       // Raw speed value
  [12]  auto          // Automatic mode flag
  [13]  reserved3      // Reserved
  [14]  R             // Red component
  [15]  G             // Green component
  [16]  B             // Blue component
```

### Default Light Configuration

```c
// Backlight: Diagonal effect, 80% brightness, gear 2, automatic color (white)
{0x14, 0x50, 0x02, 0x00, 0x01, 0x00, 0xFF, 0xFF, 0xFF,
 // Side: Time effect, 80% brightness, gear 2, automatic color (red)
 0x00, 0x50, 0x02, 0x01, 0x00, 0xFF, 0x00, 0x00}
```

### Lighting Effects

Based on the effect_data.h file, the following effects are defined:

| Effect ID (Hex) | Name | Description |
|----------------|------|-------------|
| 0x00 | Static | Solid color, no animation |
| 0x01 | Diagonal | Diagonal wipe pattern |
| 0x02 | Time | Time-based color animation |
| 0x06 | Reverse Wave | Reverse wave animation |
| 0x07 | Wipe | Single-direction wipe |
| 0x08 | Radial | Radial gradient from center |
| 0x09 | Radial Reverse | Reverse radial gradient |
| 0x0A | Custom | Custom effect (format TBD) |
| 0x0E | Gaming | Gaming mode - highlights specific keys |
| 0x0F | Identify | Identification mode - highlights key positions |
| 0x10 | Side Static | Static color for side LEDs |
| 0x11 | Side Neon | Neon effect for side LEDs |
| 0x12 | Side Time | Time-based for side LEDs |
| 0x13 | Side Wipe | Wipe effect for side LEDs |
| 0x14 | Side Wave | Wave effect for side LEDs |
| 0x15 | Side Custom | Custom side LED effect |

### Gear Ratios

The protocol supports 5 gear levels for speed control:

```c
static const float FX_GEAR_RATIO[5] = {
    0.0312f,  // Gear 0: Very slow
    0.5003f,  // Gear 1: Slow
    1.0000f,  // Gear 2: Normal (default)
    1.4997f,  // Gear 3: Fast
    2.0009f   // Gear 4: Very fast
};
```

**Conversion**: `speedRaw = 4 - gear` (in default configuration)

---

## Key Mapping

### Physical Layout

- **Matrix dimensions**: 6 rows × 17 columns
- **Scanning method**: Rows driven active low one at a time, columns read with pull-ups

### Pin Mapping

From board_data.h (generated from QMK):

```c
// Row pins: {port (A=0,B=1,C=2), pin}
static const uint8_t BD_ROW_PIN[6][2] = {
    {2,14}, {2,15}, {0,0}, {0,1}, {0,2}, {0,3}
};

// Column pins: {port, pin}
static const uint8_t BD_COL_PIN[17][2] = {
    {0,4}, {0,5}, {0,6}, {1,9}, {1,0}, {1,1}, {1,10}, {1,11}, {1,12}, {1,13}, 
    {1,14}, {1,15}, {0,8}, {0,9}, {0,10}, {0,15}, {1,3}
};
```

### Slot Mapping

The physical matrix is mapped to V3 protocol slots (0-97):

```c
// matrix (row, col) -> V3 keymap slot
// 0xFF = no key at this position
static const uint8_t BD_SLOT[6][17] = {
    // Row 0
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0xFF, 0x0F, 0x0E, 0xFF},
    // Row 1
    {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0xFF, 0x3F, 0x1F},
    // Row 2
    {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x0D, 0x4F, 0x2F},
    // Row 3
    {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0xFF, 0x3D, 0x5C, 0xFF, 0xFF},
    // Row 4
    {0x40, 0xFF, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0xFF, 0x4C, 0x4E, 0xFF, 0xFF},
    // Row 5
    {0x50, 0x51, 0x52, 0xFF, 0xFF, 0xFF, 0x56, 0xFF, 0xFF, 0x5A, 0x5B, 0xFF, 0xFF, 0x5D, 0x5E, 0x5F, 0xFF},
};
```

**Slot numbering**: Uses V3 layout where slot = row * 16 + col style numbering.

### LED to Slot Mapping

LED chain index (wiring order) maps to V3 slots:

```c
static const uint8_t BD_LED_SLOT[84] = {
    // Row 0 LEDs (left to right)
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    // Row 1 LEDs (right to left)
    31, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
    // Row 2 LEDs (left to right)
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
    // Row 3 LEDs
    47, 63, 61, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48,
    // Row 4 LEDs
    64, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 78, 79,
    // Row 5 LEDs
    95, 94, 93, 92, 91, 90, 86, 82, 81, 80
};
```

### LED Physical Positions

Each LED has X,Y coordinates in QMK's coordinate system (0-224 × 0-64 units):

```c
// X coordinates (0-224)
static const uint8_t BD_LED_X[84] = {
    0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150,  // Row 0
    150, 130, 120, 110, 100, 90, 80, 70, 60, 50, 40, 30, 20, 10, 0,     // Row 1
    0, 15, 25, 35, 45, 55, 65, 75, 85, 95, 105, 115, 125, 135,          // Row 2
    150, 150, 128, 118, 108, 98, 88, 78, 68, 58, 48, 38, 28, 18, 0,    // Row 3
    0, 23, 33, 43, 53, 63, 73, 83, 93, 103, 113, 123, 140, 150,        // Row 4
    150, 140, 130, 120, 110, 100, 38, 25, 13, 0                     // Row 5
};

// Y coordinates (0-64)
static const uint8_t BD_LED_Y[84] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                // Row 0: Y=0
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,   // Row 1: Y=10
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,      // Row 2: Y=20
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,      // Row 3: Y=30
    40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,      // Row 4: Y=40
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50                    // Row 5: Y=50
};
```

---

## Advanced Features

### Tap Dance

- **Number of slots**: 64 (NPH_TD_SLOTS)
- **Data per slot**: 8 bytes
- **Total storage**: 512 bytes
- **Default**: All 0xFF (disabled)
- **Purpose**: Allows a single key to perform different actions based on tap timing
- **Format**: TBD (stored as raw bytes, interpreted by keyeng)

### Toggle (TGL)

- **Number of slots**: 64 (NPH_TGL_SLOTS)
- **Data per slot**: 2 bytes
- **Total storage**: 128 bytes
- **Default**: All 0xFF (disabled)
- **Purpose**: Toggle keys that maintain state across presses
- **Format**: TBD

### SOCD (Same-Key Combined / Same-Key Combination Definition)

- **Number of slots**: 64 (NPH_SOCD_SLOTS)
- **Data per slot**: 4 bytes
- **Total storage**: 256 bytes
- **Default**: All 0xFF (disabled)
- **Purpose**: Defines behavior when same key is pressed on both sides of the keyboard
- **Format**: TBD

### Keyboard Functions (KBFunc)

- **Size**: 4 bytes
- **Default**: `{0x02, 0x00, 0x00, 0x00}`
- **Purpose**: Various keyboard-level configuration options
- **Format**: TBD (likely bit flags for various features)

### Sleep Configuration

- **Size**: 4 bytes
- **Default**: `{0x01, 0x06, 0x18, 0x04}`
- **Format**:
  ```
  [0] enabled:    0 = disabled, 1 = enabled
  [1] level1_min: Minutes before level 1 sleep (blank lighting)
  [2] level2_min: Minutes before level 2 sleep (cut LED power)
  [3] reserved
  ```
- **Behavior**:
  - Level 1: Blank lighting after N minutes of inactivity
  - Level 2: Cut LED power rails after M minutes of inactivity
  - Any key press wakes the board and restores full functionality

### Firmware Version

- **Size**: 8 bytes
- **Default**: `{0x0E, 0x00, 0x01, 0xAA, 0x06, 0x00, 0x00, 0x00}`
- **Format**:
  ```
  [0] patch:     Patch version (0x0E = 14)
  [1] minor:     Minor version (0x00 = 0)
  [2] major:     Major version (0x01 = 1)
  [3] marker:    0xAA (firmware marker)
  [4] build_lo:  Build number low byte (0x06 = 6)
  [5] build_hi:  Build number high byte (0x00 = 0)
  [6-7] reserved
  ```
- **Interpreted as**: Version 1.0.14.6

### Base Device Information

- **Size**: 8 bytes
- **Default**: `{0x00, 0x08, 0x04, 0x06, 0x10, 0x82, 0x00, 0x00}`
- **Format**:
  ```
  [0] active_profile: 0 = Mac, 1 = Windows (from OS slide switch)
  [1-7] identity:    V3's identity block (kept verbatim)
  ```

---

## Error Handling

### Frame Validation

The protocol performs the following validations on received frames:

1. **Magic byte check**: Frame[0] must equal 0x55 (host-to-device)
   - If not, frame is silently discarded
2. **Checksum verification**: Computed checksum must match frame[3]
   - If not, frame is silently discarded
3. **Session check**: For non-session commands, `have_key` must be true
   - If not, frame is silently discarded
4. **Command validation**: Command must be recognized
   - If not recognized, frame is silently discarded

### Response Generation

- **On success**: Device always generates a response frame
  - Response magic byte: 0xAA (device-to-host)
  - Echoes request command ID
  - Echoes request header (length, address, handle)
  - Data field contains requested/acknowledged data
  - Checksum computed on encrypted response
- **On error**: Device stays silent (no response)

### Special Commands Behavior

- **Handshake (NPH_SET_SECRET)**: Always responds, establishes session
- **Write commands**: Echo the data as sent (encrypted), mark region dirty
- **Read commands**: Return requested data (encrypted)
- **Action commands** (factory reset, DFU): Perform action, may not return data

---

## Implementation Notes

### Protocol Initialization

```c
void nph_init(nph_t *s, const nph_hooks_t *hooks) {
    memset(s, 0, sizeof *s);
    if (hooks) s->hooks = *hooks;
    
    // Set firmware version to match V3
    const uint8_t fw[8] = {0x0E, 0x00, 0x01, 0xAA, 0x06, 0x00, 0x00, 0x00};
    memcpy(s->firmware, fw, 8);
    
    // Load default configuration
    nph_load_defaults(s);
}
```

### Default Values Loading

```c
void nph_load_defaults(nph_t *s) {
    // Load default keymap from board data
    memcpy(s->keymap, BD_DEFAULT_KEYMAP, NPH_KEYMAP_BYTES);
    
    // Initialize macro storage
    memset(s->macro, 0, sizeof s->macro);
    
    // Initialize 32 empty macros at address 0x40
    for (int i = 0; i < 32; i++) {
        s->macro[i * 2] = 0x40;
        s->macro[i * 2 + 1] = 0x00;
    }
    
    // Load default lighting for both profiles
    memcpy(s->light[0], DEFAULT_LIGHT, NPH_LIGHT_BYTES);
    memcpy(s->light[1], DEFAULT_LIGHT, NPH_LIGHT_BYTES);
    
    // Initialize advanced features to disabled (0xFF)
    memset(s->td, 0xFF, sizeof s->td);
    memset(s->tgl, 0xFF, sizeof s->tgl);
    memset(s->socd, 0xFF, sizeof s->socd);
    
    // Set keyboard function and sleep defaults
    const uint8_t kbfunc[4] = {0x02, 0x00, 0x00, 0x00};
    const uint8_t sleepcfg[4] = {0x01, 0x06, 0x18, 0x04};
    memcpy(s->kbfunc, kbfunc, 4);
    memcpy(s->sleepcfg, sleepcfg, 4);
}
```

### Command Processing Flow

```
1. Receive 64-byte frame from USB HID interface 1

2. Validate frame:
   - Check magic byte == 0x55
   - Verify checksum matches
   
3. If command is NPH_SET_SECRET (0xEE):
   - Extract session key K from payload[20] or default to 170
   - Store K in nph_t.session_key
   - Set nph_t.have_key = true
   - Generate response: echo payload XOR K
   - Compute and set checksum
   - Return true (response ready)

4. If session not established (have_key == false):
   - Return false (no response)
   
5. Decrypt header fields:
   - len = in[4] ^ K
   - addr = ((in[6] ^ K) << 8) | (in[5] ^ K)
   - handle = in[7] ^ K
   
6. Determine if write command using is_write() lookup

7. If write command:
   - Find region using region_for()
   - Validate region is writable
   - Write data[0..len-1] to region at address addr
   - Mark region dirty
   - Echo the data as sent in response
   
8. If read command:
   - Switch on command ID
   - For memory regions: Read from region, copy to plain buffer
   - For special commands: Generate appropriate response data
   - XOR plain buffer with session key
   
9. Compute checksum on response frame

10. Return true (response ready for transmission)
```

### Region Access

```c
static bool region_for(nph_t *s, uint8_t cmd, uint8_t handle, region_t *r) {
    switch (cmd) {
        case NPH_GET_KEYS:    case NPH_SET_KEYS:       
            *r = (region_t){ s->keymap, NPH_KEYMAP_BYTES, true, NPH_D_KEYMAP }; return true;
        case NPH_GET_MACRO:   case NPH_SET_MACRO:      
            *r = (region_t){ s->macro, NPH_MACRO_BYTES, true, NPH_D_MACRO }; return true;
        case NPH_GET_LIGHT:   case NPH_SET_LIGHT:      
            *r = (region_t){ s->light[handle & 1], NPH_LIGHT_BYTES, true, NPH_D_LIGHT }; return true;
        case NPH_GET_TAPDANCE: case NPH_SET_TAPDANCE:  
            *r = (region_t){ s->td, sizeof s->td, true, NPH_D_TD }; return true;
        case NPH_GET_TGL:     case NPH_SET_TGL:        
            *r = (region_t){ s->tgl, sizeof s->tgl, true, NPH_D_TGL }; return true;
        case NPH_GET_SOCD:    case NPH_SET_SOCD:      
            *r = (region_t){ s->socd, sizeof s->socd, true, NPH_D_SOCD }; return true;
        case NPH_GET_KBFUNC:  case NPH_SET_KBFUNC:    
            *r = (region_t){ s->kbfunc, sizeof s->kbfunc, true, NPH_D_MISC }; return true;
        case NPH_GET_SLEEP:   case NPH_SET_SLEEP:      
            *r = (region_t){ s->sleepcfg, sizeof s->sleepcfg, true, NPH_D_MISC }; return true;
        case NPH_GET_FIRMWARE:                               
            *r = (region_t){ s->firmware, sizeof s->firmware, false, 0 }; return true;
        case NPH_GET_DEFAULT_KEYS:                         
            *r = (region_t){ (uint8_t *)BD_DEFAULT_KEYMAP, NPH_KEYMAP_BYTES, false, 0 }; return true;
        default: return false;
    }
}
```

### USB Integration

The protocol integrates with TinyUSB:

- **Interface**: HID interface 1 (configuration channel)
- **Report size**: 64 bytes (no report ID)
- **Queue depth**: 8 frames for response buffering
- **Transmission**: Responses are queued and sent when endpoint is free

**USB Callbacks:**

```c
// Called when host sends a configuration report
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, 
                            hid_report_type_t report_type, 
                            uint8_t const *buffer, uint16_t bufsize) {
    if (instance == 0) {
        // Handle keyboard LED reports
        if (report_type == HID_REPORT_TYPE_OUTPUT && bufsize)
            leds_from_host = buffer[bufsize - 1];
        return;
    }
    
    // Instance 1: Configuration channel
    uint8_t in[NPH_FRAME] = {0};
    memcpy(in, buffer, bufsize > NPH_FRAME ? NPH_FRAME : bufsize);
    uint8_t *out = q[q_head % Q_DEPTH];
    
    // Process frame and queue response
    if ((uint8_t)(q_head - q_tail) < Q_DEPTH && nph_handle(proto, in, out))
        q_head++;
}
```

### Firmware Update (IAP Mode)

- **Command**: NPH_SET_IAP (0xEF)
- **Action**: Calls `enter_dfu()` platform hook
- **Behavior**: Device enters ROM bootloader mode
- **Requirements**: Platform must provide `enter_dfu()` implementation

### Factory Reset

- **Command**: NPH_RESTORE_FACTORY (0xF1)
- **Action**: Calls `factory_reset()` platform hook
- **Behavior**: 
  - Loads default configuration
  - Sets all dirty flags
  - Saves to persistent storage
- **Note**: Can also be triggered by holding Esc during power-up

---

## Protocol Constants Summary

```c
// Frame constants
#define NPH_FRAME        64      // Total frame size in bytes
#define NPH_DATA_OFF     8      // Data offset in frame
#define NPH_MAX_DATA     56     // Maximum payload size

// Layout constants
#define NPH_LAYERS       8      // Number of keymap layers
#define NPH_SLOTS        98     // Slots per layer (V3 layout)
#define NPH_KEYMAP_BYTES (NPH_LAYERS * NPH_SLOTS * 2)  // 1568 bytes
#define NPH_MACRO_BYTES  2048   // Macro storage size
#define NPH_TD_SLOTS     64     // Tap Dance slots
#define NPH_TGL_SLOTS    64     // Toggle slots
#define NPH_SOCD_SLOTS   64     // SOCD slots
#define NPH_LIGHT_BYTES  17     // Lighting config per profile
#define NPH_LED_COUNT_V3 104    // V3 expected LED count

// Physical layout
#define BD_ROWS          6      // Matrix rows
#define BD_COLS          17     // Matrix columns
#define BD_KEY_LEDS      84     // Physical key LEDs
#define BD_SIDE_LEDS     12     // Physical side LEDs

// USB identifiers
#define USB_VID          0x19F5 // NuPhy vendor ID
#define USB_PID_AIR75V3  0x1028 // Air75 V3 product ID
#define USB_PID_AIR75V2  0x3246 // Air75 V2 product ID
```

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-10-12 | Reverse Engineering | Initial specification based on Air75 V2 firmware analysis |

---

## References

1. NuPhy Air75 V2 Open Firmware - Source code analysis
2. USB HID Specification v1.11
3. TinyUSB Documentation
4. Reverse-engineered from official nuphy.io app network captures
5. QMK firmware for Air75 V2 (board_data generation)

---

*This document is based on reverse engineering of the NuPhy proprietary protocol. While every effort has been made to ensure accuracy, some details may be incomplete or subject to change as more information becomes available. The protocol implementation may vary between different NuPhy keyboard models.*
