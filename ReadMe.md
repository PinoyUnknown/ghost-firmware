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

Git history retains previous source revisions. A copy of the firmware binary
that preceded the current feature update is preserved under
`patches/archive/pre-ui-update/`. Each subsequent source update is accompanied
by a patch in `patches/`; the release binary is kept separately in `releases/`.

### Changes

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
