# Ghost-Firmware.bin

This directory contains the download and flashing instructions for the standalone Ghost firmware for the M5Stack Cardputer. It is separate from the Meshtastic firmware and does not provide LoRa or Meshtastic messaging.

The app uses the Cyberpunk-inspired colors from `Cyberpunk_2077_v1.0.0/Cyberpunk 2077.json`, shows the Ghost splash screen, and provides a passive Wi-Fi survey. The survey lists nearby broadcast network names, signal strength, channel, and security mode; it does not connect to networks or transmit Wi-Fi management frames.

## Download

The GitHub Actions build publishes `Ghost-Firmware.bin` as a downloadable artifact:

1. Open the [Ghost Firmware build workflow](https://github.com/PinoyUnknown/ghost-firmware/actions/workflows/build-ghost-firmware.yml).
2. Select the latest successful run.
3. Download the **Ghost-Firmware** artifact and extract `Ghost-Firmware.bin`.

Published GitHub releases will also include the binary.

## Flash

`Ghost-Firmware.bin` is a merged ESP32-S3 image containing the bootloader, partition table, and app. With Espressif esptool installed, connect the Cardputer in download mode and run:

```sh
python -m esptool --chip esp32s3 --port COMx --baud 1500000 write_flash 0x0 Ghost-Firmware.bin
```

Replace `COMx` with the Cardputer's serial port. On macOS or Linux, use the device path (for example, `/dev/ttyACM0`) instead. This standalone image replaces the firmware currently on the device; save anything you need before flashing.

## Build from source

From the repository root, build and upload directly to a connected Cardputer with PlatformIO:

```sh
pio run -e m5stack-cardputer --target upload
```
