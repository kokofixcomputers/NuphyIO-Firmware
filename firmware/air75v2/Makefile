# NuPhy V2-generation open firmware (bare metal, TinyUSB). Requires arm-none-eabi-gcc; flash with dfu-util.
#   make                       Air75 V2   -> build/air75v2/air75v2.bin
#   make BOARD=halo75v2        Halo75 V2  -> build/halo75v2/halo75v2.bin
BOARD   ?= air75v2
TARGET  := $(BOARD)
BUILD   := build/$(BOARD)
LEDSRC  := $(if $(filter halo75v2,$(BOARD)),src/hw/leds_is31.c,src/hw/leds_ws2812.c)
USB_PID ?= 0x1028
CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size

TUSB := third_party/tinyusb/src
SRCS := src/main.c src/core/nph_proto.c src/core/keyeng.c src/core/light.c src/core/rf.c src/core/wlstore.c src/core/fnkeys.c \
        src/hw/startup.c src/hw/board.c src/hw/matrix.c $(LEDSRC) src/hw/store.c src/hw/rfio.c src/usb/usb.c \
        $(TUSB)/tusb.c $(TUSB)/common/tusb_fifo.c $(TUSB)/device/usbd.c $(TUSB)/device/usbd_control.c \
        $(TUSB)/class/hid/hid_device.c $(TUSB)/portable/st/stm32_fsdev/dcd_stm32_fsdev.c

CFLAGS := -mcpu=cortex-m0 -mthumb -Os -g -std=gnu11 -Wall -Wextra -Wno-unused-parameter \
          -ffunction-sections -fdata-sections -fno-common -DSTM32F072xB -DUSB_PID=$(USB_PID) \
          -Isrc -Isrc/boards/$(BOARD) -Isrc/core -Isrc/hw -Isrc/usb -Ithird_party/cmsis -I$(TUSB) -MMD -MP
LDFLAGS := -mcpu=cortex-m0 -mthumb -nostartfiles --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map -Tlink/stm32f072rb.ld

OBJS := $(SRCS:%.c=$(BUILD)/%.o)
.DEFAULT_GOAL := all                    # the generated .d files below define rules; they must not become the default goal
-include $(OBJS:.o=.d)      # recompile whenever a header (e.g. the generated board_data.h) changes

all: $(BUILD)/$(TARGET).bin
	@$(SIZE) $(BUILD)/$(TARGET).elf

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# Host tests (native): protocol core, key engine, lighting engine
test:
	cc -std=c11 -Wall -Wextra -O1 -Isrc/boards/air75v2 -o test/sim test/sim.c src/core/nph_proto.c
	cc -std=c11 -Wall -Wextra -O1 -Isrc/boards/air75v2 -o test/keyeng_test test/keyeng_test.c src/core/keyeng.c src/core/nph_proto.c
	cc -std=c11 -Wall -Wextra -O1 -Isrc/boards/air75v2 -o test/light_test test/light_test.c src/core/light.c src/core/nph_proto.c
	cc -std=c11 -Wall -Wextra -O1 -Isrc/boards/air75v2 -o test/rf_test test/rf_test.c src/core/rf.c
	cc -std=c11 -Wall -Wextra -O1 -Isrc/boards/halo75v2 -o test/halo_test test/halo_test.c src/core/light.c src/core/nph_proto.c
	cc -std=c11 -Wall -Wextra -O1 -Isrc/boards/air75v2 -o test/fn_test test/fn_test.c src/core/fnkeys.c src/core/nph_proto.c
	cc -std=c11 -Wall -Wextra -O1 -Isrc/boards/air75v2 -o test/wl_test test/wl_test.c src/core/wlstore.c
	./test/keyeng_test && ./test/light_test && ./test/rf_test && ./test/wl_test && ./test/halo_test && ./test/fn_test && python3 test/replay_v3.py

# End-to-end: the web app's real device client <-> the C protocol core
e2e: test
	cd ../.. && npx esbuild firmware/air75v2/test/e2e.ts --bundle --platform=node --format=cjs --outfile=firmware/air75v2/test/e2e.cjs --log-level=error
	node test/e2e.cjs

# Flash over the ROM DFU bootloader (hold BOOT0 / use the button under the Caps Lock keycap while plugging in, or hold Esc)
flash: $(BUILD)/$(TARGET).bin
	dfu-util -a 0 -d 0483:df11 -s 0x08000000:leave -D $<

# Back up whatever is on the keyboard now (stock or QMK) so you can always restore it
backup:
	dfu-util -a 0 -d 0483:df11 -s 0x08000000:0x20000 -U backup-original.bin

clean:
	rm -rf $(BUILD)
.PHONY: all test e2e flash backup clean
