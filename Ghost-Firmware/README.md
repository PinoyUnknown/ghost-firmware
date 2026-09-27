# Ghost Firmware for M5Stack Cardputer

This is a standalone ESP32-S3 application with a Cyberpunk-themed, keyboard-navigable menu and an animated Ghost splash screen labeled “PinoyUnknown.” The 14 theme images are scaled proportionally to fit the Cardputer's 240×135 display.

Use the built-in keyboard arrow keys or W/S to browse the CONNECT, CONFIG, CLOCK, BLE, WIFI, RF, NRF, NFC, MISC, JS, IR, GPS, FM, and FILE screens. Press Enter to open a screen and Esc or Backspace to return. The Clock shows uptime. WIFI performs a receive-only scan of nearby access points; press R or Enter to rescan. Cardputer button A opens the selected screen and returns from feature screens.

The other category screens currently show their theme art and status text only; they do not imply the corresponding external radios, NFC readers, GPS, infrared, Bluetooth, JavaScript, or file-browser functions are implemented. This standalone app does not provide Meshtastic or LoRa messaging.

## Download

GitHub Actions publishes `Ghost-Firmware.bin` as a build artifact:

1. Open the [Ghost Firmware build workflow](https://github.com/PinoyUnknown/ghost-firmware/actions/workflows/build-ghost-firmware.yml).
2. Select the latest successful run.
3. Download the **Ghost-Firmware** artifact and extract `Ghost-Firmware.bin`.

Published GitHub releases also include the binary.

## Flash

`Ghost-Firmware.bin` is a merged ESP32-S3 image containing the bootloader, partition table, and application. With Espressif esptool installed, connect the Cardputer in download mode and run:

```sh
python -m esptool --chip esp32s3 --port COMx --baud 1500000 write_flash 0x0 Ghost-Firmware.bin
```

Replace `COMx` with the Cardputer's serial port. On macOS or Linux, use the device path (for example, `/dev/ttyACM0`) instead. This standalone image replaces the firmware currently on the device; save anything you need before flashing.

## Build from source

From the repository root, build and upload directly to a connected Cardputer with PlatformIO:

```sh
pio run -e m5stack-cardputer --target upload
```
