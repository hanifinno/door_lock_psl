#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR"

MKSPIFFS="/Users/psl/Library/Arduino15/packages/esp32/tools/mkspiffs/0.2.3/mkspiffs"
ESPTOOL="/Users/psl/Library/Arduino15/packages/esp32/tools/esptool_py/5.3.1/esptool"
PORT=$(ls /dev/cu.usbserial* /dev/tty.usbserial* 2>/dev/null | head -n 1)
if [ -z "$PORT" ]; then
  PORT="/dev/cu.usbserial-1110"
fi

if [ ! -f "$MKSPIFFS" ]; then
  MKSPIFFS=$(ls /Users/psl/Library/Arduino15/packages/esp32/tools/mkspiffs/*/mkspiffs 2>/dev/null | head -n 1)
fi

if [ ! -f "$ESPTOOL" ]; then
  ESPTOOL=$(ls /Users/psl/Library/Arduino15/packages/esp32/tools/esptool_py/*/esptool 2>/dev/null | head -n 1)
fi

mkdir -p .build
echo "📦 Building SPIFFS image from data/ folder..."
"$MKSPIFFS" -c data -b 4096 -p 256 -s 0x160000 .build/spiffs.bin

echo "⚡ Flashing SPIFFS image to ESP32 at port $PORT..."
"$ESPTOOL" --chip esp32 --port "$PORT" --baud 115200 write-flash 0x290000 .build/spiffs.bin

echo "✅ Done! SPIFFS successfully uploaded."
