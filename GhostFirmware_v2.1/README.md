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

## ⚡ Overview & What's New in v2.1

**Ghost Firmware v2.1** is a complete, production-grade tactical diagnostics suite and cyber multi-tool engineered specifically for:
- **Cardputer Mesh & Cardputer Adv** (ESP32-S3, ST7789 LCD, 74HC138/TCA8418 keyboard, SX1262 LoRa module)
- **All ESP32-S3 Development Boards** (DevKitC-1, StampS3, CoreS3, 4MB / 8MB / 16MB Flash)
- **Cross-Platform Compatibility**: Clean abstractions for Arduino, ESP32, and Raspberry Pi / Linux nodes.

### 🌟 Key Highlights in v2.1

1. **Landscape Orientation & Edge-to-Edge HUD Layout Lock**:
   - The ST7789 display is locked to strictly horizontal **Landscape mode ($240 \times 135$)**.
   - Automatically sanitizes NVS rotation preferences to prevent portrait inversion.
   - All 14 Cyberpunk HUD layouts (320×144) are scaled down at `0.75x` ratio to fit edge-to-edge horizontally across $240 \times 108$ pixels between the top header ($y=0..14$) and bottom footer ($y=122..135$). Zero clipping, zero distortion, 100% visible graphics.

2. **Neon Cyberpunk Clock (`clockfaces.com/neon-clock`)**:
   - **Glowing Neon Tubes**: Replicating classic high-voltage glass neon signs with bloom shadows, core highlights, and accent corner brackets.
   - **Animated Matrix Code Rain**: Real-time vertical streams of green digital code rain (`01`, `K`, `Z`, `#`, `*`) falling behind the clock face.
   - **5 Neon Themes**:
     - `CYBER CYAN & PINK`: Electric Cyan tubes with Hot Pink brackets and white core.
     - `TOXIC MATRIX`: Vibrant Green phosphor with Lime accents.
     - `SYNTHWAVE SUNSET`: Neon Magenta/Red with Golden Amber glow.
     - `ELECTRIC ICE`: Deep Neon Blue with Ice Cyan accents.
     - `RETRO AMBER`: Warm Monochrome Amber neon.
   - **Interactive Clock Settings Menu**: Press `S` or `ENTER` on Clock screen to adjust 12H/24H format, neon theme, matrix rain speed (Off/Slow/Normal/Fast), tube flicker simulation, and trigger NTP sync over Wi-Fi.

3. **Bruce Diagnostic & Tactical Tools (`BruceDevices/Firmware`)**:
   - **Wi-Fi Beacon Flooder**: Transmits raw 802.11 beacon frames with cyberpunk SSIDs (`GhostNet_Mesh`, `NightCity_Free_WiFi`, `Arasaka_ICE_Secure`, `Militech_Tactical`, `Blackwall_Breach`, `PinoyUnknown_Ghost`). Toggle with `B` in CONNECT.
   - **Wi-Fi Promiscuous Deauth Sniffer**: Monitors 802.11 management frames (`0x00C0` Deauth / `0x00A0` Disassoc) with live on-screen packet counter and alerts. Toggle with `D` in CONNECT.
   - **BLE Spammer**: Generates dynamic Apple ActionModal and FastPair advertisement packets. Toggle with `B` in BLE.
   - **Infrared TV-B-Gone**: Rapid 38kHz NEC pulses on GPIO 44 for Sony, Samsung, LG, Panasonic, Sharp, Philips, and TCL.
   - **2.4GHz RF Energy Spectrum**: 14-channel live spectrum visualizer with bar meters.
   - **NFC/RFID Scanner**: I2C bus scanner for PN532 (`0x24`) and RC522 (`0x28`).
   - **GPS Receiver**: Grove serial NMEA parser for Latitude, Longitude, Altitude, Speed, and Satellites.

4. **LoRa Meshtastic Protocol Engine**:
   - Integrated hardware driver for SX1262 LoRa SPI (SCK=40, MISO=39, MOSI=14, CS=5, RST=3, DIO1=4, BUSY=6).
   - LongFast modem configuration, sync word `0x2B`, NodeId generation from chip MAC.
   - Broadcast Text Beacons (`T` key) and NodeInfo packets (`N` key).
   - Multi-Region Frequency switching: 915.0 MHz (US/PH), 868.0 MHz (EU), 433.1 MHz (HAM), 923.0 MHz (AS).
   - Live discovered peer table with RSSI, SNR, hop count, and packet counts.
   - ESP-NOW mesh fallback for non-LoRa ESP32 devices.

5. **WebRadio (`halbeshuhn/Cardputer-WebRadio`)**:
   - High-performance streaming audio using `ESP32-audioI2S` (Cardputer I2S pins: BCK=41, WS=43, DOUT=42).
   - **Philippine Radio Stations as DEFAULT**:
     1. `DWIZ 882 Manila` (News & Talk)
     2. `DZRH 666 Manila` (Nationwide News)
     3. `Monster RX 93.1` (Top 40 & Pinoy Hits)
     4. `Love Radio 90.7` (OPM & Love Songs)
     5. `Wish 107.5 Manila` (Pinoy Wish Bus & OPM)
     6. `Star FM 102.7` (Pinoy Pop & Trending)
     7. `Yes The Best 101.1` (Manila Mass Hits)
     8. `Pinoy Classic OPM` (Classic Golden OPM)
   - Followed by 6 International stations (DEF CON Radio, SomaFM Groove Salad, Nightwave Plaza, SomaFM Secret Agent, BBC World Service, KEXP 90.3 Seattle).
   - Live frequency tuner dial needle and 16-band animated audio spectrum visualizer.

---

## 🎮 Navigation & Keyboard Controls

| Key | Action / Mapping |
| :--- | :--- |
| **W / S / Up / Down** | Scroll menu items, channels, or adjust volume |
| **A / D / Left / Right** | Switch stations or navigate pages |
| **Enter / Return** | Open selected app / Execute action / Play or Stop stream |
| **Esc** | Return to Home / Exit preview / Close clock settings |
| **BtnA (GPIO 0)** | Short press = Select, Long press (>450ms) = Back / Cycle App |
| **Clock Screen** | `S`/`ENTER`: Settings, `C`: Theme, `M`: Matrix Rain, `F`: 12/24H, `R`: NTP Sync, `T`: Set Time |
| **Connect Screen** | `ENTER`: Scan APs, `B`: Beacon Flood Toggle, `D`: Deauth Sniffer Toggle |
| **BLE Screen** | `R`: Rescan, `B`: BLE Spammer Toggle |
| **RF Mesh Screen** | `T`: Text Beacon, `N`: NodeInfo Broadcast, `F`: Freq Toggle, `R`: Rescan Bus |
| **WebRadio Screen** | `SPACE`/`ENTER`: Play/Stop, `A`/`D`: Station Change, `W`/`S`: Volume, `M`: Mute, `R`: Reconnect |

---

## 💾 Flashing Guide (Merged 0x0 Binary)

The firmware is provided as a complete, pre-merged binary ready to flash directly to offset `0x0`:

```bash
esptool.py --chip esp32s3 --port COM_PORT --baud 1500000 write_flash 0x0 Ghost-Firmware-v2.1.bin
```

### Binary Checksum
- **File**: `Ghost-Firmware-v2.1.bin` / `GhostFirmware_Cardputer.bin`
- **Size**: 2,045,728 bytes
- **SHA-256**: `9673FC3F2D6CEE1AF8EFF18AF18CD836C648B5B2A0EE836B6288EEE42CBA774F`
