# CrossPet Reader

**Your pocket e-reader — with a virtual chicken.**

CrossPet is a Vietnamese fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) — open-source firmware for the **Xteink X4** e-paper reader, built with **PlatformIO** on **ESP32-C3**.

![](./docs/images/crosspet.png)

## Hardware

| Spec | Detail |
|------|--------|
| MCU | ESP32-C3 RISC-V @ 160MHz |
| RAM | ~380KB (no PSRAM) |
| Flash | 16MB |
| Display | 800×480 E-Ink (SSD1677) |
| Storage | SD Card |
| Wireless | WiFi 802.11 b/g/n, BLE 5.0 |

## What makes CrossPet special

### Virtual Chicken Companion
Your chicken grows with every page you read. Evolution system: Egg → Hatchling → Juvenile → Adult → Elder, with 3 variants (Scholar/Balanced/Wild) based on reading consistency. Hunger, happiness, and energy are affected by reading activity and care.

### Reading Experience
- **EPUB 2 & 3** with image support
- **Anti-aliased grayscale** text rendering
- **3 font families**, 4 sizes, 4 styles (including Bokerlam Vietnamese serif)
- **Multi-language hyphenation**
- **Reading statistics & streaks** — per-session, daily, all-time tracking
- **KOReader Sync** cross-device progress
- **4 screen orientations**, remappable buttons

### Sleep Screens
- **Clock** — 7-segment digital clock + calendar with lunar date and 28 rotating literary quotes
- **Reading Stats** — today's reading time, all-time total, current book progress
- **Cover** — book cover art display
- **Reliable refresh** — clock and stats update correctly on brief wakeups

### Tools
- **Weather** — current weather via Open-Meteo API
- **Clock with lunar calendar** — month navigation with Vietnamese lunar dates
- **Pomodoro timer** — configurable work/break intervals
- **BLE Presenter** — wireless slide controller for presentations
- **5 mini games** — Chess, Caro (Gomoku), Sudoku, Minesweeper, 2048

### Home Screen
- Cover art + reading progress for recent books in top panel
- 3-row grid: Tools, Pet | Library, Recent | Transfer, Settings
- Header clock and cached weather temperature

### Connectivity
- **WiFi book upload** and Calibre/OPDS browser
- **WiFi OTA updates**
- **BLE 5.0** remote control and presenter mode
- **SD-first layout caching** for performance

Everything else (EPUB rendering, KOReader Sync, WiFi upload) is inherited from CrossPoint.

> Fork of [crosspoint-reader/crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader). Not affiliated with Xteink.

---

## Installing

### Web (latest firmware)

1. Connect your Xteink X4 to your computer via USB-C and wake/unlock the device
2. Go to https://xteink.dve.al/ and click "Flash CrossPoint firmware"

To revert back to the official firmware, you can flash the latest official firmware from https://xteink.dve.al/, or swap
back to the other partition using the "Swap boot partition" button here https://xteink.dve.al/debug.

### Manual

See [Development](#development) below.

## Development

### Prerequisites

* **PlatformIO Core** (`pio`) or **VS Code + PlatformIO IDE**
* Python 3.8+
* USB-C cable for flashing the ESP32-C3
* Xteink X4

### Checking out the code

```
git clone --recursive https://github.com/trilwu/crosspet
```

### Flashing your device

Connect your Xteink X4 to your computer via USB-C and run:

```sh
pio run --target upload
```

### Debugging

Capture detailed logs from the serial port:

```python
python3 -m pip install pyserial colorama matplotlib
```

```sh
# Linux
python3 scripts/debugging_monitor.py

# macOS
python3 scripts/debugging_monitor.py /dev/cu.usbmodem2101
```

## Internals

The ESP32-C3 only has ~380KB of usable RAM. CrossPoint aggressively caches data to the SD card to minimize RAM usage.

### Data caching

The first time chapters of a book are loaded, they are cached to the SD card at `.crosspoint/`:

```
.crosspoint/
├── epub_12471232/       # Each EPUB cached to epub_<hash>
│   ├── progress.bin     # Reading progress
│   ├── cover.bmp        # Book cover image
│   ├── book.bin         # Book metadata
│   └── sections/        # Chapter data
│       ├── 0.bin
│       ├── 1.bin
│       └── ...
├── reading_stats.bin    # Reading statistics (v2)
└── weather_cache.json   # Weather data cache
```

Deleting `.crosspoint/` clears the entire cache. Moving a book file resets its reading progress.

For more details, see [file formats](./docs/file-formats.md).

## Contributing

Contributions welcome! See [contributing docs](./docs/contributing/README.md).

1. Fork the repo
2. Create a branch (`feature/your-feature`)
3. Make changes
4. Submit a PR

---

CrossPet Reader is **not affiliated with Xteink or any manufacturer of the X4 hardware**.

Based on [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader). Inspired by [diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader) by atomic14.
