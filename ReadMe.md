<div align="center" markdown="1">

<img src=".github/meshtastic_logo.png" alt="Meshtastic Logo" width="80"/>
<h1>Ghost Firmware for Cardputer Mesh</h1>

![GitHub release downloads](https://img.shields.io/github/downloads/meshtastic/firmware/total)
[![CI](https://img.shields.io/github/actions/workflow/status/meshtastic/firmware/main_matrix.yml?branch=master&label=actions&logo=github&color=yellow)](https://github.com/meshtastic/firmware/actions/workflows/ci.yml)
[![CLA assistant](https://cla-assistant.io/readme/badge/meshtastic/firmware)](https://cla-assistant.io/meshtastic/firmware)
[![Fiscal Contributors](https://opencollective.com/meshtastic/tiers/badge.svg?label=Fiscal%20Contributors&color=deeppink)](https://opencollective.com/meshtastic/)
[![Vercel](https://img.shields.io/static/v1?label=Powered%20by&message=Vercel&style=flat&logo=vercel&color=000000)](https://vercel.com?utm_source=meshtastic&utm_campaign=oss)

<a href="https://trendshift.io/repositories/5524" target="_blank"><img src="https://trendshift.io/api/badge/repositories/5524" alt="meshtastic%2Ffirmware | Trendshift" style="width: 250px; height: 55px;" width="250" height="55"/></a>

</div>

</div>

<div align="center">
	<a href="https://meshtastic.org">Website</a>
	-
	<a href="https://meshtastic.org/docs/">Documentation</a>
</div>

## Overview

Ghost Firmware is a Cardputer Mesh build based on the upstream Meshtastic firmware. It preserves Meshtastic's LoRa mesh messaging and GPS support, and applies the Ghost splash screen plus the cyan/yellow/black palette from `Cyberpunk_2077_v1.0.0`.

The upstream firmware and its licensing/attribution are retained. This project does not include Bruce's offensive Wi-Fi/RF attack functions.

The Wi-Fi survey reports broadcast SSID, BSSID/MAC, channel, RSSI, and security mode. It cannot reveal a nearby network's password or private IP address; Network Info reports this device's IP and gateway only while connected.

Developer links:

- [PinoyUnknown](https://github.com/PinoyUnknown/)
- [unidentifiedcyberghost](https://github.com/unidentifiedcyberghost)
- [M5Burner developer page](https://burner.m5stack.com/developers/2102783541171187713)

Meshtastic enables text messaging, location sharing, and telemetry over a decentralized mesh network, making it ideal for outdoor adventures, emergency preparedness, and remote operations.

### Build the Cardputer Mesh image

Install PlatformIO, then run this from the repository root:

```sh
pio run -e m5stack-cardputer --target upload
```

The `m5stack-cardputer` alias uses the Cardputer Mesh Kit's production hardware configuration, including its LoRa radio and GPS. The build workflow publishes the firmware as `Ghost-Firmware.bin`. For local builds, the image is in `.pio/build/m5stack-cardputer/`. This command uploads the firmware over the detected serial port; add `--upload-port COMx` if PlatformIO cannot select the Cardputer automatically.

### Experimental generic M5Stack profiles

The project also includes the requested generic Arduino configuration profiles:

```sh
pio run -e m5stack-cardputer
pio run -e m5stack-core2
pio run -e m5stick-cplus
```

The Core2 and StickC Plus profiles remain experimental configuration-only starting points and are not validated firmware targets. The Cardputer profile is the exception: it aliases the supported Cardputer Mesh Kit environment.

### Get Started

- 🔧 **[Building Instructions](https://meshtastic.org/docs/development/firmware/build)** - Learn how to compile the firmware from source.
- ⚡ **[Flashing Instructions](https://meshtastic.org/docs/getting-started/flashing-firmware/)** - Install or update the firmware on your device.

Join our community and help improve Meshtastic! 🚀

## Stats

https://repobeats.axiom.co/api/embed/8025e56c482ec63541593cc5bd322c19d5c0bdcf.svg "Repobeats analytics image"
