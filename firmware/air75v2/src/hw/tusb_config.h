#pragma once
#define CFG_TUSB_MCU            OPT_MCU_STM32F0
#define CFG_TUSB_OS             OPT_OS_NONE
#define CFG_TUSB_DEBUG          0
#define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#define CFG_TUD_ENABLED         1
#define CFG_TUD_MAX_SPEED       OPT_MODE_FULL_SPEED
#define CFG_TUD_ENDPOINT0_SIZE  64
#define CFG_TUD_HID             2          /* 0 = keyboard + media + system, 1 = V3-protocol config channel */
#define CFG_TUD_CDC             0
#define CFG_TUD_MSC             0
#define CFG_TUD_MIDI            0
#define CFG_TUD_VENDOR          0
#define CFG_TUD_HID_EP_BUFSIZE  64
