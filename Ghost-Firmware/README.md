# Ghost Firmware for M5Stack Cardputer (v1.2.0)

This is a standalone ESP32-S3 application with a Cyberpunk-themed, keyboard-navigable interface and an animated Ghost splash screen labeled “PinoyUnknown.” The 14 theme images are scaled proportionally to fit the Cardputer's 240×135 display.

### Controls

Use the built-in keyboard arrow keys (or `W`/`S` / `Fn`+arrows) to browse the CONNECT, CONFIG, CLOCK, BLE, WIFI, RF, NRF, NFC, MISC, JS, IR, GPS, FM, and FILE screens. Press `Enter` to open a screen and `Esc` or `Backspace` to return.
- **Universal Hardware Support:** Works on both standard M5Stack Cardputer v1.0/v1.1 (74HC138 GPIO scanning matrix) and Cardputer-Adv (TCA8418 I2C controller).
- **Button A Fallback:** Short-press `BtnA` (`G0`) to select or open; long-press `BtnA` (>500ms) acts as `Esc` to return from any screen, preventing locks.

### Features

- **CONNECT:** Live Wi-Fi station manager, IP/DNS/Gateway telemetry, reachability test, and connect/disconnect.
- **CONFIG:** 5 selectable color themes (Cyberpunk, Matrix, Tron, Sci-Fi, Pink Candy), animated tool backdrops, screen brightness, and key click tone.
- **CLOCK:** Live UTC time, uptime counter, Wi-Fi NTP sync, and manual time entry.
- **BLE:** Passive Bluetooth LE advertisement scanner with detailed inspector and `L` key SPIFFS log export (`/ble_scan.txt`).
- **WIFI:** Passive 802.11 network scanner with detailed AP inspector, `C` key direct network join, and `L` key SPIFFS log export (`/wifi_scan.txt`).
- **RF / NRF / NFC / GPS / FM:** Live Grove HY2.0-4P I2C bus scanner (`G2/G1`) with module probe and address detection.
- **MISC:** Comprehensive system telemetry (CPU frequency, battery voltage/%, free heap/PSRAM, flash/sketch size, MACs, keyboard driver mode).
- **JS:** Automated 6-stage hardware self-test diagnostic macro with report export to `/selftest.txt`.
- **IR:** Built-in Cardputer GPIO 44 infrared transmitter diagnostic testing.
- **FILE:** SPIFFS flash filesystem browser with auto-format on first boot, readme file, and text previewer.

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
