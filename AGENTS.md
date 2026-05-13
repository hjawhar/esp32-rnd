# Agent guide — `esp32-rnd`

Reference for any AI assistant or human dev working in this repo. Captures every non-obvious decision and the reasoning behind it.

---

## 1. What this repo is

Personal ESP32-S3 firmware playground. Targets a specific Chinese clone of the official Espressif DevKitC-1 board (44-pin Type-C, ESP32-S3-WROOM-1 N16R8 module, 16 MB QSPI flash + 8 MB embedded octal PSRAM). The clone has several quirks documented below; most of them caused multiple hours of debugging the first time around.

## 2. Hardware specifics that are not obvious

| Quirk | Reality | Implication |
|---|---|---|
| `BOOT` button is not wired to GPIO0 | Confirmed empirically: holding BOOT while resetting does not pull GPIO0 low — the LED never goes dark, which it would if the chip actually entered ROM download mode | Cannot rely on `BOOT+RESET` to enter download mode. Must use a wire jumper between `IO0` and `GND` if auto-reset fails. |
| `COM`/`UART` port is non-functional | No USB-UART bridge chip behind it, doesn't even power the board | Always use the `USB` port. Don't waste time troubleshooting CH340/CP2102 drivers. |
| Onboard RGB LED is on GPIO 48 | Standard for DevKitC-1, and this clone follows it | Use `rgbLedWrite(48, r, g, b)`. NeoPixel one-wire protocol — `digitalWrite` does nothing. |
| `_5G` Wi-Fi SSIDs invisible | ESP32-S3 has 2.4 GHz radio only | Don't put a `_5G` SSID in `secrets.h` and waste time. Most routers broadcast a separate 2.4 GHz SSID with the same name minus the suffix. |
| Embedded PSRAM, not external | esptool reports `Embedded PSRAM 8MB (AP_3v3)` — so this is an ESP32-S3R8 chip, not a separate PSRAM die | Memory type must be `qio_opi`. Wrong setting → app panics at boot, no Serial output, looks dead. |

## 3. Tooling

### Build: PlatformIO via Homebrew
**Do NOT use** the VSCode PlatformIO extension — its bundled installer downloads a portable Python from a slow server (30 KB/s sustained, takes forever). Use:

```bash
brew install platformio
```

The `pio` CLI works from any editor. For C/C++ IntelliSense in VSCode use the Microsoft C/C++ extension and point it at `.pio/build/<env>/compile_commands.json`.

### Platform: `pioarduino` fork, NOT stock `espressif32`
The official `platformio/espressif32` platform pulls toolchain tarballs from `dl.registry.platformio.org`, which (at least from this network) gets routed to a Singapore CDN mirror that crawls at ~30 KB/s. The 246 MB `framework-arduinoespressif32` would take 3+ hours.

The community **`pioarduino`** fork hosts the same artifacts on GitHub Releases at ~10 MB/s. Full clean build in ~8 minutes.

Both `platformio.ini` files in this repo pin the platform to:
```ini
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.38-1/platform-espressif32.zip
```
Check [pioarduino releases](https://github.com/pioarduino/platform-espressif32/releases) for newer tags before bumping.

### Board variant
**Always `esp32-s3-devkitc1-n16r8`**, not `esp32-s3-devkitc-1`. The stock `esp32-s3-devkitc-1` is an **N8 (no PSRAM) variant** — using it with this hardware causes the bootloader to be built without PSRAM support, app panics at boot, looks like nothing happened. We verified this by reading eFuses with `espefuse` and confirming the chip reports embedded PSRAM.

The N16R8 variant board JSON automatically sets:
- `partitions = default_16MB.csv`
- `memory_type = qio_opi`
- `-DBOARD_HAS_PSRAM`

## 4. USB / serial behavior

This is where most of the debugging time went.

### Config: `USB_MODE=0` + `CDC_ON_BOOT=1`
```ini
build_flags =
    -DARDUINO_USB_MODE=0
    -DARDUINO_USB_CDC_ON_BOOT=1
```

- `USB_MODE=0` → use the chip's **hardware USB-Serial/JTAG ROM peripheral** (HWCDC) instead of TinyUSB on USB-OTG. PID `0x303A:0x1001`. Always available regardless of firmware state.
- `CDC_ON_BOOT=1` → map `Serial` to the USB-Serial/JTAG interface so `Serial.println` shows up on `/dev/cu.usbmodem*`. Without this, `Serial` defaults to UART0 (GPIO 43/44), invisible to host.

### Why `USB_MODE=1` (TinyUSB on OTG) was avoided
With TinyUSB on USB-OTG, the firmware itself manages USB. If user code wedges, USB dies and esptool can't reach the chip → wire-jumper required to recover. The HWCDC path uses ROM hardware that stays alive even if firmware crashes.

### Esptool reset options
```ini
upload_flags =
    --before
    usb_reset
```

- `usb_reset` sends a USB control transfer to the ROM peripheral → hardware reset, doesn't need firmware cooperation.
- `default_reset` (UART-bridge DTR/RTS) is a no-op on this chip — there's no bridge.
- esptool prints `Hard resetting via RTS pin...` after a flash, but on this chip that's a **misleading log line**. The actual reset is via USB. Sometimes the chip doesn't get reset cleanly; if Serial monitor sees no output, **unplug + replug the cable** for a true cold boot.

### Mandatory: `Serial.setTxTimeoutMs(0)` (or short timeout)
Arduino-ESP32 `Serial.print` (USBCDC or HWCDC) **blocks for up to 250 ms when the TX buffer fills** waiting for the host to drain. If no monitor is attached or it's slow, a few hundred prints in quick succession wedges the entire chip — the USB-CDC handler can't process esptool's reset signals, so you're back to the wire jumper.

**Always add this in `setup()` for any project that prints in a tight loop:**
```cpp
Serial.begin(115200);
Serial.setTxTimeoutMs(0);    // drop prints if host isn't reading, never block
```

Exception: only safe to omit if your prints are slow (every few seconds) **and** you only print from `loop()`, not from interrupt/event handlers that could fire in bursts.

## 5. The wire-jumper trick (manual download mode)

When auto-flash fails (`Failed to connect to ESP32-S3: No serial data received`), the chip isn't entering download mode and needs to be forced. On this clone:

1. Bridge **`IO0`** (the pin labeled `0` on the header) **to `GND`** with a wire / paperclip
2. Tap **`RST/EN`** while the wire is in contact with both pins
3. Carefully remove the wire (don't touch other pins)
4. `pio run -t upload`

⚠️ Never let the wire touch `3V3`, `5V`, `VIN`, `VBUS`, or `BAT` — shorts to GND will damage the board.

This is needed:
- The very first flash if the chip's existing firmware doesn't expose USB-CDC reset
- After flashing a firmware that wedges (blocking Serial, infinite loop, panic)
- Anytime auto-reset fails for any reason

When the firmware in this repo is running normally, the wire jumper is not needed — `usb_reset` works.

## 6. Per-project notes

### `scripts/blink/`
Onboard NeoPixel cycle on GPIO 48. Uses `rgbLedWrite()` (Arduino-ESP32 3.x built-in, no library deps). 18-line `main.cpp`.

### `scripts/wifi/`
Wi-Fi station, prints assigned IP + RSSI on connect, auto-reconnects on disconnect. Credentials in `src/secrets.h` (gitignored). Template at `src/secrets.h.example`.

WiFi quirks:
- `WiFi.onEvent(...)` is preferred over polling `WiFi.status()` — connect is async
- `setAutoReconnect(true)` lets the framework retry on its own
- ESP32 is **2.4 GHz only** — if SSID isn't in `WiFi.scanNetworks()` output, it's almost certainly a 5 GHz network
- `WL_DISCONNECTED` status (6) can mean either "trying" or "gave up" — driver-dependent; if stuck on it for 20+s and the SSID is in scan results, the password is probably wrong

### `scripts/fleet-flash.sh`
Bench-flashing script for provisioning N boards. Wraps `esptool` directly — no PlatformIO needed at flash time. Logs MAC to `fleet-log.csv`.

Why a shell script + esptool, not `pio run -t upload`:
- esptool dep is `pip install esptool`, ~15 MB
- PlatformIO + Arduino-ESP32 toolchain is ~500 MB
- For bench machines that just flash, esptool alone is enough
- The script auto-detects new ports as boards are plugged, so the operator just plugs/unplugs

## 7. End-user distribution via ESP Web Tools

`install/` contains a static page that flashes firmware directly from the browser. No installation on the user's machine — just Chrome or Edge.

- `install/index.html` — minimal page with an `<esp-web-install-button>`
- `install/manifest.json` — points to the `firmware.factory.bin` for each project

The flow:
1. Build firmware: `pio run` in a project — produces `.pio/build/<env>/firmware.factory.bin`
2. Upload that file to GitHub Releases (or any HTTPS host)
3. Update the URL in `install/manifest.json`
4. Host `install/` on GitHub Pages
5. End user visits the page, clicks Connect, picks port, clicks Install — done

This works because ESP Web Tools uses the browser's WebSerial API + a JS port of esptool to flash without any installed tooling.

CI auto-publish (not yet set up): a GitHub Actions workflow on `v*` tags that runs `pio run` and uploads the factory bin to the release. Trivial to add when ready.

## 8. Troubleshooting cheat sheet

| Symptom | Cause | Fix |
|---|---|---|
| `pio run` framework download stuck at 1%, ETA 3+ hours | Slow PlatformIO CDN mirror | Pin `platform = .../pioarduino/.../platform-espressif32.zip` (already set) |
| `Failed to connect to ESP32-S3: No serial data received` | Chip not in download mode; auto-reset failed or firmware wedged | Wire-jumper trick (§5) |
| `Could not open ... port is busy or doesn't exist` | Port name changed (firmware↔ROM transition); or `pio device monitor` still has it open | Remove `upload_port` from `platformio.ini` (auto-detect), close monitor first |
| Flash verifies byte-perfect but firmware never runs | Wrong board variant → bootloader/app PSRAM mismatch | Use `board = esp32-s3-devkitc1-n16r8` (already set) |
| `Hard resetting via RTS pin...` printed, but no Serial output appears | Misleading log; reset didn't actually take | Unplug + replug for a true cold boot |
| LED appears "stuck on a color" | NeoPixel retains its last latched value when no new data is written. Doesn't necessarily mean firmware crashed | Drive the pin explicitly somewhere in `setup()` or `loop()` |
| `Serial.print` works for a few seconds then hangs the chip | TX buffer full + default 250 ms timeout blocks every print | Add `Serial.setTxTimeoutMs(0)` in `setup()` |
| `pio device monitor` shows monitor header then nothing | Boot prints already happened before monitor attached, and there's no loop print | Press `RST/EN` with monitor open, or add periodic `Serial.println` in `loop()` |
| Wi-Fi `WL_NO_SSID_AVAIL` or stuck at `WL_DISCONNECTED` | SSID is 5 GHz only, or typo in `secrets.h`, or out of range | Use `WiFi.scanNetworks()` to list visible 2.4 GHz APs; use SSID without `_5G` suffix |

## 9. When adding a new script

Follow the structure of `scripts/blink/` or `scripts/wifi/`:

```
scripts/<name>/
├── platformio.ini      # always pin pioarduino platform + n16r8 board
└── src/
    └── main.cpp        # call Serial.setTxTimeoutMs(0) in setup()
```

If it has secrets:
```
scripts/<name>/
└── src/
    ├── main.cpp
    ├── secrets.h           # gitignored
    └── secrets.h.example   # committed
```

Update the top-level `README.md` table and add an entry in `install/manifest.json` so end users can flash the new firmware via the browser.

## 10. Things to NOT do

- Don't use `board = esp32-s3-devkitc-1` — that's the N8 variant.
- Don't use `platform = espressif32` (stock) — slow CDN.
- Don't omit `Serial.setTxTimeoutMs(0)` if you're printing in a loop.
- Don't put real Wi-Fi credentials, API keys, or anything secret directly in source files — use `secrets.h` (gitignored).
- Don't trust esptool's "Hard resetting via RTS pin..." message on this chip — verify with `ls /dev/cu.usbmodem*` whether the port name changed to the MAC-based form (firmware running) or stayed at the short form (still in ROM).
- Don't commit `firmware.bin` or anything in `.pio/` — already in `.gitignore`.

---

For absolute first-time setup and a "why is this so painful" narrative, the original debug log is in commit history. This file is the distilled "what's true now" version.
