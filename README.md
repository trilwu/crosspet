# CrossPet

**CrossPet** is a personal fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) —
open-source firmware for the **Xteink X4** e-paper reader, built with **PlatformIO** on **ESP32-C3**.

![](./docs/images/cover.jpg)

## What's different from CrossPoint?

CrossPet extends the upstream firmware with reading-productivity, personality, and utility features that turn the X4 into more than just a book reader.

### Reading Experience
- **Smoother page turns** — anti-aliased text always uses FAST_REFRESH, eliminating the ~1s transition stutter
- **Bokerlam font** — Vietnamese-optimized serif font with full diacritics support, built-in as a selectable reading font
- **Reading time tracking** — per-session, daily, and all-time reading statistics with binary persistence

### Sleep Screens
- **Clock sleep screen** — 7-segment digital clock + calendar with lunar date display and rotating daily literary quotes (28 curated literary quotes)
- **Reading Stats sleep screen** — shows today's reading time, all-time total, and current book progress bar; resets daily at midnight
- **Reliable sleep display** — clock and reading stats refresh correctly on brief wakeups (no stale timestamp)
- **SD-based clock backup** — preserves accurate time across deep sleep even when ESP32-C3 RTC memory is lost

### Virtual Pet
- **Tamagotchi-style companion** — lives and evolves alongside your reading habit
- **Evolution system** — Egg → Hatchling → Juvenile → Adult → Elder, with 3 evolution variants (Scholar/Balanced/Wild) based on reading consistency
- **8 pet types** — Cat, Dog, Dragon, Bunny, Robot, Bear, Slime (pixel-art sprites loaded from SD)
- **Mood & needs** — hunger, happiness, energy affected by reading activity and care
- **Reading streaks** — consecutive reading days boost pet growth and unlock Scholar variant

### Home Screen
- **Redesigned home** — cover art + reading progress for recent books in top panel, 2×3 grid for quick access to all features
- **Lunar calendar** — Vietnamese lunar date displayed on clock cells and sleep screen

### Tools & Games
- **Pomodoro timer** — focus timer with configurable work/break intervals
- **BLE Presenter** — Bluetooth Low Energy slide controller for presentations
- **Daily quotes** — curated literary quote viewer
- **Conference badge** — name badge display mode
- **Photo frame** — BMP image slideshow from SD card
- **Games** — Maze, Snake, 2048, Conway's Game of Life

Everything else (EPUB rendering, KOReader Sync, WiFi upload, OTA updates) is inherited from CrossPoint.

> Fork of [crosspoint-reader/crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader). Not affiliated with Xteink.

---

## Why CrossPoint? (upstream)

Most affordable e-paper readers ship locked-down firmware. The Xteink X4 is capable hardware with a closed ecosystem.
CrossPoint opened it up:

- **Fully open-source** EPUB firmware for constrained embedded hardware (ESP32-C3, 800×480 e-ink)
- **Distraction-free reading** — configurable fonts, layouts, margins, anti-aliased text
- **KOReader Sync** — cross-device progress sync
- **Openness** — flash via browser, OTA over WiFi, web-based book upload

No subscriptions. No telemetry. No vendor lock-in.

## Features & Usage

- [x] EPUB parsing and rendering (EPUB 2 and EPUB 3)
- [x] Image support within EPUB
- [x] Saved reading position
- [x] File explorer with file picker
  - [x] Basic EPUB picker from root directory
  - [x] Support nested folders
  - [ ] EPUB picker with cover art
- [x] Custom sleep screen
  - [x] Cover sleep screen
  - [x] Clock + calendar + lunar date sleep screen with daily literary quotes
  - [x] Reading Stats sleep screen (today's time, all-time total, last book progress)
- [x] Wifi book upload
- [x] Wifi OTA updates
- [x] KOReader Sync integration for cross-device reading progress
- [x] Configurable font, layout, and display options
  - [x] Bokerlam Vietnamese serif font
  - [ ] User provided fonts
  - [ ] Full UTF support
- [x] Screen rotation
- [x] Reading time tracking (per-session, daily, all-time)
- [x] Virtual pet with evolution system and reading-based growth
- [x] BLE Presenter (Bluetooth slide controller)
- [x] Pomodoro timer
- [x] Games: Maze, Snake, 2048, Game of Life
- [x] Lunar calendar support
- [x] SD-based clock backup for reliable timekeeping

Multi-language support: Read EPUBs in various languages, including English, Spanish, French, German, Italian, Portuguese, Russian, Ukrainian, Polish, Swedish, Norwegian, [and more](./USER_GUIDE.md#supported-languages).

See [the user guide](./USER_GUIDE.md) for instructions on operating CrossPoint, including the
[KOReader Sync quick setup](./USER_GUIDE.md#365-koreader-sync-quick-setup).

For more details about the scope of the project, see the [SCOPE.md](SCOPE.md) document.

## Installing

### Web (latest firmware)

1. Connect your Xteink X4 to your computer via USB-C and wake/unlock the device
2. Go to https://xteink.dve.al/ and click "Flash CrossPoint firmware"

To revert back to the official firmware, you can flash the latest official firmware from https://xteink.dve.al/, or swap
back to the other partition using the "Swap boot partition" button here https://xteink.dve.al/debug.

### Web (specific firmware version)

1. Connect your Xteink X4 to your computer via USB-C
2. Download the `firmware.bin` file from the release of your choice via the [releases page](https://github.com/crosspoint-reader/crosspoint-reader/releases)
3. Go to https://xteink.dve.al/ and flash the firmware file using the "OTA fast flash controls" section

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

CrossPoint uses PlatformIO for building and flashing the firmware. To get started, clone the repository:

```
git clone --recursive https://github.com/crosspoint-reader/crosspoint-reader

# Or, if you've already cloned without --recursive:
git submodule update --init --recursive
```

### Flashing your device

Connect your Xteink X4 to your computer via USB-C and run the following command.

```sh
pio run --target upload
```
### Debugging

After flashing the new features, it’s recommended to capture detailed logs from the serial port.

First, make sure all required Python packages are installed:

```python
python3 -m pip install pyserial colorama matplotlib
```
after that run the script:
```sh
# For Linux
# This was tested on Debian and should work on most Linux systems.
python3 scripts/debugging_monitor.py

# For macOS
python3 scripts/debugging_monitor.py /dev/cu.usbmodem2101
```
Minor adjustments may be required for Windows.

## Internals

CrossPoint Reader is pretty aggressive about caching data down to the SD card to minimise RAM usage. The ESP32-C3 only
has ~380KB of usable RAM, so we have to be careful. A lot of the decisions made in the design of the firmware were based
on this constraint.

### Data caching

The first time chapters of a book are loaded, they are cached to the SD card. Subsequent loads are served from the 
cache. This cache directory exists at `.crosspoint` on the SD card. The structure is as follows:


```
.crosspoint/
├── epub_12471232/       # Each EPUB is cached to a subdirectory named `epub_<hash>`
│   ├── progress.bin     # Stores reading progress (chapter, page, etc.)
│   ├── cover.bmp        # Book cover image (once generated)
│   ├── book.bin         # Book metadata (title, author, spine, table of contents, etc.)
│   └── sections/        # All chapter data is stored in the sections subdirectory
│       ├── 0.bin        # Chapter data (screen count, all text layout info, etc.)
│       ├── 1.bin        #     files are named by their index in the spine
│       └── ...
│
└── epub_189013891/
```

Deleting the `.crosspoint` directory will clear the entire cache. 

Due the way it's currently implemented, the cache is not automatically cleared when a book is deleted and moving a book
file will use a new cache directory, resetting the reading progress.

For more details on the internal file structures, see the [file formats document](./docs/file-formats.md).

## Contributing

Contributions are very welcome!

If you are new to the codebase, start with the [contributing docs](./docs/contributing/README.md).

If you're looking for a way to help out, take a look at the [ideas discussion board](https://github.com/crosspoint-reader/crosspoint-reader/discussions/categories/ideas).
If there's something there you'd like to work on, leave a comment so that we can avoid duplicated effort.

Everyone here is a volunteer, so please be respectful and patient. For more details on our goverance and community 
principles, please see [GOVERNANCE.md](GOVERNANCE.md).

### To submit a contribution:

1. Fork the repo
2. Create a branch (`feature/dithering-improvement`)
3. Make changes
4. Submit a PR

---

CrossPoint Reader is **not affiliated with Xteink or any manufacturer of the X4 hardware**.

Huge shoutout to [**diy-esp32-epub-reader** by atomic14](https://github.com/atomic14/diy-esp32-epub-reader), which was a project I took a lot of inspiration from as I
was making CrossPoint.
