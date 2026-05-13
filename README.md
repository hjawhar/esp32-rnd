# esp32-rnd

ESP32-S3 firmware playground — small, focused scripts I use to learn the chip and to provision boards.

Targets the **ESP32-S3-WROOM-1 N16R8** module (16 MB flash, 8 MB octal PSRAM, native USB-OTG), specifically a 44-pin Type-C clone. Most code works on any ESP32-S3 with minor pin changes.

## What's in here

| Path | What it does |
|---|---|
| [`scripts/blink/`](scripts/blink/) | Cycle the onboard WS2812 RGB LED red → green → blue. |
| [`scripts/wifi/`](scripts/wifi/) | Join a Wi-Fi network as a station, print IP + RSSI, auto-reconnect on drop. |
| [`scripts/fleet-flash.sh`](scripts/fleet-flash.sh) | Bench script: flash N boards one after another using `esptool` only — no PlatformIO needed on the bench machine. Logs each board's MAC to `fleet-log.csv`. |
| [`install/`](install/) | A static page that flashes the firmware directly from a browser via [ESP Web Tools](https://esphome.github.io/esp-web-tools/) — zero install required for end-users. |

For the deep "why every line of `platformio.ini` looks the way it does", see [`AGENTS.md`](AGENTS.md).

## Quick start (developer side)

You need [Homebrew](https://brew.sh) PlatformIO once, on the machine that compiles firmware:

```bash
brew install platformio
```

Then for either project:

```bash
cd scripts/blink             # or scripts/wifi
pio run -t upload            # build + flash via USB
pio device monitor           # see Serial output
```

The Wi-Fi project needs credentials. Copy and edit the template (the real file is `.gitignore`d):

```bash
cp scripts/wifi/src/secrets.h.example scripts/wifi/src/secrets.h
$EDITOR scripts/wifi/src/secrets.h
```

> ⚠️ ESP32 is **2.4 GHz only**. If your SSID has `_5G` in it, use the 2.4 GHz twin instead.

## End-user install (no toolchain required)

Open the install page in Chrome/Edge:

→ **https://hjawhar.github.io/esp32-rnd/** *(once GitHub Pages is enabled — see [Publishing](#publishing-firmware) below)*

Plug in the board, click **Connect**, pick the serial port, click **Install**. Done in 30 seconds.

For CLI-comfortable folks, the same `firmware.factory.bin` can be flashed with:

```bash
pip install --user esptool
esptool write-flash 0x0 firmware.factory.bin
```

## Fleet provisioning

For batch-flashing many boards:

```bash
# 1. Build once on your dev machine
cd scripts/wifi && pio run

# 2. Run the bench script (esptool only — no PlatformIO needed for flashing)
cd .. && ./fleet-flash.sh wifi
```

Plug a board, press Enter, beep, next. Each board's MAC + timestamp lands in `fleet-log.csv` for inventory tracking.

## Hardware notes (this specific clone)

- Two USB-C ports — only the one labeled **`USB`** powers the board on this clone. The `COM`/`UART` port has no bridge chip behind it.
- The **`BOOT` button is not wired to GPIO0** on this clone. If you need to force download mode (very first flash, or after a hung firmware), bridge **`IO0` ↔ `GND`** with a wire, tap **`RST/EN`**, remove the wire, then upload.
- Onboard RGB LED is a single **WS2812 NeoPixel on GPIO 48**. Drive it with `rgbLedWrite(48, r, g, b)` from Arduino-ESP32 3.x.

## Publishing firmware

The end-user install flow expects `firmware.factory.bin` to be hosted at a stable URL. Three options:

1. **GitHub Releases** — tag a commit, attach the `.bin` from `.pio/build/<env>/firmware.factory.bin`. The install manifest points at `https://github.com/<user>/esp32-rnd/releases/download/<tag>/firmware.factory.bin`.
2. **GitHub Pages** — drop the `.bin` into a `release/` folder served by Pages alongside `install/`.
3. **CI auto-publish** — a small GitHub Actions workflow that runs `pio run`, uploads the artifacts on every `v*` git tag. Not set up yet; trivial to add.

## License

See [`LICENSE`](LICENSE) if present; otherwise treat as MIT.
