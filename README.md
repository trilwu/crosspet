# CrossPet Reader

**Custom firmware for Xteink X4 e-reader — custom fonts, flashcards, Bluetooth keyboard, and more.**

CrossPet is a Vietnamese fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) — open-source firmware for the **Xteink X3 / X4** e-paper readers.

![](./docs/images/crosspet.png)

---

## What's New in v1.8.3

- **Custom fonts from SD card** — Drop `.bin` fonts into `/fonts/`, select in settings. Dual font model: primary + supplement for mixed scripts (e.g. Latin + CJK)
- **Flashcards** — SM-2 spaced repetition. Import decks from SD card (`/flashcards/`, TSV format)
- **Bluetooth keyboard (beta)** — Pair a BLE page turner or keyboard. Separate firmware build due to memory constraints

---

## Features

### Reading
- EPUB 2/3, XTC (>2GB), TXT/Markdown
- 3 built-in fonts + custom fonts from SD card
- 4 sizes, anti-aliased grayscale, font cache
- Auto page turn, bookmarks, reading stats & streaks
- KOReader Sync, 4 orientations, hyphenation

### Apps
| App | Description |
|-----|-------------|
| **Flashcards** | SM-2 spaced repetition with SD card deck import |
| **Virtual Pet** | Tamagotchi chicken — evolves with reading |
| **Clock** | Digital clock + lunar calendar |
| **Weather** | Open-Meteo (no account needed) |
| **Pomodoro** | Work/break timer |
| **Sleep Image** | 9 sleep screen modes with image picker |
| **OPDS Browser** | Browse & download from Calibre/OPDS |
| **Games** | Chess, Caro, Sudoku, Minesweeper, 2048 |

### Connectivity
- WiFi file transfer from browser
- OPDS / Calibre library browsing
- KOReader Sync
- OTA firmware updates
- BLE keyboard (beta, separate build)

### Bluetooth Keyboard (Beta)

Pair a BLE page turner or keyboard. Available as a **separate firmware download**.

| Key | Reader | Menus |
|-----|--------|-------|
| Arrow keys | Page turn | Navigate |
| Enter | Select | Select |
| Escape | Back | Back |

Supports: GameBrick V1/V2, Free2/Free3, Kobo Remote, generic HID.

> BLE uses ~50KB RAM. The BLE build disables images and CSS to fit. WiFi and BLE share one radio — they can't run simultaneously.

---

## Installing

### Web Flasher

1. Connect X3/X4 via USB-C, wake the device
2. Go to https://xteink.dve.al/ and flash

### Manual

```sh
git clone --recursive https://github.com/trilwu/crosspet
pio run --target upload
```

### Build Environments

```bash
pio run -e default      # Standard build
pio run -e ble          # Bluetooth build (beta)
pio run -e gh_release   # Release build
```

---

## SD Card Setup

```
/fonts/          # Custom fonts (Xteink .bin format: FontName_size_WxH.bin)
/flashcards/     # Flashcard decks (TSV: front<tab>back per line)
/sleep/          # Sleep screen images (PNG/BMP)
```

---

## Contributing

1. Fork → branch → changes → PR

See [contributing docs](./docs/contributing/README.md).

---

CrossPet Reader is **not affiliated with Xteink**. Based on [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader).
