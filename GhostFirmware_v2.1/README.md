# Ghost Firmware v2.1 (Universal ESP32-S3 & Cardputer Mesh Edition)

<div align="center">

```
  ██████╗ ██╗  ██╗ ██████╗ ███████╗████████╗
 ██╔════╝ ██║  ██║██╔═══██╗██╔════╝╚══██╔══╝
 ██║  ███╗███████║██║   ██║███████╗   ██║   
 ██║   ██║██╔══██║██║   ██║╚════██║   ██║   
 ╚██████╔╝██║  ██║╚██████╔╝███████║   ██║   
  ╚═════╝ ╚═╝  ╚═╝ ╚═════╝ ╚══════╝   ╚═╝   
       F I R M W A R E   v 2 . 1
```

### ⚡ Universal ESP32-S3, Cardputer-Adv & Cardputer Mesh Cyber Multi-Tool ⚡
**Advanced Wireless Diagnostics, LoRa Meshtastic Mesh, WebRadio, Cyberpunk Neon Clock & Tactical Bruce Suite**

[![PlatformIO Registry](https://img.shields.io/badge/PlatformIO-Build%20Passing-brightgreen?logo=platformio)](https://platformio.org/)
[![Target](https://img.shields.io/badge/Hardware-M5Stack%20Cardputer%20Mesh%20%7C%20ESP32--S3-blue?logo=espressif)](https://m5stack.com/)
[![Version](https://img.shields.io/badge/Version-v2.1-orange.svg)](https://github.com/PinoyUnknown/ghost-firmware/releases)
[![License](https://img.shields.io/badge/License-GPL--3.0-purple.svg)](LICENSE)

*Author & Lead Developer:* **PinoyUnknown** ([Instagram @pinoyunknown](https://www.instagram.com/pinoyunknown))  
*GitHub Repository:* [https://github.com/PinoyUnknown/ghost-firmware](https://github.com/PinoyUnknown/ghost-firmware)

</div>

---

## 📑 Table of Contents
1. [Overview & What is Ghost Firmware?](#-overview--what-is-ghost-firmware)
2. [Supported Hardware & Technical Specifications](#-supported-hardware--technical-specifications)
3. [Key Highlights & New Features in v2.1](#-key-highlights--new-features-in-v21)
4. [User Interface & Landscape HUD Layout](#-user-interface--landscape-hud-layout)
5. [Complete Module & Tool Guide](#-complete-module--tool-guide)
   - [Neon Cyberpunk Clock](#1-neon-cyberpunk-clock-clock)
   - [International WebRadio (Philippine Default)](#2-international-webradio-fm)
   - [Wi-Fi Beacon Spammer & Deauth Sniffer](#3-wi-fi-tactical-tools-connect--wifi)
   - [BLE Spammer & Scanner](#4-ble-tactical-tools-ble)
   - [LoRa Meshtastic Mesh](#5-lora-meshtastic-mesh-rf-mesh)
   - [GNSS GPS Navigation & Tracking](#6-gnss-gps-receiver-gps)
   - [Infrared TV-B-Gone](#7-infrared-tv-b-gone-ir-tx)
   - [2.4GHz RF Spectrum Visualizer](#8-24ghz-rf-spectrum-visualizer-nrf24g)
   - [NFC & RFID Scanner](#9-nfc--rfid-scanner-nfcrfid)
   - [SPIFFS Flash File Manager](#10-internal-flash-file-manager-file)
   - [Configuration & Settings](#11-device-configuration-config)
   - [System Diagnostics](#12-system-diagnostics-misc)
6. [Keyboard Navigation & Shortcuts Reference](#-keyboard-navigation--shortcuts-reference)
7. [How to Flash to Your Cardputer Mesh (Step-by-Step)](#-how-to-flash-to-your-cardputer-mesh-step-by-step)
   - [Method 1: Single-File Flash via esptool (Recommended)](#method-1-single-file-flash-via-esptool-recommended)
   - [Method 2: Web Browser Flashing (Chrome / Edge)](#method-2-web-browser-flashing-chrome--edge)
   - [Method 3: PlatformIO CLI Upload](#method-3-platformio-cli-upload)
   - [Method 4: Multi-Partition Manual Flash](#method-4-multi-partition-manual-flash)
   - [Troubleshooting & Bootloader Mode](#troubleshooting--bootloader-mode)
8. [Binary Release & Checksums](#-binary-release--checksums)
9. [Comprehensive Changelog (Version History)](#-comprehensive-changelog-version-history)
10. [License & Acknowledgments](#-license--acknowledgments)

---

## 🌐 Overview & What is Ghost Firmware?

**Ghost Firmware v2.1** is a high-performance, open-source tactical diagnostics firmware engineered specifically for the **M5Stack Cardputer**, **Cardputer-Adv**, **Cardputer Mesh Kit**, and **universal ESP32-S3** microcontrollers.

Named after cybernetic "ghosts" in cyberpunk lore, Ghost Firmware merges:
* **The rich visual HUD aesthetic** of retro-futuristic Cyberpunk terminals with custom full-color pixel art.
* **The tactical wireless capabilities** of Bruce (`BruceDevices/Firmware`), including Wi-Fi beacon generation, promiscuous 802.11 deauth/disassociation sniffing, and multi-protocol BLE spamming.
* **The real-time streaming audio power** of WebRadio (`halbeshuhn/Cardputer-WebRadio`) with hardware I2S decoding and default Philippine radio stations.
* **The long-range decentralized communications** of the Meshtastic LoRa wire protocol with the Cap LoRa-1262 expansion module.
* **The aesthetic timepiece experience** of Neon Cyberpunk Clock (`clockfaces.com/neon-clock`) with animated Matrix digital rain and glowing neon tubes.

---

## 📟 Supported Hardware & Technical Specifications

Ghost Firmware v2.1 is optimized for the **Cardputer Mesh Kit** and all standard **ESP32-S3** boards:

| Subsystem | Specification Details |
| :--- | :--- |
| **Core Controller** | **Stamp-S3A / ESP32-S3FN8** (Dual-core Xtensa LX7 @ 240MHz, 320KB SRAM, 8MB Flash) |
| **Display** | **ST7789V2 @ 1.14" LCD Screen**, native resolution **240 x 135 pixels**, 16-bit RGB565 |
| **Keyboard** | **56 Keys (4 x 14 matrix)** with 74HC138 demultiplexer + TCA8418 / CardKB I2C fallback |
| **Audio Hardware** | **ES8311 Codec** + **NS4150B 3W Class-D Amp** + 8Ω 1W Speaker + 3.5mm Headphone Jack |
| **Microphone** | **MEMS Microphone** (SNR: 65 dB) |
| **IMU Sensor** | **BMI270 6-Axis Motion Sensor** (3-axis accelerometer + 3-axis gyroscope) |
| **LoRa Transceiver** | **Cap LoRa-1262** module with Semtech **SX1262** (868 ~ 923 MHz, +22dBm TX, -147dBm RX) |
| **GNSS GPS** | **ATGM336H-6N @ AT6668** multi-constellation GNSS (GPS/QZSS/BD2/BD3/GAL/GLO) @ **115200bps 8N1** |
| **Infrared Emitter** | **38kHz Infrared High-Output LED** on GPIO 44 |
| **Expansion Ports** | HY2.0-4P Grove interface (I2C: SDA=2, SCL=1) + EXT 2.54-14P bus |
| **Storage** | Onboard microSD TF-card slot + 4MB/8MB SPIFFS Flash partition |
| **Battery & Power** | Built-in 1500mAh lithium-polymer battery with power monitoring |

---

## ⚡ Key Highlights & New Features in v2.1

* **Horizontal Landscape Lock ($240 \times 135$):** Eliminates previous portrait rotation bugs. All rendering is locked to standard horizontal rotation with automatic NVS preference sanitization.
* **Aspect-Correct $0.75\times$ HUD Art Scaling:** All 14 Cyberpunk HUD illustrations ($320 \times 144$) are downscaled on the fly to $240 \times 108$ pixels ($y=14..122$), guaranteeing that zero pixels are clipped between the status header and bottom navigation footer.
* **Neon Cyberpunk Clock (`clockfaces.com/neon-clock`):** Multi-layered glowing neon glass tubes with drop-shadow bloom, accent corner brackets, and a real-time animated **Matrix green code rain** background.
* **International WebRadio (`halbeshuhn/Cardputer-WebRadio`):** Online streaming internet radio powered by `ESP32-audioI2S` with **8 default Philippine FM/AM stations** + 6 international stations, frequency dial needle, and 16-band live spectrum visualizer.
* **Tactical Bruce Tool Suite (`BruceDevices/Firmware`):**
  - Raw 802.11 Beacon Flooder with customized Cyberpunk SSIDs (`B` key in CONNECT).
  - Promiscuous 802.11 Deauth/Disassociation frame sniffer with live attack counter (`D` key in CONNECT).
  - Continuous BLE Spammer targeting Apple AirDrop/Setup, Android Fast Pair, and Samsung QuickShare (`B` key in BLE).
  - TV-B-Gone multi-brand 38kHz infrared power code blaster on GPIO 44.
  - 2.4GHz RF spectrum visualizer showing energy distribution across all 14 channels.
* **LoRa Meshtastic Protocol Engine:**
  - SX1262 SPI hardware driver on Cap LoRa-1262.
  - Region selection for 915 MHz (US/PH), 868 MHz (EU), 923 MHz (AS), and 433 MHz (HAM).
  - Broadcast Text Beacons (`T` key), NodeInfo packets (`N` key), and live peer tracking table.
* **Fixed Toolchain & GCC Compatibility:** PlatformIO build configured cleanly with Espressif 32 framework (`2.0.17`) and GCC 8.4, eliminating `-std=gnu++2b` and `-mdisable-hardware-atomics` errors.

---

## 🖥️ User Interface & Landscape HUD Layout

The 1.14" ST7789 screen is strictly mapped in landscape mode:

```
+-------------------------------------------------------+  y = 0
| [GHOST v2.1]   CH:915M   BAT:98%   WIFI:ON   02:18:00 |  Header Bar (y=0..13)
+-------------------------------------------------------+  y = 14
|                                                       |
|                                                       |
|               240 x 108 HUD ARTWORK AREA              |  HUD Canvas (y=14..121)
|              (Scaled 0.75x Edge-to-Edge)              |
|                                                       |
|                                                       |
+-------------------------------------------------------+  y = 122
|  [ENTER] OPEN   [W/S] SELECT   [ESC] BACK             |  Footer Bar (y=122..134)
+-------------------------------------------------------+  y = 135
```

---

## 🛠️ Complete Module & Tool Guide

### 1. Neon Cyberpunk Clock (`CLOCK`)
* **Visuals:** High-voltage neon tube glow with white core highlights and colored ambient bloom. Real-time digital Matrix code rain cascades behind the clock face.
* **Controls:**
  * `S` or `ENTER`: Open Clock Settings Modal.
  * `C`: Cycle Neon Color Theme (Cyber Cyan & Pink, Toxic Matrix, Synthwave Sunset, Electric Ice, Retro Amber).
  * `M`: Cycle Matrix Rain Speed (Off $\rightarrow$ Slow $\rightarrow$ Normal $\rightarrow$ Fast).
  * `F`: Toggle 12-Hour (AM/PM) / 24-Hour display format.
  * `R`: Trigger NTP network time sync over Wi-Fi.
  * `T`: Enter manual UTC time string (`YYYYMMDDhhmmss`).

### 2. International WebRadio (`FM`)
* **Audio Engine:** `ESP32-audioI2S` driver routing high-quality audio through the ES8311 codec and NS4150B speaker amplifier or 3.5mm jack.
* **Default Philippine Radio Stations:**
  1. `DWIZ 882 Manila` (News & Commentary)
  2. `DZRH 666 Manila` (Nationwide Public News)
  3. `Monster RX 93.1` (Manila Top 40 Pop & Pinoy Hits)
  4. `Love Radio 90.7` (OPM Hits & Love Songs)
  5. `Wish 107.5 Manila` (Live Pinoy Wish Bus & OPM)
  6. `Star FM 102.7` (Pinoy Pop & Trending)
  7. `Yes The Best 101.1` (Manila Mass Radio)
  8. `Pinoy Classic OPM` (Classic Philippine Hits)
* **International Stations:**
  9. `DEF CON Radio` (SomaFM Hacker Lounge)
  10. `SomaFM Groove Salad` (Ambient Downtempo)
  11. `Nightwave Plaza` (Vaporwave / Synth)
  12. `SomaFM Secret Agent` (Spy / Lounge)
  13. `BBC World Service` (Global News)
  14. `KEXP 90.3 Seattle` (Indie Rock)
* **Controls:**
  * `SPACE` / `ENTER`: Play / Stop stream.
  * `A` / `D` (or Left / Right): Change radio station.
  * `W` / `S` (or Up / Down): Volume Up / Down (0..21).
  * `M`: Mute / Unmute audio.
  * `R`: Reconnect active stream.

### 3. Wi-Fi Tactical Tools (`CONNECT` & `WIFI`)
* **Passive Wi-Fi Scanner:** Lists nearby SSIDs, RSSI signal strength, channels, BSSID MACs, and encryption types.
* **Wi-Fi Beacon Flooder (`B` key):** Transmits raw 802.11 beacon management frames broadcasting custom SSIDs (`GhostNet_Mesh`, `NightCity_Free_WiFi`, `Arasaka_ICE_Secure`, `Militech_Tactical`, `Blackwall_Breach`, `PinoyUnknown_Ghost`).
* **Promiscuous Deauth Sniffer (`D` key):** Puts ESP32 Wi-Fi radio into promiscuous mode, capturing and logging incoming 802.11 Deauthentication (`0x0C`) and Disassociation (`0x0A`) frames with a live on-screen counter.
* **Credentials Manager:** Connects to selected AP with on-screen password input.

### 4. BLE Tactical Tools (`BLE`)
* **Passive BLE Scanner:** Discovers nearby BLE devices, advertising names, addresses, RSSI, and service UUIDs.
* **BLE Spammer (`B` key):** Transmits rapid BLE advertising packets imitating:
  * Apple ActionModal (AirPods, Apple TV, AirDrop popups)
  * Google Android Fast Pair alerts
  * Samsung QuickShare transfer prompts

### 5. LoRa Meshtastic Mesh (`RF MESH`)
* **Hardware Integration:** Cap LoRa-1262 with Semtech SX1262 connected over SPI.
* **Wire Protocol:** Implements Meshtastic LongFast framing (Sync Word: `0x2B`, Bandwidth: 250kHz, Spreading Factor: 11, Coding Rate: 4/5).
* **Frequencies:** Toggle between 915 MHz (PH/US), 868 MHz (EU), 923 MHz (AS), and 433 MHz (HAM).
* **Node Discovery:** Automatically parses incoming packet headers to build a live peer table with Node ID, SNR, RSSI, and hop counts.
* **Beacon Transmission:**
  * Press `T` to broadcast a Text Beacon across the mesh.
  * Press `N` to broadcast local NodeInfo with hardware telemetry.
* **ESP-NOW Fallback:** Automatically operates over 2.4GHz ESP-NOW when LoRa Cap is not attached.

### 6. GNSS GPS Receiver (`GPS`)
* **Hardware:** ATGM336H-6N @ AT6668 high-sensitivity GPS module.
* **Interface:** Grove UART at **115200 baud (8N1)**.
* **Data Displayed:** Real-time Latitude, Longitude, Altitude, Speed (km/h), Fix Quality, Tracked Satellites, and UTC timestamp.

### 7. Infrared TV-B-Gone (`IR TX`)
* **Hardware:** High-output 38kHz infrared diode on Cardputer GPIO 44.
* **Features:** Built-in TV-B-Gone power codes for major TV brands (Sony, Samsung, LG, Panasonic, Sharp, Philips, TCL).
* **Controls:** Select target brand with `W`/`S` and press `ENTER` to transmit the 38kHz NEC pulse sequence.

### 8. 2.4GHz RF Spectrum Visualizer (`NRF/2.4G`)
* Visualizes real-time 2.4GHz energy across Wi-Fi channels 1 through 14 using dynamic bar meters, useful for identifying congested channels and RF noise.

### 9. NFC & RFID Scanner (`NFC/RFID`)
* Scans Grove I2C bus (`SDA=2, SCL=1`) for connected NXP PN532 (`0x24`) or RC522 (`0x28`) RFID readers, parsing card UID and technology type.

### 10. Internal Flash File Manager (`FILE`)
* Explores internal SPIFFS flash memory: browse directories, view file sizes, preview text/config files, and delete obsolete logs.

### 11. Device Configuration (`CONFIG`)
* **Display Brightness:** Adjust backlight duty cycle (10% to 100%).
* **Theme Palettes:** Switch between 6 global UI palettes (Cyberpunk Neon, Matrix Green, Vaporwave Pink, Retro Amber, Ice Blue, High Contrast).
* **LoRa Region:** Change default operating frequency band.
* **Hardware Self-Test:** Runs automated diagnostics on I2C bus, keyboard, display, audio amp, and radio.

### 12. System Diagnostics (`MISC`)
* Live display of Battery Voltage (mV) and Percentage, CPU Temperature, Free Heap, Minimum Free Heap, Total PSRAM/Flash space, and system uptime.

---

## 🎮 Keyboard Navigation & Shortcuts Reference

| Key | Global Action | Context-Specific Action |
| :--- | :--- | :--- |
| **`W` / `S`** or **`Up` / `Down`** | Move selection Up / Down | Adjust WebRadio Volume / Scroll text preview |
| **`A` / `D`** or **`Left` / `Right`** | Navigate Left / Right | Switch WebRadio Station / Switch config option |
| **`ENTER` / `Return`** | Open selected tool / Execute action | Play / Pause WebRadio stream |
| **`ESC` / `Backspace`** | Exit app / Go back to Home | Exit clock settings / Go to parent directory |
| **`BtnA` (GPIO 0)** | Short Press = Select / Open | Long Press (>450ms) = Back to Home |
| **`Fn` + `,` / `;` / `.` / `/`** | Directional Arrows | Left, Up, Down, Right |
| **`B`** | — | Toggle Beacon Flooder (in `CONNECT`) / BLE Spammer (in `BLE`) |
| **`D`** | — | Toggle Promiscuous Deauth Sniffer (in `CONNECT`) |
| **`T`** | — | Send LoRa Text Beacon (in `RF MESH`) / Set Manual Time (in `CLOCK`) |
| **`N`** | — | Send LoRa NodeInfo (in `RF MESH`) / Trigger NTP Sync (in `CLOCK`) |
| **`F`** | — | Toggle LoRa Frequency (in `RF MESH`) / Toggle 12H/24H (in `CLOCK`) |
| **`M`** | — | Mute WebRadio (in `FM`) / Toggle Matrix Rain (in `CLOCK`) |
| **`R`** | — | Rescan Wi-Fi / Rescan BLE / Reconnect WebRadio |

---

## 💾 How to Flash to Your Cardputer Mesh (Step-by-Step)

Ghost Firmware v2.1 provides a single, pre-merged binary (`Ghost-Firmware-v2.1.bin`) that can be flashed directly to offset **`0x0`**.

### Method 1: Single-File Flash via esptool (Recommended)

This is the fastest and most reliable method for Windows, macOS, and Linux.

1. **Connect your Cardputer** to your computer via a USB-C data cable.
2. **Identify your serial port:**
   * **Windows:** Open Device Manager $\rightarrow$ Ports (COM & LPT) (e.g., `COM3`, `COM5`).
   * **macOS:** `/dev/cu.usbmodem*`
   * **Linux:** `/dev/ttyACM0` or `/dev/ttyUSB0`
3. **Open Terminal / PowerShell** in the folder containing `Ghost-Firmware-v2.1.bin`:
4. **Run the following command:**
   ```bash
   python -m esptool --chip esp32s3 -p COM5 -b 1500000 write_flash 0x0 Ghost-Firmware-v2.1.bin
   ```
   *(Replace `COM5` with your actual port. On Linux/macOS, use `-p /dev/ttyACM0`)*

5. Once flashing completes, power cycle or press the `RST` button on your Cardputer.

---

### Method 2: Web Browser Flashing (Chrome / Edge)

No Python or terminal required! Use any WebSerial-compatible browser:

1. Open **[ESP Web Tools](https://esp.element-gui.com/)** or **[Adafruit ESP Web Flasher](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/)**.
2. Click **Connect** and select your Cardputer's USB Serial Port.
3. Select **Add File**:
   * File: `Ghost-Firmware-v2.1.bin`
   * Address / Offset: `0x0`
4. Click **Program** / **Flash**.
5. Wait for the progress bar to reach 100%, then reboot the device.

---

### Method 3: PlatformIO CLI Upload

If you have PlatformIO installed, you can compile and upload directly from source:

1. Open terminal in the project directory:
   ```bash
   cd C:\Project-PinoyUnknown\Ghost-Firmware\M5stack-Cardputer-Tools\GhostFirmware_v2.1
   ```
2. Build and upload for **Cardputer Mesh**:
   ```bash
   pio run -e cardputer-mesh -t upload --upload-port COM5
   ```
3. Or build for **standard Cardputer v1.0 / v1.1**:
   ```bash
   pio run -e m5stack-cardputer -t upload --upload-port COM5
   ```

---

### Method 4: Multi-Partition Manual Flash

If flashing individual binaries manually:

```bash
python -m esptool --chip esp32s3 -p COM5 -b 1500000 write_flash \
  0x0000 bootloader.bin \
  0x8000 partitions.bin \
  0xe000 boot_app0.bin \
  0x10000 firmware.bin
```

---

### Troubleshooting & Bootloader Mode

* **Device not detected:**
  Ensure you are using a high-quality USB-C **data cable** (some cheap cables are charging-only). Check that the CH9102 / CP210x or ESP32-S3 USB JTAG/CDC driver is installed.
* **Flash fails or times out:**
  Force the ESP32-S3 into ROM Bootloader mode manually:
  1. Hold down the **`G0` button** (or `BtnA` on Cardputer).
  2. While holding `G0`, toggle the power switch **ON** or plug in the USB cable.
  3. Release `G0` after 1 second. The screen will remain black; this confirms ROM bootloader mode.
  4. Run the flashing command again.

---

## 📦 Binary Release & Checksums

| File | Size | SHA-256 Checksum |
| :--- | :--- | :--- |
| **`Ghost-Firmware-v2.1.bin`** | 2,045,728 bytes | `E4BC550E6196BB0AE08DBC725B7D31615D0015F7FB853B52032837D3F3242B7C` |
| **`GhostFirmware_Cardputer.bin`** | 2,045,728 bytes | `E4BC550E6196BB0AE08DBC725B7D31615D0015F7FB853B52032837D3F3242B7C` |

*Both files are identical single-file flasher binaries ready for offset `0x0`.*

---

## 📜 Comprehensive Changelog (Version History)

### 🚀 Version 2.1 (Current Release) — November 2026
* **Fixed Display Orientation:**
  - Implemented `enforceLandscapeOrientation()` locking ST7789 rotation strictly to horizontal landscape mode ($240 \times 135$).
  - Added runtime NVS preference sanitization to prevent accidental portrait rotation.
* **Edge-to-Edge HUD Layout Scaling:**
  - Scaled all 14 Cyberpunk HUD illustrations ($320 \times 144$) to $240 \times 108$ at $0.75\times$ scale ($y=14..121$).
  - Header ($y=0..13$) and footer ($y=122..134$) are preserved with zero clipping and zero visual distortion.
* **Neon Cyberpunk Clock (`clockfaces.com/neon-clock`):**
  - Integrated realistic multi-layer glowing neon glass tube simulation with bloom and core highlights.
  - Added live animated **Matrix green code rain** cascade running on a FreeRTOS background task.
  - Created 5 neon color themes (Cyber Cyan & Pink, Toxic Matrix, Synthwave Sunset, Electric Ice, Retro Amber).
  - Added interactive in-app settings modal (`S` or `ENTER`) for 12H/24H format, themes, matrix rain speed, and NTP Wi-Fi sync.
* **International WebRadio (`halbeshuhn/Cardputer-WebRadio`):**
  - Built high-performance audio streaming engine using `ESP32-audioI2S` on ES8311 codec + NS4150B amplifier.
  - Configured **8 default Philippine radio stations** (DWIZ 882 Manila, DZRH 666, Monster RX 93.1, Love Radio 90.7, Wish 107.5, Star FM 102.7, Yes The Best 101.1, Pinoy Classic OPM) + 6 international stations.
  - Added real-time 16-band audio spectrum visualizer and analog-style tuner needle.
* **Bruce Tactical Tools Suite (`BruceDevices/Firmware`):**
  - **Wi-Fi Beacon Flooder (`B` key):** Transmits raw 802.11 beacon management frames broadcasting custom SSIDs.
  - **Promiscuous Deauth Sniffer (`D` key):** Captures and counts 802.11 deauthentication and disassociation frames in real time.
  - **BLE Spammer (`B` key):** Broadcasts spoofed Apple ActionModal, Android Fast Pair, and Samsung QuickShare advertisements.
  - **TV-B-Gone:** 38kHz infrared power code blaster on GPIO 44 for major TV brands.
  - **2.4GHz RF Spectrum:** 14-channel live energy bar visualizer.
  - **NFC/RFID Scanner:** Automatic I2C bus scanner for PN532 (`0x24`) and RC522 (`0x28`).
  - **GNSS GPS:** High-speed NMEA parser on Grove UART at 115200 baud for ATGM336H GPS.
* **LoRa Meshtastic Mesh Engine:**
  - Full SPI driver for Cap LoRa-1262 (SX1262) transceiver.
  - Meshtastic LongFast wire protocol, sync word `0x2B`, NodeId derivation, and multi-region frequency switching (868/915/923/433 MHz).
  - Text Beacon (`T`) and NodeInfo (`N`) packet broadcasting with live peer table.
* **Build System & Toolchain Fix:**
  - Resolved PlatformIO compiler error by aligning Arduino-ESP32 2.0.17 with GCC 8.4 toolchain.
  - Removed unsupported `-std=gnu++2b` and `-mdisable-hardware-atomics` flags.
  - Generated pre-merged `0x0` flasher binaries and updated documentation.

---

### 📦 Version 2.0 — September 2026
* Initial Cardputer port and integration of 14 Cyberpunk pixel-art HUD layouts.
* Basic passive Wi-Fi and BLE scanning capabilities.
* UTC clock with basic NTP and manual time input.
* Internal SPIFFS flash filesystem browser.
* Foundation of modular tool architecture.

---

### 📦 Version 1.0 — September 2026
* Proof-of-concept release for ESP32-S3 DevKit boards.
* Basic serial interface and display initializations.

---

## ⚖️ License & Acknowledgments

* **Ghost Firmware** is distributed under the **GPL-3.0 License**. See [LICENSE](LICENSE) for details.
* **Credits & Inspiration:**
  * **[Bruce](https://github.com/BruceDevices/Firmware)** — Exceptional tactical and RF tool design inspiration.
  * **[Cardputer-WebRadio](https://github.com/halbeshuhn/Cardputer-WebRadio)** — Outstanding ESP32-S3 WebRadio architecture.
  * **[Neon Clock](https://clockfaces.com/neon-clock/)** — Cyberpunk glowing neon tube and digital clock styling.
  * **[Meshtastic](https://meshtastic.org/)** — Decentralized, off-grid LoRa mesh networking protocol.
  * **[M5Stack](https://m5stack.com/)** — Groundbreaking Cardputer hardware and M5Unified ecosystem.

---

<div align="center">
  <strong>Developed with ❤️ by PinoyUnknown</strong><br>
  <em>Follow on Instagram: <a href="https://www.instagram.com/pinoyunknown">@pinoyunknown</a></em>
</div>
