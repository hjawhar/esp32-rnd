#!/usr/bin/env bash
# Bench flasher for ESP32-S3 N16R8 boards.
# Usage:  ./fleet-flash.sh <project>     # e.g. ./fleet-flash.sh wifi
# Plug a board, it flashes, beeps, prompts for the next one.

set -euo pipefail

PROJECT="${1:?usage: $0 <project-dir>}"
BIN="$(dirname "$0")/$PROJECT/.pio/build/n16r8/firmware.factory.bin"
[[ -f "$BIN" ]] || { echo "no firmware: $BIN — did you 'pio run' yet?"; exit 1; }

count=0
trap 'echo; echo "flashed $count board(s)."' EXIT

while true; do
    echo
    read -rp "[$((count+1))] plug in next board, press ENTER (q to quit): " ans
    [[ "$ans" == "q" ]] && break

    # Wait briefly for the port to appear
    for _ in {1..40}; do
        port=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
        [[ -n "${port:-}" ]] && break
        sleep 0.25
    done
    [[ -z "${port:-}" ]] && { echo "no /dev/cu.usbmodem* found, retry"; continue; }

    echo "→ flashing $port from $BIN"
    if esptool --port "$port" --before usb_reset write-flash 0x0 "$BIN"; then
        count=$((count+1))
        printf '\a'                 # terminal bell on success
        # Record the board's MAC for inventory tracking
        mac=$(esptool --port "$port" --before no_reset --after no_reset read-mac 2>/dev/null | awk '/^MAC:/ {print $2}')
        echo "✓ board $count flashed — MAC=$mac"
        echo "$(date -u +%FT%TZ),$mac,$PROJECT" >> "$(dirname "$0")/fleet-log.csv"
    else
        echo "✗ flash failed, leave board plugged in and retry, or 'q' to quit"
    fi
done
