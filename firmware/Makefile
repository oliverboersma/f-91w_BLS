# F-91W BLE watch firmware (STM32WB55REV6, CPU1 application)
#
#   make                 build build/f91w.elf/.hex/.bin, linked at 0x08000000 (flash over SWD)
#   make OTA=1           link at 0x08007000 for ST's BLE_Ota loader
#   make NO_LED=1        board without D1 (RGB backlight)
#   make NO_BUZZER=1     board without L3 (piezo coil)
#   make test            host tests (step counter, display, time, heart rate)
#   make flash           flash over SWD with STM32_Programmer_CLI
#
# Needs arm-none-eabi-gcc and STM32CubeWB (scripts/fetch_cubewb.sh puts it in ../third_party).

CUBEWB  ?= ../third_party/STM32CubeWB
BUILD   ?= build
TARGET  := f91w
CROSS   ?= arm-none-eabi-

CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE    := $(CROSS)size

WPAN := $(CUBEWB)/Middlewares/ST/STM32_WPAN
HAL  := $(CUBEWB)/Drivers/STM32WBxx_HAL_Driver

SRC := \
  Core/Src/main.c Core/Src/app_entry.c Core/Src/app_debug.c Core/Src/hw_timerserver.c \
  Core/Src/stm32_lpm_if.c Core/Src/stm32wbxx_hal_msp.c Core/Src/stm32wbxx_it.c Core/Src/system_stm32wbxx.c \
  STM32_WPAN/App/app_ble.c STM32_WPAN/App/dis_app.c STM32_WPAN/App/watch_service.c STM32_WPAN/Target/hw_ipcc.c \
  app/watch_app.c app/display.c app/stepcount.c app/timeutil.c hr/hr_spotcheck.c \
  drivers/board.c drivers/lcd.c drivers/buzzer.c drivers/i2c_bus.c drivers/lis2dw12.c drivers/maxm86161.c drivers/battery.c \
  $(addprefix $(HAL)/Src/stm32wbxx_, hal.c hal_cortex.c hal_dma.c hal_dma_ex.c hal_exti.c hal_flash.c hal_flash_ex.c \
    hal_gpio.c hal_hsem.c hal_ipcc.c hal_pwr.c hal_pwr_ex.c hal_rcc.c hal_rcc_ex.c hal_rng.c hal_rtc.c hal_rtc_ex.c \
    hal_i2c.c hal_i2c_ex.c hal_lcd.c hal_lptim.c hal_adc.c hal_adc_ex.c ll_adc.c) \
  $(addprefix $(WPAN)/ble/core/auto/, ble_gap_aci.c ble_gatt_aci.c ble_hal_aci.c ble_hci_le.c ble_l2cap_aci.c) \
  $(WPAN)/ble/core/template/osal.c $(WPAN)/ble/svc/Src/dis.c $(WPAN)/ble/svc/Src/svc_ctl.c \
  $(WPAN)/interface/patterns/ble_thread/shci/shci.c \
  $(addprefix $(WPAN)/interface/patterns/ble_thread/tl/, hci_tl.c hci_tl_if.c shci_tl.c shci_tl_if.c tl_mbox.c) \
  $(addprefix $(WPAN)/utilities/, dbg_trace.c otp.c stm_list.c stm_queue.c) \
  $(CUBEWB)/Utilities/lpm/tiny_lpm/stm32_lpm.c $(CUBEWB)/Utilities/sequencer/stm32_seq.c

ASM := startup/startup_stm32wb55revx.s
SRC += startup/syscalls.c startup/sysmem.c

INC := Core/Inc STM32_WPAN/App app drivers hr board \
  $(HAL)/Inc $(HAL)/Inc/Legacy $(CUBEWB)/Drivers/CMSIS/Device/ST/STM32WBxx/Include $(CUBEWB)/Drivers/CMSIS/Include \
  $(WPAN) $(WPAN)/ble $(WPAN)/ble/core $(WPAN)/ble/core/auto $(WPAN)/ble/core/template $(WPAN)/ble/svc/Inc $(WPAN)/ble/svc/Src \
  $(WPAN)/interface/patterns/ble_thread $(WPAN)/interface/patterns/ble_thread/tl $(WPAN)/interface/patterns/ble_thread/shci \
  $(WPAN)/utilities $(CUBEWB)/Utilities/lpm/tiny_lpm $(CUBEWB)/Utilities/sequencer

DEFS := -DUSE_HAL_DRIVER -DSTM32WB55xx -DCORE_CM4
ifeq ($(NO_LED),1)
DEFS += -DBOARD_HAS_LED=0
endif
ifeq ($(NO_BUZZER),1)
DEFS += -DBOARD_HAS_BUZZER=0
endif

LDSCRIPT := startup/STM32WB55REVX_FLASH.ld
ifeq ($(OTA),1)
LDSCRIPT := startup/STM32WB55REVX_FLASH_OTA.ld
DEFS += -DVECT_TAB_OFFSET=0x08007000U
endif

MCU     := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS  := $(MCU) -std=gnu11 -Os -g3 -Wall -ffunction-sections -fdata-sections -fstack-usage $(DEFS) $(addprefix -I,$(INC))
LDFLAGS := $(MCU) -T$(LDSCRIPT) -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map --specs=nano.specs \
           -Wl,--print-memory-usage -Wl,--no-warn-rwx-segments -lm

OBJ := $(addprefix $(BUILD)/obj/,$(notdir $(SRC:.c=.o)) $(notdir $(ASM:.s=.o)))
vpath %.c $(sort $(dir $(SRC)))
vpath %.s $(dir $(ASM))

all: $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin

$(BUILD)/obj/%.o: %.c | $(BUILD)/obj
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/obj/%.o: %.s | $(BUILD)/obj
	$(CC) $(MCU) -x assembler-with-cpp -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJ) $(LDSCRIPT)
	$(CC) $(OBJ) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/%.hex: $(BUILD)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/%.bin: $(BUILD)/%.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/obj:
	mkdir -p $@

$(OBJ): $(MAKEFILE_LIST)

test:
	$(MAKE) -C tests

flash: $(BUILD)/$(TARGET).hex
	STM32_Programmer_CLI -c port=SWD mode=UR -w $< -v -rst

clean:
	rm -rf $(BUILD)
	$(MAKE) -C tests clean

.PHONY: all test flash clean
