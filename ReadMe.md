<div align="center">
  <h1>Ghost Firmware</h1>
  <strong>Cyberpunk-themed passive wireless and device diagnostics</strong>
</div>

## Overview

Ghost Firmware is an open-source ESP32 field-diagnostics interface using the
Cyberpunk theme assets in this repository. The verified Cardputer build offers
passive Wi-Fi and BLE surveys, a UTC clock, and a read-only browser for the
device's internal flash filesystem.

This project does not implement Wi-Fi or BLE deauthentication, flooding,
credential capture, unauthorized access, or other disruptive operations.
Wireless surveys only receive nearby broadcast information; they do not connect
to or modify other devices.

The firmware uses the license in [LICENSE](./LICENSE). Third-party components
retain their own licenses and notices.

## Cardputer controls

- `W` / `S` or `Fn` + arrow keys: move through the home menu or a list.
- `Enter`: open a tool or selected item's details.
- `R`: repeat a Wi-Fi or BLE scan.
- `Esc` / `Backspace`: go back. In the file browser, `Esc` moves to the parent
  folder before leaving the browser.
- `BtnA`: open the selected tool/item.

The Cardputer arrow combinations are `Fn` + `,` (left), `;` (up), `.` (down),
and `/` (right). The keyboard has to be detected at startup for menu navigation.

## Available functions

| Tool | Function |
| --- | --- |
| Wi-Fi | Passive scan; shows SSID, BSSID, RSSI, channel, and security mode. Up to 64 results are retained per scan. |
| BLE | Passive scan for advertisements; shows the advertised name, address, RSSI, service count/first UUID, and manufacturer-data size. Up to 64 results are retained per scan. |
| Clock | Displays UTC time; `N` attempts NTP using an already saved Wi-Fi configuration; `M` allows manual entry as `YYYYMMDDhhmmss`. |
| File | Read-only browser for the internal SPIFFS flash filesystem, with text preview. It does not expose a `/root` Linux filesystem. |
| RF / NRF / NFC / IR / GPS / FM | Status only where applicable; additional radio/sensor hardware and board-specific integration are required. These are not presented as working scanners in this build. |
| MISC | Describes the functions currently available in this firmware. |

The standard Cardputer target configuration does not declare an SD-card slot or
SD-card wiring, so this build cannot browse an SD card. The file browser uses
the internal flash partition defined by the Cardputer build's partition table.

## Compatibility

| Device / PlatformIO environment | Status | Notes |
| --- | --- | --- |
| M5Stack Cardputer / `m5stack-cardputer` | Supported; build verified | UI, onboard keyboard, passive Wi-Fi/BLE surveys, UTC clock, internal flash browser. Hardware behavior still requires testing on the actual device. |
| M5Stack Core2 / `m5stack-core2` | Experimental build profile | Not verified as a working device target; Cardputer keyboard controls are not available. |
| M5StickC Plus / `m5stick-cplus` | Experimental build profile | Not verified as a working device target; Cardputer keyboard controls are not available. |
| Other ESP32 boards | Not supported by this target | Screen, buttons, radio peripherals, and pin assignments need board-specific integration and validation. |

Build or upload the Cardputer target from the repository root:

```sh
pio run -e m5stack-cardputer
pio run -e m5stack-cardputer --target upload
```

PlatformIO writes the image to
`.pio/build/m5stack-cardputer/firmware.bin`. Add `--upload-port COMx` if the
upload port is not detected automatically.

## Disclaimer

Ghost Firmware is provided for lawful, authorized diagnostics and development.
Only use it on devices, networks, and radio equipment you own or have explicit
permission to assess. The verified build is limited to passive wireless
observation and local device functions; it does not disrupt networks or collect
credentials. The software is provided under the repository license, without
warranty. Users are responsible for complying with applicable laws and
regulations.

## Version backups and update history

Every version of Ghost Firmware and Email Firmware is archived version-by-version and time-by-time under `backups/`, including full source snapshots, compiled `.bin` images, and verified SHA-256 checksums:

| Version | Date & Time (UTC+8) | Commit / Source | Binary Path & Size | SHA-256 Checksum |
| --- | --- | --- | --- | --- |
| **Ghost v1.2.0** (Current) | 2026-10-18 14:48 | `src/ghost_app/main.cpp` | `releases/Ghost-Firmware.bin` (1,766,544 B) | `9726E108A095375834EA31BE613B5FF0E623F50D1329AD55B37426AFAE7B0034` |
| **Ghost v1.1.0** | 2026-09-27 10:50 | `e6e242a` | `backups/v1.1.0_2026-09-27_1050_fullscreen-diagnostics/` (1,655,568 B) | `CD2791575DC0CE7274FB20E0128D3179A53E7814BBD3972CE09F27787071DDAC` |
| **Ghost v1.0.1** | 2026-09-27 10:01 | `467a79c` | `backups/v1.0.1_2026-09-27_1001_ble-survey/` (1,602,576 B) | `4168D4917F9DBB757C12624924FD97BAAF85C4953888CBC2AC60E5F7E4F5CF2D` |
| **Ghost v1.0.0** | 2026-09-27 08:12 | `12dba09` / `11dcc03` | `backups/v1.0.0_2026-09-27_0812_cyberpunk-menu/` (1,042,752 B) | `15901B682B2CDB71C39261956DD65BC1844491E7F43FCE483E0E53750C42E52E` |
| **Ghost v0.1.0** | 2026-09-26 18:50 | `065cff5` | `backups/v0.1.0_2026-09-26_1850_initial-dashboard/` (875,584 B) | `59A4482A3957E45DD505CF8A536DA5E0A5C563A3D7114F7FAD82E0E7D2A40A5C` |
| **Email v1.1.0** (Current) | 2026-10-18 16:24 | `Email-Firmware/src/main.cpp` | `releases/Email-Firmware.bin` (1,025,696 B) | `01FF5303DB35B341F0E90843CBEAB7F430A9C2D14AA746F177177813F3846AC9` |
| **Email v1.0.0** | 2026-09-27 12:20 | `Email-Firmware/src/main.cpp` | `backups/email-v1.0.0_2026-09-27_1220_email-client/` (1,025,008 B) | `62415874609B78B3340BD2F6B55DA42C624F8C23000A93EFCC8E665627970663` |

### Changes

- **2026-10-18 — v1.2.0 Universal Hardware & Full Diagnostic Suite:**
  - **Universal Cardputer Keyboard Driver:** added native support for the M5Stack Cardputer v1.0/v1.1 74HC138 GPIO scanning matrix (`G8, G9, G11` -> `G13, G15, G3, G4, G5, G6, G7`) alongside the Cardputer-Adv TCA8418 I2C controller (`0x34`). Resolves the hardware freeze where the keyboard was not detected.
  - **Non-blocking Button A Fallback:** short-pressing `BtnA` (`G0`) selects/opens; holding `BtnA` (>500ms) acts as `Esc` to go back or cycle home apps, guaranteeing the device never gets stuck on a screen.
  - **All 14 Tools Working:**
    - `CONNECT`: Interactive Wi-Fi station manager, IP/DNS/Gateway details, network connectivity test (`pool.ntp.org`), and connect/disconnect.
    - `CONFIG`: Interactive NVS settings (5 color themes, animated matrix rain / scanline backdrops, screen brightness 25-100%, key click audio beep).
    - `CLOCK`: Live UTC time + uptime, Wi-Fi NTP sync, manual `YYYYMMDDhhmmss` setter.
    - `BLE`: Passive survey with detailed advertiser inspector and `L` key SPIFFS log export (`/ble_scan.txt`).
    - `WIFI`: Passive survey with detailed AP inspector, `C` key direct network join, and `L` key SPIFFS log export (`/wifi_scan.txt`).
    - `RF / NRF / NFC / GPS / FM`: Live Grove HY2.0-4P I2C bus scanner (`G2/G1`) with module probe and address detection.
    - `MISC`: 2-page system telemetry (CPU, RAM, Flash, Battery voltage/%, MACs, keyboard driver mode, dev branding).
    - `JS`: Automated 6-stage hardware self-test macro with `/selftest.txt` report export.
    - `IR`: Built-in GPIO 44 infrared transmitter testing with NEC diagnostic frames.
    - `FILE`: SPIFFS flash browser with auto-format on first boot, readme file, and text log previewer.
  - **Email-Firmware Keyboard Fix:** updated Email Firmware with dual 74HC138 + TCA8418 keyboard scanning so all Cardputer revisions can enter passphrases and compose emails.
- **2026-09-27 — Cardputer full-screen diagnostics:** expanded Wi-Fi and BLE
  result lists and detail views; added a seconds-refreshing UTC clock with
  NTP/manual setting and a scrollable, read-only internal-flash file browser;
  tool screens no longer display the home theme artwork.
- **2026-09-27 — Passive BLE survey and key hints:** added passive BLE scanning
  and documented keyboard navigation.
- **2026-09-27 — Initial standalone Cardputer app:** added the Cyberpunk home
  screen, passive Wi-Fi survey, build environment, and firmware image workflow.

## Developers

- [PinoyUnknown](https://github.com/PinoyUnknown/)
- [unidentifiedcyberghost](https://github.com/unidentifiedcyberghost)
- [Instagram: @pinoyunknown](https://www.instagram.com/pinoyunknown)

## Email Firmware

The separate `Email-Firmware` PlatformIO project is an experimental M5Stack
Cardputer email client; it does not replace or share the Ghost Firmware app.
It can submit a plain-text email to one explicitly entered recipient through
authenticated SMTPS and browse/read a small IMAP inbox over verified TLS. It
requires Wi-Fi with internet access and a mail provider that allows app-password
authentication over implicit TLS (SMTP port 465 and IMAP port 993). OAuth-only
accounts, SMTP STARTTLS-only servers, HTML-only message rendering, and
attachments are not supported.

The client never offers anonymous or spoofed sending. The sender is the
authenticated account, and the device requires a separate unlock phrase of at
least 12 characters. Saved Wi-Fi and mail settings are encrypted in NVS using
AES-256-GCM and a PBKDF2-HMAC-SHA256 key derived from that phrase. The phrase
itself is not written to storage. This protects the stored envelope but does not
make a device physically resistant to compromise; use a dedicated app password
with the minimum provider permissions and remove it if the device is lost.
Messages are plain text, are shown for review, and require a separate send
confirmation.

### Email Firmware compatibility

| Device | Status | Notes |
| --- | --- | --- |
| M5Stack Cardputer / `cardputer` | Build verified | ESP32-S3, onboard 240x135 display and TCA8418 keyboard. Device behavior and live provider interoperability still require hardware testing. |
| M5Stack Core2 | Unsupported | No Core2 input mapping or target configuration in this email app. |
| M5StickC Plus | Unsupported | No StickC Plus input mapping or target configuration in this email app. |
| Other ESP32 boards | Unsupported | Display, controls, and board-specific configuration are not integrated. |

Create an unlock phrase, then enter the Wi-Fi SSID/password (an empty password
is accepted for open Wi-Fi), SMTP hostname, IMAP hostname, account email, and
provider-issued app password. The account settings menu allows replacing these
values while the device is unlocked. The inbox lists up to the latest eight
messages and reads a bounded plain-text body; large messages may be truncated.
The Cardputer needs working internet time synchronization to validate TLS
certificates.

Build or upload from the repository root:

```powershell
pio run -d Email-Firmware -e cardputer
pio run -d Email-Firmware -e cardputer --target upload
```

PlatformIO writes the app image to
`Email-Firmware/.pio/build/cardputer/firmware.bin`. Specify
`--upload-port COMx` if PlatformIO cannot identify the Cardputer port. The
merged distributable image is generated by the
`Build Email Firmware` GitHub Actions workflow and attached as an artifact (and
to published releases). The current local image is
[releases/Email-Firmware.bin](./releases/Email-Firmware.bin), with its checksum
in [releases/Email-Firmware.bin.sha256](./releases/Email-Firmware.bin.sha256).

### Changes

- **2026-09-27 — Initial Cardputer email client:** added the animated Ghost
  splash, encrypted account setup, Wi-Fi connection, verified-TLS SMTP sending,
  explicit send review/confirmation, and IMAP inbox/message reading.
