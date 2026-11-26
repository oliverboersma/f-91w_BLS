#!/bin/sh
# Fetch the parts of STM32CubeWB this firmware builds against (~250 MB) into ../third_party.
set -e
DEST="${1:-$(dirname "$0")/../../third_party/STM32CubeWB}"
TAG=v1.24.0
if [ ! -d "$DEST/.git" ]; then
  git clone --depth 1 --branch "$TAG" --filter=blob:none --sparse https://github.com/STMicroelectronics/STM32CubeWB.git "$DEST"
fi
cd "$DEST"
git sparse-checkout set Drivers/CMSIS Middlewares/ST/STM32_WPAN Utilities/sequencer Utilities/lpm \
  Projects/STM32WB_Copro_Wireless_Binaries/STM32WB5x Projects/P-NUCLEO-WB55.Nucleo/Applications/BLE/BLE_Ota
git submodule update --init --depth 1 Drivers/STM32WBxx_HAL_Driver Drivers/CMSIS/Device/ST/STM32WBxx
echo "STM32CubeWB $TAG ready in $DEST"
