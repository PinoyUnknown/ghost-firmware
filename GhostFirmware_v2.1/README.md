# Ghost Firmware v2.1 (Universal ESP32-S3, Cardputer & Cardputer Mesh Edition)

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

### ⚡ Universal ESP32-S3, Cardputer-Adv & Cardputer Mesh Cyber Diagnostic Multi-Tool ⚡
**Sub-1 GHz CC1101 & LoRa Transceiver • 125 kHz RFID & 13.56 MHz NFC • 2.4 GHz NRF Spectrum Analyzer • Precision GPS Navigation • Universal IR Blaster & Learner • MicroSD WebRadio • Zero-Flicker HUD**

[![PlatformIO Registry](https://img.shields.io/badge/PlatformIO-Build%20Passing-brightgreen?logo=platformio)](https://platformio.org/)
[![Target](https://img.shields.io/badge/Hardware-M5Stack%20Cardputer%20Mesh%20%7C%20ESP32--S3-blue?logo=espressif)](https://m5stack.com/)
[![Version](https://img.shields.io/badge/Version-v2.1-orange.svg)](https://github.com/PinoyUnknown/ghost-firmware/releases)
[![License](https://img.shields.io/badge/License-GPL--3.0-purple.svg)](LICENSE)

*Author & Lead Developer:* **PinoyUnknown** ([Instagram @pinoyunknown](https://www.instagram.com/pinoyunknown))  
*GitHub Repository:* [https://github.com/PinoyUnknown/ghost-firmware](https://github.com/PinoyUnknown/ghost-firmware)

</div>

---

## 📑 Table of Contents
1. [Overview & Architectural Philosophy](#-overview--architectural-philosophy)
2. [Supported Hardware & Technical Specifications](#-supported-hardware--technical-specifications)
3. [Key Highlights & New Capabilities in v2.1](#-key-highlights--new-capabilities-in-v21)
4. [User Interface & Landscape HUD Layout](#-user-interface--landscape-hud-layout)
5. [Complete Module & Tool Guide](#-complete-module--tool-guide)
   - [Tool 1: Neon Cyberpunk Clock (`CLOCK`)](#tool-1-neon-cyberpunk-clock-clock)
   - [Tool 2: Wi-Fi Tactical & Beacon Suite (`CONNECT` / `WIFI`)](#tool-2-wi-fi-tactical--beacon-suite-connect--wifi)
   - [Tool 3: BLE Proximity & Broadcast Suite (`BLE`)](#tool-3-ble-proximity--broadcast-suite-ble)
   - [Tool 4: Hardware Peripheral Bus (`PERIPHERAL`)](#tool-4-hardware-peripheral-bus-peripheral)
   - [Tool 5: Sub-1 GHz CC1101 & SX1262 LoRa Transceiver Suite (`RF MESH` / `SUB-1G`)](#tool-5-sub-1-ghz-cc1101--sx1262-lora-transceiver-suite-rf-mesh--sub-1g)
   - [Tool 6: 2.4 GHz NRF24 Spectrum Analyzer (`NRF/2.4G`)](#tool-6-24-ghz-nrf24-spectrum-analyzer-nrf24g)
   - [Tool 7: 125 kHz RFID & 13.56 MHz NFC Proximity Suite (`NFC/RFID`)](#tool-7-125-khz-rfid--1356-mhz-nfc-proximity-suite-nfcrfid)
   - [Tool 8: Environment & Motion Sensors (`ENV/IMU`)](#tool-8-environment--motion-sensors-envimu)
   - [Tool 9: Microphone & Audio Diagnostics (`MIC/SND`)](#tool-9-microphone--audio-diagnostics-micsnd)
   - [Tool 10: Universal Infrared Remote & Timing Learner (`IR TX`)](#tool-10-universal-infrared-remote--timing-learner-ir-tx)
   - [Tool 11: GNSS GPS Precision Navigation & Compass Rose (`GPS`)](#tool-11-gnss-gps-precision-navigation--compass-rose-gps)
   - [Tool 12: International WebRadio with MicroSD Playlist & ES8311 Codec (`FM`)](#tool-12-international-webradio-with-microsd-playlist--es8311-codec-fm)
   - [Tool 13: Dual Storage File Explorer & Text Previewer (`FILE`)](#tool-13-dual-storage-file-explorer--text-previewer-file)
   - [Tool 14: System Configuration & NVS Preferences (`CONFIG`)](#tool-14-system-configuration--nvs-preferences-config)
6. [Keyboard Navigation & Shortcuts Reference](#-keyboard-navigation--shortcuts-reference)
7. [How to Flash to Your Cardputer Mesh (Step-by-Step)](#-how-to-flash-to-your-cardputer-mesh-step-by-step)
   - [Method 1: Single-File 0x0 Flash via esptool (Recommended)](#method-1-single-file-0x0-flash-via-esptool-recommended)
   - [Method 2: Web Browser Flashing (Chrome / Edge Web Serial)](#method-2-web-browser-flashing-chrome--edge-web-serial)
   - [Method 3: PlatformIO CLI Upload](#method-3-platformio-cli-upload)
   - [Troubleshooting & Bootloader Mode](#troubleshooting--bootloader-mode)
8. [MicroSD Playlist Setup (`/station_list.txt`)](#-microsd-playlist-setup-station_listtxt)
9. [Binary Release & Checksums](#-binary-release--checksums)
10. [Comprehensive Changelog (Version History)](#-comprehensive-changelog-version-history)
11. [License & Acknowledgments](#-license--acknowledgments)

---

## 🌐 Overview & Architectural Philosophy

**Ghost Firmware v2.1** is an all-in-one cybernetic diagnostic firmware and wireless multi-tool engineered from the ground up for the **M5Stack Cardputer**, **Cardputer-Adv**, **Cardputer Mesh Kit**, and **universal ESP32-S3** microcontrollers.

Designed for security researchers, hardware hackers, radio operators, and electronics enthusiasts, Ghost Firmware combines:
* **Tactical Wireless Capabilities**: Surpassing contemporary tools like Bruce with rich Sub-1 GHz RF decoding, 2.4 GHz spectrum scanning, dual-band RFID/NFC emulation, Wi-Fi 802.11 management frame inspection, and BLE broadcast diagnostics.
* **Decentralized LoRa Mesh Communications**: Native Meshtastic LongFast framing over Semtech SX1262 (Cap LoRa-1262) and 2.4 GHz ESP-NOW fallback.
* **Audio Engineering**: Zero-lag I2S streaming over the ES8311 I2C codec and NS4150B 3W amplifier with an on-screen custom playlist loader from MicroSD.
* **Cyberpunk Visual Interface**: 100% flicker-free double-buffered rendering via `M5Canvas` with 14 full-color illustrations scaled at $0.75	imes$ to fit the landscape display edge-to-edge.
* **Cinematic 3-Phase Bootloader**: Pulsing ghost cyber-skull animation with live diagnostic telemetry, developer attribution, and dedicated typography screens for **Futuristic Softwares** and **Team White Hat Philippines**.

---

## 📟 Supported Hardware & Technical Specifications

| Subsystem | Hardware Specification |
| :--- | :--- |
| **Main Processor** | ESP32-S3FN8 (Dual-Core Xtensa LX7 @ 240MHz, 512KB SRAM, 8MB Flash) |
| **Display Panel** | 1.14" ST7789V2 IPS TFT LCD, $240 	imes 135$ pixels, 16-bit RGB565 |
| **Keyboard Engine** | 56-key matrix (4x14) with TI TCA8418 I2C controller (`0x34`) + 74HC138 demux fallback |
| **Audio Subsystem** | Everest Semiconductor ES8311 I2C Codec (`0x18`) + NS4150B 3W Class-D Amp + Speaker & 3.5mm Jack |
| **Sub-1 GHz RF** | TI CC1101 transceiver (SPI CS=5) & Semtech SX1262 LoRa (Cap LoRa-1262) |
| **2.4 GHz Radio** | ESP32-S3 802.11b/g/n Wi-Fi + BLE 5.0 + ESP-NOW + NRF24 spectrum analyzer |
| **RFID / NFC** | 125 kHz Low-Frequency (EM4100/HID) + 13.56 MHz High-Frequency NFC (ISO 14443-A/Mifare/NTAG) |
| **Infrared** | High-output 38 kHz IR transmitter LED on GPIO 44 + IR timing learner |
| **GNSS Positioning** | ATGM336H-6N @ AT6668 multi-constellation GNSS (GPS/QZSS/BD/GLO) @ 115200 8N1 |
| **Sensors** | Bosch BMI270 6-Axis IMU (accel/gyro) + MEMS microphone (GPIO 41/43/42) |
| **Storage** | Integrated MicroSD TF-Card slot (CS=12, SCK=40, MISO=39, MOSI=14) + 4MB SPIFFS Flash |
| **Power Management** | Internal 1500mAh LiPo battery with AXP/charge monitoring & physical power switch |

---

## ⚡ Key Highlights & New Capabilities in v2.1

* **100% Zero-Flicker Double-Buffered Display Pipeline (`M5Canvas`):** Completely eliminates LCD flashing and visual tearing by generating all UI layers off-screen in RAM sprites before issuing atomic DMA transfers.
* **ES8311 Codec Handover & Speaker Clean Release:** Resolves the silent audio bug by releasing `M5.Speaker` (`I2S_NUM_1`) prior to launching `ESP32-audioI2S` on `I2S_NUM_0` and configuring Everest ES8311 internal I2C registers (`0x18`) over SDA=8, SCL=9.
* **Custom MicroSD Station Playlist Loader (`/station_list.txt`):** Automatically detects MicroSD or SPIFFS, reads custom stream URLs, formats up to 32 stations, creates a default template file if missing, and provides a quick station list selector (`L` key).
* **Advanced Sub-1 GHz CC1101 & LoRa Transceiver (Tool 5):**
  - Multi-band frequency hopping: 315.00 MHz, 433.92 MHz, 868.35 MHz, and 915.00 MHz.
  - Multi-modulation engine: OOK, 2-FSK, 4-FSK, GFSK, and MSK digital shaping.
  - Built-in live protocol decoders: Weather stations (Oregon Scientific, LaCrosse), smart sockets/bulbs (PT2262), IoT sensors and doorbells (EV1527), and garage barriers (Came, KeeLoq).
  - Real-time rolling waterfall visualizer with RSSI signal meter.
  - One-touch toggle to Semtech SX1262 LoRa Meshtastic LongFast mesh engine via `TAB`.
* **2.4 GHz NRF24 Spectrum Analyzer (Tool 6):** Real-time energy sweep across channels 1 to 14 with peak-hold memory indicators and Wi-Fi / BLE channel overlap highlighting.
* **125 kHz RFID & 13.56 MHz NFC Proximity Suite (Tool 7):**
  - Read, save, and emulate LF cards (EM4100, HID Prox 26-bit) and HF NFC tags (ISO 14443-A, Mifare Classic 1K, NTAG213).
  - Manual UID entry modal (`CardIdEntry` screen) allowing users to key in hex card IDs directly from the Cardputer keyboard.
  - One-touch dumping of card UIDs to MicroSD card (`D` key).
* **Universal Infrared Remote & Learner (Tool 10):** Multi-brand appliance database (TV, AC, Projector) plus high-speed GPIO timing receiver for learning and cloning IR remote signals.
* **GNSS GPS Precision Navigation & Compass Rose (Tool 11):** NMEA parser with decimal degrees (DEC) and degrees-minutes-seconds (DMS) toggle (`F` key), speed, altitude, HDOP precision, and an 8-point digital compass rose.
* **Dual Storage File Explorer & In-App Text Previewer (Tool 13):** Auto-detects MicroSD TF-cards, traverses nested directories, and previews `.txt`, `.json`, `.csv`, and `.log` files with smooth vertical scrolling (`W`/`S`) without crashing.
* **Cinematic 3-Phase Cyberpunk Boot Animation:**
  - Phase 1 (13.5s): Pulsing cyber skull with sonar rings, diagnostics, progress bar, and developer attribution (`PinoyUnknown`).
  - Phase 2 (3.2s): Large typography banner `FUTURISTIC SOFTWARES`.
  - Phase 3 (3.2s): Cyber defense typography banner `"TEAM WHITE HAT" PHILIPPINES`.
  - Instant skip support: Press any keyboard key or casing `G0` button to jump straight to the OS.

---

## 🖥️ User Interface & Landscape HUD Layout

The 1.14" ST7789 display is locked to landscape mode ($240 	imes 135$ pixels):

```text
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

### Tool 1: Neon Cyberpunk Clock (`CLOCK`)
* **Aesthetic Visuals:** Multilayer glowing neon glass tubes with drop-shadow bloom and white core illumination. An animated Matrix green digital code rain background runs concurrently.
* **Controls:**
  * `S` or `ENTER`: Open Clock Settings Modal.
  * `C`: Cycle Neon Color Theme (Cyber Cyan/Pink, Toxic Matrix, Synthwave Sunset, Electric Ice, Retro Amber).
  * `M`: Cycle Matrix Rain Speed (Off $ightarrow$ Slow $ightarrow$ Normal $ightarrow$ Fast).
  * `F`: Toggle 12-Hour (AM/PM) / 24-Hour display format.
  * `R`: Trigger NTP network time synchronization over Wi-Fi (Philippine Time UTC+8 default).

### Tool 2: Wi-Fi Tactical & Beacon Suite (`CONNECT` / `WIFI`)
* **Passive 802.11 Scanner:** Scans and lists nearby SSIDs, RSSI signal levels, channels, BSSID MACs, and authentication types.
* **Interactive Password Prompt:** Select any scanned network, type the WPA2/WPA3 password with full on-screen backspace and cursor navigation, and connect. Credentials persist in NVS.
* **Beacon Generator (`B` key):** Transmits raw 802.11 beacon management frames broadcasting custom SSIDs (`GhostNet_Mesh`, `NightCity_Free_WiFi`, `Arasaka_ICE_Secure`, `Militech_Tactical`, `Blackwall_Breach`, `PinoyUnknown_Ghost`).
* **Promiscuous Deauth Sniffer (`D` key):** Engages promiscuous packet capture to detect and log incoming 802.11 Deauthentication (`0x0C`) and Disassociation (`0x0A`) frames with a live attack tally counter.

### Tool 3: BLE Proximity & Broadcast Suite (`BLE`)
* **Passive BLE Scanner:** Discovers Bluetooth Low Energy peripherals, advertising names, MAC addresses, RSSI, and primary service UUIDs.
* **Broadcast Beacon Generator (`B` key):** Transmits diagnostic advertising packets simulating Apple ActionModal (AirDrop/AirPods pairing), Google Android Fast Pair alerts, and Samsung QuickShare transfer prompts.

### Tool 4: Hardware Peripheral Bus (`PERIPHERAL`)
* Inspects Grove Port A (I2C SDA=2, SCL=1) and internal buses for connected sensors, I2C addresses, and hardware interfaces.

### Tool 5: Sub-1 GHz CC1101 & SX1262 LoRa Transceiver Suite (`RF MESH` / `SUB-1G`)
* **Multi-Band Transceiver:** Operates across standard ISM bands: **315 MHz**, **433.92 MHz**, **868.35 MHz**, and **915.00 MHz**.
* **Digital Modulation Support:** Switches dynamically between **OOK**, **2-FSK**, **4-FSK**, **GFSK**, and **MSK**.
* **Protocol Decoders:**
  * **Weather Stations:** Decodes Oregon Scientific & LaCrosse temperature and humidity wireless telemetry.
  * **Smart Sockets & Bulbs:** Decodes PT2262 fixed-code remote switches.
  * **IoT Sensors & Doorbells:** Decodes EV1527 rolling/learning-code pulses.
  * **Garage Doors & Boom Barriers:** Decodes Came & KeeLoq gate remote access packets.
* **Live Waterfall & RSSI Display:** Displays real-time signal activity and packet strength waterfall.
* **LoRa Meshtastic Mesh Engine (`TAB` key):**
  * Toggles instantly to Semtech SX1262 LoRa mode with Meshtastic LongFast wire protocol (`0x2B` sync word, SF=11, CR=4/5, BW=250kHz).
  * `T`: Broadcast Text Beacon packet.
  * `N`: Broadcast NodeInfo packet with battery and telemetry data.

### Tool 6: 2.4 GHz NRF24 Spectrum Analyzer (`NRF/2.4G`)
* **Channel Sweep:** Measures RF signal energy across all 14 channels (2400 to 2484 MHz).
* **Visual Bar Chart:** Displays dynamic bar graphs with peak-hold indicators.
* **Channel Highlighting:** Highlights active Wi-Fi and BLE advertising frequencies to identify low-interference channels.

### Tool 7: 125 kHz RFID & 13.56 MHz NFC Proximity Suite (`NFC/RFID`)
* **Low-Frequency (125 kHz RFID):**
  * Reads EM4100 and HID Prox 26-bit access cards.
  * Emulates saved card UIDs via coil antenna.
* **High-Frequency (13.56 MHz NFC):**
  * Reads ISO 14443-A cards, Mifare Classic 1K, and NTAG213 tags.
  * Emulates NFC tags and extracts UID / ATQA / SAK.
* **Manual UID Input (`CardIdEntry` screen):** Press `M` to open the manual card entry screen and key in any hex UID using the physical Cardputer keyboard.
* **SD Card UID Dumping (`D` key):** Automatically appends scanned or entered card IDs to `/rfid_dump.txt` on the MicroSD card.

### Tool 8: Environment & Motion Sensors (`ENV/IMU`)
* Real-time readouts from onboard Bosch BMI270 6-axis IMU (pitch, roll, yaw, acceleration vectors) and external Grove environmental sensors (DHT11/22, BME280).

### Tool 9: Microphone & Audio Diagnostics (`MIC/SND`)
* Live FFT audio frequency analyzer capturing ambient sound through the Cardputer MEMS microphone.

### Tool 10: Universal Infrared Remote & Timing Learner (`IR TX`)
* **Appliance Database:** Built-in 38 kHz NEC power and control codes for:
  * **Televisions:** Sony, Samsung, LG, Panasonic, Sharp, Philips, TCL.
  * **Air Conditioners:** Daikin, Mitsubishi, Panasonic, Gree, Carrier.
  * **Projectors:** Epson, BenQ, ViewSonic, Optoma.
* **IR Timing Learner (`L` key):** Listens on the IR receiver pin for external remotes, sampling mark/space pulse timings and displaying the decoded protocol and hex code.

### Tool 11: GNSS GPS Precision Navigation & Compass Rose (`GPS`)
* **Hardware Support:** Multi-constellation GNSS (GPS, GLONASS, BeiDou, Galileo) via Grove UART at 115200 baud.
* **Position Readout:** Displays Latitude, Longitude, Altitude, Speed (km/h), and Satellites Locked.
* **Coordinate Formats (`F` key):** Switch between Decimal Degrees (`LAT: 14.599512° N`) and Degrees-Minutes-Seconds (`LAT: 14° 35' 58.2" N`).
* **8-Point Compass Rose:** Dynamic header bar with N, NE, E, SE, S, SW, W, NW directional heading indicator.

### Tool 12: International WebRadio with MicroSD Playlist & ES8311 Codec (`FM`)
* **I2S Audio Pipeline:** ES8311 I2C codec (`0x18`) + NS4150B amplifier with zero hardware speaker conflicts.
* **Default Philippine Radio Stations:**
  1. `DWIZ 882 Manila` (News & Aliw Network)
  2. `DZRH 666 Manila` (Nationwide Public News)
  3. `Monster RX 93.1` (Manila Top 40 & Pinoy Hits)
  4. `Love Radio 90.7` (OPM & Love Songs)
  5. `Wish 107.5 Manila` (Pinoy Wish Bus & OPM)
  6. `Star FM 102.7` (Pinoy Pop & Trending)
  7. `Yes The Best 101.1` (Manila Mass Radio)
  8. `Pinoy Classic OPM` (Classic Philippine Hits)
* **International Stations:** DEF CON Radio, SomaFM Groove Salad, Nightwave Plaza Vaporwave, SomaFM Secret Agent, BBC World Service, KEXP 90.3 Seattle.
* **MicroSD Playlist Support:** Automatically loads custom stations from `/station_list.txt`.
* **Station Selector Popup (`L` key):** Opens an on-screen station browser with live cursor navigation.

### Tool 13: Dual Storage File Explorer & Text Previewer (`FILE`)
* **Dual Storage:** Seamlessly toggle between internal SPIFFS flash and MicroSD TF-Card via `TAB` or `T`.
* **Directory Navigation:** Move up/down with `W`/`S`, press `[` or `Backspace` to enter parent folders.
* **In-App Text Previewer:** Press `ENTER` on any file to open the scrollable previewer (`Screen::FilePreview`) supporting `.txt`, `.json`, `.csv`, `.log`, and `.ini` files up to 16 lines at a time.

### Tool 14: System Configuration & NVS Preferences (`CONFIG`)
* Configure display brightness (25%, 50%, 75%, 100%), backdrop effects, key beeps, screen orientation, default radio station, and factory reset.

---

## ⌨️ Keyboard Navigation & Shortcuts Reference

### Global Shortcuts (Everywhere)
| Key | Function |
| :--- | :--- |
| `A` / `D` or `Left` / `Right` | Move between previous / next tool |
| `W` / `S` or `Up` / `Down` | Scroll list items, menus, or adjust volume |
| `ENTER` / `SPACE` | Open selected feature or execute action |
| `ESC` / `G0` | Return to Home screen / Skip boot animation |
| `[` or `Backspace` | Navigate up one directory in File Explorer |

### Tool-Specific Shortcuts
| Tool | Key | Action |
| :--- | :--- | :--- |
| **Sub-1 GHz RF (Tool 5)** | `F` | Cycle frequency (315 $ightarrow$ 433.92 $ightarrow$ 868.35 $ightarrow$ 915 MHz) |
| | `M` | Cycle modulation (OOK $ightarrow$ 2-FSK $ightarrow$ 4-FSK $ightarrow$ GFSK $ightarrow$ MSK) |
| | `P` | Cycle protocol decoder (Weather $ightarrow$ Sockets $ightarrow$ Doorbells $ightarrow$ Barriers) |
| | `TAB` | Switch between CC1101 Sub-1G and SX1262 LoRa Meshtastic |
| **NRF Spectrum (Tool 6)** | `R` | Reset peak-hold indicators |
| | `H` | Toggle Wi-Fi / BLE channel highlight overlay |
| **RFID & NFC (Tool 7)** | `TAB` | Toggle between 125 kHz RFID and 13.56 MHz NFC |
| | `M` | Open manual Card UID entry modal (`CardIdEntry`) |
| | `D` | Dump current UID to MicroSD card (`/rfid_dump.txt`) |
| **IR Remote (Tool 10)** | `ENTER` | Transmit active appliance power code |
| | `L` | Toggle IR timing learning mode |
| **GPS (Tool 11)** | `F` | Toggle Decimal Degrees (DEC) vs Degrees-Minutes-Seconds (DMS) |
| **WebRadio (Tool 12)** | `SPACE` / `ENTER` | Play / Pause audio stream |
| | `A` / `D` | Previous / Next station |
| | `W` / `S` | Volume Up / Volume Down (0 to 21) |
| | `M` | Mute / Unmute audio |
| | `L` | Open on-screen Station List selector |
| **File Explorer (Tool 13)** | `TAB` / `T` | Toggle between MicroSD and Internal SPIFFS |
| | `ENTER` | Open file previewer (`Screen::FilePreview`) |
| | `[` / `Backspace`| Navigate to parent directory |

---

## ⚡ How to Flash to Your Cardputer Mesh (Step-by-Step)

### Method 1: Single-File 0x0 Flash via esptool (Recommended)

Ghost Firmware v2.1 releases provide a **pre-merged single binary** containing the bootloader, partition table, and firmware application aligned to offset `0x0`.

1. Connect your M5Stack Cardputer to your PC via a USB-C data cable.
2. Determine your serial COM port (e.g. `COM5` on Windows or `/dev/ttyACM0` on Linux/macOS).
3. Open a terminal and run:

```bash
# Flash the single merged binary directly to offset 0x0
python -m esptool --chip esp32s3 -p COM5 -b 1500000 write_flash 0x0 Ghost-Firmware-v2.1.bin
```

---

### Method 2: Web Browser Flashing (Chrome / Edge Web Serial)

You can flash Ghost Firmware directly from your browser without installing Python:

1. Open **[ESP Web Tools](https://esp.adrel/flash)** or **[Adafruit Web Serial ESPTool](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/)** in Google Chrome or Microsoft Edge.
2. Plug in your Cardputer and click **Connect**. Select your Cardputer USB Serial port.
3. Set the Flash Offset to `0x0000` (or `0x0`).
4. Select `Ghost-Firmware-v2.1.bin` (or `GhostFirmware_Cardputer.bin`).
5. Click **Program** / **Flash**. Wait until progress reaches 100%, then reboot your device.

---

### Method 3: PlatformIO CLI Upload

If building from source:

```bash
# Clone the repository
git clone https://github.com/PinoyUnknown/ghost-firmware.git
cd ghost-firmware/GhostFirmware_v2.1

# Build and flash for Cardputer Mesh Kit
pio run -e cardputer-mesh -t upload --upload-port COM5

# Or build and flash for standard Cardputer v1.0 / v1.1
pio run -e m5stack-cardputer -t upload --upload-port COM5
```

---

### Troubleshooting & Bootloader Mode

* **Port Not Detected / COM Port Error:**
  Ensure you are using a USB-C cable capable of high-speed data transfer (not charging-only). Check Device Manager on Windows for `USB Serial JTAG / CDC`.
* **Flashing Times Out / Failed to Connect:**
  Put the ESP32-S3 into ROM Bootloader mode manually:
  1. Hold down the **`G0` casing button** (GPIO 0).
  2. While holding `G0`, switch the power switch **ON** (or insert the USB cable).
  3. Release `G0` after 1 second. The screen remains black.
  4. Execute the flash command again.

---

## 📻 MicroSD Playlist Setup (`/station_list.txt`)

You can add any online MP3/AAC internet radio stream to your Cardputer by creating a file named `station_list.txt` in the root of your MicroSD card.

### Format
Each line contains fields separated by pipe characters (`|`):
```text
Station Name | Stream URL | Frequency / Call | Region | Genre
```

### Example `/station_list.txt`
```text
DWIZ 882 Manila|http://dwizmanila.radioca.st/stream|882 kHz AM|PH|News & Talk (Aliw)
DZRH 666 Manila|http://stream.zeno.fm/f920ghk8vkeuv|666 kHz AM|PH|News Nationwide (MBC)
Monster RX 93.1|http://icecast.eradioportal.com:8000/rx931_manila|93.1 MHz FM|PH|Top 40 & Pinoy Hits
Love Radio 90.7|http://stream.zeno.fm/s4k81v76w4zuv|90.7 MHz FM|PH|OPM & Love Songs
Wish 107.5 Manila|http://stream.zeno.fm/0r0xa792kwzuv|107.5 MHz FM|PH|Pinoy Wish Bus & OPM
Star FM 102.7|http://stream.zeno.fm/a816p2v4w4zuv|102.7 MHz FM|PH|Pinoy Pop & Trending
Yes The Best 101.1|http://stream.zeno.fm/cvu4v626w4zuv|101.1 MHz FM|PH|Manila Mass Hits
Pinoy Classic OPM|http://stream.zeno.fm/uDkifg|Web Stream|PH|Classic Golden OPM
DEF CON Radio|http://ice1.somafm.com/defcon-128-mp3|Online Cyber|INT|Hacker / Cyber Chill
SomaFM Groove Salad|http://ice1.somafm.com/groovesalad-128-mp3|Online Synth|INT|Ambient / Chillout
Nightwave Plaza|http://ice1.somafm.com/vaporwaves-128-mp3|Vaporwave|INT|Aesthetic Synthwave
SomaFM Secret Agent|http://ice1.somafm.com/secretagent-128-mp3|Online Spy|INT|Spy & Lounge Downtempo
BBC World Service|http://stream.live.vc.bbcmedia.co.uk/bbc_world_service|World News|INT|Global News & Analysis
KEXP 90.3 Seattle|http://live-aac-64.kexp.org/kexp64.aac|90.3 MHz FM|INT|Indie & Alternative
```

*If no `station_list.txt` is present on your MicroSD card, Ghost Firmware automatically generates this default template for you on boot!*

---

## 📦 Binary Release & Checksums

| Binary File | Size (Bytes) | Flash Offset | SHA-256 Checksum |
| :--- | :--- | :--- | :--- |
| **`Ghost-Firmware-v2.1.bin`** | 2,105,376 | `0x0000` | `66cc6644fe5ea8df1760406d1d3f362a237ceb6d83996da559e209133b54872d` |
| **`GhostFirmware_Cardputer.bin`** | 2,105,376 | `0x0000` | `66cc6644fe5ea8df1760406d1d3f362a237ceb6d83996da559e209133b54872d` |

---

## 📜 Comprehensive Changelog (Version History)

### 🚀 Version 2.1 (Current Major Release) — November 2026
* **ES8311 Codec Audio Handover & Silent Speaker Fix:**
  - Resolved hardware resource conflict between `M5Unified` (`M5.Speaker`) on `I2S_NUM_1` and `ESP32-audioI2S` on `I2S_NUM_0`.
  - Added clean driver shutdown (`M5.Speaker.end()`) prior to stream initialization.
  - Implemented Everest Semiconductor ES8311 I2C codec (`0x18`) register sequence for Cardputer-Adv and Cardputer Mesh Kit.
  - Suppressed key beep tone interruptions while WebRadio is streaming.
* **Dynamic MicroSD Radio Playlist Loader:**
  - Supports up to 32 streaming stations loaded dynamically from `/station_list.txt` on MicroSD or internal SPIFFS.
  - Automatically generates an initial playlist file with 8 Philippine stations and 6 international streams if missing.
  - Added on-screen station browser popup (`L` key) with live cursor and one-touch station tuning.
* **Advanced Sub-1 GHz CC1101 & LoRa Transceiver Suite (Tool 5):**
  - Integrated multi-band transceiver covering 315, 433.92, 868.35, and 915 MHz.
  - Modulations: OOK, 2-FSK, 4-FSK, GFSK, MSK.
  - Built-in decoders for Oregon/LaCrosse Weather Stations, PT2262 Smart Sockets, EV1527 Doorbells, and Came/KeeLoq Garage Barriers.
  - Real-time rolling waterfall visualizer and signal strength meter.
  - One-touch toggle (`TAB`) to Semtech SX1262 LoRa Meshtastic LongFast mesh engine.
* **2.4 GHz NRF24 Spectrum Analyzer (Tool 6):**
  - Live 14-channel frequency sweep with peak-hold bar graphs and Wi-Fi/BLE highlight overlays.
* **125 kHz RFID & 13.56 MHz NFC Proximity Suite (Tool 7):**
  - Read, save, and emulate LF cards (EM4100, HID Prox 26-bit) and HF NFC tags (ISO 14443-A, Mifare Classic 1K, NTAG213).
  - Added dedicated `CardIdEntry` screen for manual UID keying via Cardputer keyboard.
  - Added one-touch SD card UID dumping (`D` key).
* **Universal Infrared Remote & Learner (Tool 10):**
  - Database of TV, AC, and Projector power codes plus GPIO timing learner for decoding and cloning remotes.
* **GNSS GPS Precision Navigation & Compass Rose (Tool 11):**
  - High-speed NMEA parser with DEC/DMS coordinate toggling, speed, altitude, HDOP, and 8-point compass rose.
* **Dual Storage File Explorer & Text Previewer (Tool 13):**
  - Auto-mounts MicroSD cards and provides in-app preview of `.txt`, `.json`, `.csv`, and `.log` files with smooth vertical scrolling.
* **Cinematic 3-Phase Cyberpunk Boot Animation:**
  - 13.5-second pulsing cyber skull with telemetry and developer links (`PinoyUnknown`).
  - 3.2-second typography flash: `FUTURISTIC SOFTWARES`.
  - 3.2-second typography flash: `"TEAM WHITE HAT" PHILIPPINES`.
  - Instant skip support via any key or casing `G0` button.
* **100% Zero-Flicker Double-Buffered Rendering:**
  - Strict `M5Canvas` RAM sprite off-screen rendering with atomic DMA transfer to ST7789 display.
* **Hardware Compatibility:**
  - Dual TI TCA8418 I2C (`0x34`) + 74HC138 keyboard driver with Shift/Fn modifier mappings.
  - Fixed horizontal landscape rotation ($240 	imes 135$) with NVS sanitization.

---

### 📦 Version 2.0 — September 2026
* Initial Cardputer port with 14 Cyberpunk HUD illustrations.
* Passive Wi-Fi and BLE scanners.
* Basic UTC clock and SPIFFS flash browser.

---

### 📦 Version 1.0 — September 2026
* Proof-of-concept ESP32-S3 serial diagnostic utility.

---

## ⚖️ License & Acknowledgments

* **Ghost Firmware** is distributed under the **GPL-3.0 License**. See [LICENSE](LICENSE) for details.
* **Credits & Inspiration:**
  * **[Bruce](https://github.com/BruceDevices/Firmware)** — Outstanding tactical radio multi-tool design.
  * **[Cardputer-WebRadio](https://github.com/halbeshuhn/Cardputer-WebRadio)** — ESP32-S3 WebRadio architecture.
  * **[Neon Clock](https://clockfaces.com/neon-clock/)** — Cyberpunk glowing neon tube styling.
  * **[Meshtastic](https://meshtastic.org/)** — Decentralized, off-grid LoRa mesh networking protocol.
  * **[M5Stack](https://m5stack.com/)** — Cardputer hardware and M5Unified ecosystem.

---

<div align="center">
  <strong>Developed with ❤️ by PinoyUnknown</strong><br>
  <em>Follow on Instagram: <a href="https://www.instagram.com/pinoyunknown">@pinoyunknown</a></em>
</div>
