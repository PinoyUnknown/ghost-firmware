# Ghost Firmware v2.1 (Universal ESP32-S3 & Cardputer Mesh Edition)

<div align="center">
  <h2>👾 GHOST FIRMWARE v2.1 👾</h2>
  <strong>Universal ESP32-S3, Cardputer v1.0/v1.1/Adv & Cardputer Mesh Diagnostics Suite</strong>
  <br>
  <em>Author: PinoyUnknown (https://www.instagram.com/pinoyunknown)</em>
  <br>
  <em>Repository: https://github.com/PinoyUnknown/ghost-firmware</em>
</div>

---

## ⚡ Overview

**Ghost Firmware v2.1** is an all-in-one embedded tactical diagnostics firmware and field suite engineered specifically for:
- **All ESP32-S3 boards** (4MB, 8MB, and 16MB Flash models)
- **M5Stack Cardputer v1.0 & v1.1** (74HC138 GPIO scanning matrix)
- **M5Stack Cardputer-Adv & Cardputer Mesh** (TCA8418 I2C keyboard, ES8311 I2S audio codec, SX1262 LoRa module)
- **Universal compatibility**: Features safe bus timeouts, auto-formatting SPIFFS, and non-blocking Button A (G0) / Touch navigation so the device **never freezes or locks up during boot or operation**.

---

## 🎮 Navigation & Universal Controls

| Input Method | Action / Mapping |
| :--- | :--- |
| **W / S / Up / Down** | Scroll menu items, channels, or file listings |
| **A / D / Left / Right** | Page switch (MISC 1/2) or navigate settings |
| **Enter / Return** | Open selected app / Execute action / Send packet |
| **Esc / Backspace** | Return to previous menu / Go back to Home / Exit preview |
| **R** | Repeat scan (Wi-Fi, BLE, I2C Grove bus, Mesh probe) |
| **L** | Save full scan report to flash storage (`/wifi_scan.txt`, `/ble_scan.txt`) |
| **C** | Connect directly to selected Wi-Fi access point with on-screen password input |
| **BtnA (GPIO 0)** | **Universal Single-Button Fallback**: Short press = Select/Enter, Long press (>450ms) = Esc/Back, Double click = Cycle App |
| **Touchscreen** | Tap anywhere to open / return on touch-equipped boards (Core2, Tough) |

---

## 🛠️ The 14 Integrated Diagnostic Modules

1. **CONNECT (Wi-Fi Station & Network Reachability)**
   - Connects to 802.11 b/g/n networks with live status tracking.
   - Shows Local IP, Gateway, Subnet, DNS server, Signal RSSI (dBm), and device MAC address.
   - Built-in network reachability / DNS latency ping test (`pool.ntp.org`).

2. **CONFIG (Theme Engine & System Preferences)**
   - **5 Cyberpunk Themes**: CYBERPUNK 2077, HACKER MATRIX, TRON SYSTEM, FUTURE SCI-FI, PINK CANDY.
   - **3 Backdrop Modes**: Animated Green Digital Matrix Rain, Neon Scanline Pulse, or Static.
   - **Screen Brightness**: 25%, 50%, 75%, 100% duty cycle.
   - **Key Beep**: Auditory feedback toggle using hardware tone.
   - **Mesh Channel**: Selectable Wi-Fi / ESP-NOW channel 1 to 13.
   - Settings saved permanently to non-volatile flash storage (`Preferences`).

3. **CLOCK (UTC & Real-Time Sync)**
   - Live UTC clock with seconds-accurate real-time updates and device uptime (`hh:mm:ss`).
   - Network time synchronization via SNTP (`pool.ntp.org` / `time.nist.gov`).
   - Manual 14-digit timestamp setter (`YYYYMMDDhhmmss`).

4. **BLE (Passive Bluetooth LE Survey)**
   - Low-energy 2.4GHz beacon receiver tracking nearby advertisers.
   - Detailed inspector displaying Device Name, Bluetooth MAC, RSSI, Advertised UUIDs, and Manufacturer Data bytes.
   - Instant export to flash storage (`/ble_scan.txt`).

5. **WIFI (Passive 802.11 Survey & Network Joiner)**
   - Full 2.4GHz channel sweep discovering all nearby Access Points (SSID, BSSID, Channel, RSSI dBm, Security type).
   - In-depth AP telemetry view.
   - On-screen alphanumeric keyboard prompt to join any network directly.
   - Instant export to flash storage (`/wifi_scan.txt`).

6. **RF MESH (SX1262 LoRa & Native ESP-NOW Mesh)**
   - Hardware detection for onboard SX1262 LoRa radio on Cardputer Mesh / Cardputer-Adv.
   - Native dual-core ESP-NOW Mesh engine active on all ESP32-S3 boards.
   - Broadcast Mesh Ping: transmit test frames across peers (`GMSH` protocol).
   - Live packet counter and last-heard peer tracker.

7. **NRF/2.4G (Spectrum & Channel Analyzer)**
   - 2.4GHz Wi-Fi and NRF spectrum channel activity monitor.
   - Channel occupancy overview across channels 1 to 13.
   - Grove NRF24L01 pinout guide.

8. **NFC/RFID (Grove I2C Bus Tag Scanner)**
   - Scans Grove port (`G2=SDA, G1=SCL`) for PN532 (0x24), RC522 (0x28), ST25DV, and I2C transponders.
   - Real-time address reporting.

9. **MISC (Hardware Telemetry & Silicon Inspector)**
   - **Page 1**: ESP32-S3 Chip Model, Silicon Revision, Cores (Dual-Core @ 240MHz), Flash Size (4MB/8MB/16MB), SPIRAM/PSRAM, Internal Free Heap, Min Heap, Battery Voltage (mV), Battery Percentage (%).
   - **Page 2**: Firmware Version (`Ghost OS v2.1`), MAC Addresses (STA, AP, BLE), Active Keyboard Driver (`74HC138 MATRIX` / `TCA8418 I2C` / `CARDKB I2C` / `BUTTON ONLY`), Audio Status, Developer Information (`PinoyUnknown`).

10. **JS MACRO (Automated Diagnostic Self-Test)**
    - Multi-stage automated validation checking:
      1. Display framebuffer & RGB color rendering.
      2. Keyboard matrix and I2C controllers.
      3. Power management & battery telemetry.
      4. SPIFFS internal flash file read/write integrity.
      5. Wi-Fi and BLE radio transceiver status.
      6. IR transmitter and Grove expansion bus.
    - Exports comprehensive self-test report to `/selftest.txt`.

11. **IR TX (Infrared Transmitter Diagnostics)**
    - Direct drive of Cardputer GPIO 44 IR emitter with 38kHz modulation.
    - Selectable NEC diagnostic commands (Beacon, TV Power, TV Mute, Vol+, Vol-).

12. **GPS (NMEA Navigation Monitor)**
    - Real-time GPS serial receiver on Grove port (`G1=RX, G2=TX`) and Cardputer-Adv internal GPS (`GPIO 15/13`).
    - NMEA frame monitor and fix indicator.

13. **FM/RADIO (I2C Radio Receiver Probe)**
    - Probes Grove bus for FM receiver chipsets (RDA5807M, TEA5767, SI4703).
    - Signal presence indicator.

14. **FILE (Flash Storage Browser & Text Previewer)**
    - SPIFFS internal filesystem browser with used/total partition capacity.
    - Folder navigation and file entry inspector.
    - Integrated text file previewer with line scrolling (`/readme.txt`, `/wifi_scan.txt`, `/ble_scan.txt`, `/selftest.txt`).

---

## ⚡ Flashing Instructions

### Method 1: Pre-merged Binary via esptool
To flash the full image (including bootloader, partition table, and application) to any ESP32-S3 or Cardputer:
```sh
python -m esptool --chip esp32s3 --port COMx --baud 1500000 write_flash 0x0 Ghost-Firmware-v2.1.bin
```

### Method 2: PlatformIO Direct Build & Upload
From this directory:
```sh
pio run -e m5stack-cardputer --target upload
```
For Cardputer Mesh:
```sh
pio run -e cardputer-mesh --target upload
```
For Generic ESP32-S3 boards:
```sh
pio run -e esp32-s3 --target upload
```

