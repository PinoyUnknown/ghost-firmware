<div align="center" markdown="1">

<h1>Ghost Firmware</h1>

</div>

<div align="center">
	<strong>Cyberpunk-inspired wireless awareness for M5Stack devices</strong>
</div>

## Overview

Ghost Firmware is a standalone diagnostic UI for M5Stack devices, with a Ghost boot splash, animated themes, and a **passive-only** Wi-Fi survey. It reports broadcast network names, signal strength, channel, and security mode. It does not transmit wireless management frames, connect to access points, or modify other devices. This standalone app does not provide LoRa mesh messaging; use the separate `m5stack-cardputer-adv` environment for the Mesh Kit firmware.

Developer links:

- [PinoyUnknown](https://github.com/PinoyUnknown/)
- [unidentifiedcyberghost](https://github.com/unidentifiedcyberghost)
- [Instagram: @pinoyunknown](https://www.instagram.com/pinoyunknown)

### Build the Cardputer Mesh image

Install PlatformIO, connect the target device, and run this from the repository root:

```sh
pio run -e m5stack-cardputer --target upload
```

The GitHub Actions workflow publishes a `Ghost-Firmware.bin` artifact. A local build image is written to `.pio/build/m5stack-cardputer/firmware.bin`. Add `--upload-port COMx` if PlatformIO does not select the device automatically.

### Other profiles

The generic Core2 and StickC Plus profiles are configuration-only and are not yet supported Ghost firmware targets:

```sh
pio run -e m5stack-cardputer
pio run -e m5stack-core2
pio run -e m5stick-cplus
```


### Get Started

- 🔧 **[Building Instructions](https://meshtastic.org/docs/development/firmware/build)** - Learn how to compile the firmware from source.
- ⚡ **[Flashing Instructions](https://meshtastic.org/docs/getting-started/flashing-firmware/)** - Install or update the firmware on your device.

## Stats

https://repobeats.axiom.co/api/embed/8025e56c482ec63541593cc5bd322c19d5c0bdcf.svg "Repobeats analytics image"
