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
| Display | 800x480 E-Ink (SSD1677) |
| Storage | SD Card |
| Wireless | WiFi 802.11 b/g/n |

---

## Features

### Reading Experience

- **EPUB 2 & 3** with image support and CSS styling
- **XTC** native pre-rendered format (supports files >2GB)
- **TXT / Markdown** with auto-wrapping and chapter detection
- **3 font families** — Bookerly, Lexend (new), Bokerlam (Vietnamese serif)
- **4 font sizes** — Small, Medium, Large, Extra Large
- **Anti-aliased grayscale** text rendering with 3 darkness levels
- **Multi-language hyphenation** support
- **4 screen orientations** with remappable buttons
- **Auto page turn** — on/off toggle + speed 1-20 pages/min (global setting)
- **Reading statistics & streaks** — per-session, daily, all-time tracking
- **KOReader Sync** for cross-device reading progress
- **Bookmarks** (starred pages) via long-press Confirm

> **Note:** Bokerlam font may not have all Unicode glyphs. Missing characters are silently hidden rather than showing placeholder symbols. Test with your books before long reading sessions.

> **Note:** Auto page turn speed is a global setting shared across all books. Adjust manually when switching between books with different reading speeds.

### Home Screen

- **Continue Reading card** — cover art + title + author + progress bar
- **Recent books grid** — 3-column cover thumbnails with progress bars
- **Bottom navigation** — Apps | Recents | Library | Settings
- **Header widgets** — clock + cached weather temperature + pet status
- **Focus mode** — hides recent books for a cleaner look

### Sleep Screens

9 modes available, configurable from the **Sleep Image** app:

| Mode | Description |
|------|-------------|
| **Dark** | Black screen (default, lowest battery) |
| **Light** | White screen |
| **Custom** | User image from `/sleep/` folder on SD card |
| **Cover** | Current book's cover art |
| **None** | Keep last screen content |
| **Cover + Custom** | Book cover with image overlay |
| **Clock** | 7-segment digital clock + lunar calendar + literary quotes |
| **Reading Stats** | Today's reading time, all-time total, current book progress |
| **Page Overlay** | PNG/BMP image composited on top of current book page |

**Page Overlay slideshow:** Put PNG images (with transparency) in `/sleep/` on your SD card. The Sleep Image app lets you preview each overlay on your current book page and cycle through them.

> **Battery warning:** Clock and Reading Stats modes require **Keep Clock Alive** enabled to update during sleep. This drains ~3-4mA continuously (~40 days on a full charge instead of months). Only enable if you actively use these sleep screen modes.

> **Note:** Sleep Refresh Interval (periodic screen updates during sleep) requires Keep Clock Alive to be ON. Both settings must be enabled together.

### Virtual Chicken Companion

Your chicken grows with every page you read.

**Evolution:** Egg > Hatchling > Youngster > Companion > Elder

| Stage | Requirements |
|-------|-------------|
| Hatchling | 1 day old + 20 pages read |
| Youngster | 3 days + 100 pages + avg hunger >= 40 |
| Companion | 7 days + 500 pages + avg hunger >= 50 + 1 book finished |
| Elder | 14 days + 1500 pages + avg hunger >= 60 |

**3 evolution variants** based on reading consistency: Scholar, Balanced, Wild.

**Reading feeds your pet:** Every 20 pages read earns a meal (+25 hunger). Reading streaks reduce the cost:

| Streak | Pages per meal |
|--------|---------------|
| 0-6 days | 20 pages |
| 7-13 days | 16 pages |
| 14-29 days | 13 pages |
| 30+ days | 10 pages |

> **Tip:** Read at least 1 page every day to maintain your streak. A single day with 0 pages resets the streak to 0.

**Care mechanics:** Hunger, happiness, and health decay over time (~1/hour). Attention calls appear every ~4 hours — 30% are fake calls that test discipline. Ignoring fake calls improves discipline; falling for them lowers it.

### Tools & Apps

| App | Description |
|-----|-------------|
| **File Transfer** | WiFi book upload from computer |
| **Clock** | Digital clock with lunar calendar and month navigation |
| **Weather** | Current weather via Open-Meteo (Hanoi / TPHCM / Da Nang) |
| **Pomodoro** | Work/break timer with pet happiness bonus |
| **Virtual Pet** | Pet care, feeding, evolution tracking |
| **Reading Stats** | Today/total/sessions summary with streak info |
| **Sleep Image** | All-in-one sleep mode selector + image picker + overlay slideshow |
| **OPDS Browser** | Browse and download from OPDS catalog servers (requires setup) |
| **Chess** | Full chess game with AI (Easy/Medium/Hard) |
| **Caro** | Gomoku (5-in-a-row) with AI |
| **Sudoku** | 9x9 puzzle generator |
| **Minesweeper** | Classic minesweeper |
| **2048** | Tile-merging puzzle game |

### Connectivity

- **WiFi book upload** — transfer books from your computer's browser
- **Calibre / OPDS** — browse and download from your Calibre library or any OPDS server
- **KOReader Sync** — sync reading progress across devices
- **WiFi OTA updates** — update firmware over-the-air
- **Weather sync** — Open-Meteo API (no account required)

> **Note:** WiFi uses 80-200mA. Charge your device before OTA updates or long sync sessions.

### Button Configuration

**Power button** supports single/double/triple click, each independently configurable:

| Action | Description |
|--------|-------------|
| Ignore | Do nothing |
| Sleep | Enter sleep mode |
| Page Turn | Turn page forward (reader only) |
| Screen Refresh | Force full e-ink refresh (clears ghosting) |
| Reading Stats | Show reading statistics |
| Star Page | Bookmark current page |
| Block Front | Lock/unlock front buttons |
| Sync Weather/Time | Connect WiFi and update weather + clock |
| Auto Page Turn | Toggle auto page turn on/off |

**Side buttons** and **front pad** layouts are remappable. Long-press side button skips chapters.

### Status Bar

Fully customizable — each element can be shown or hidden independently:

- Chapter page count (e.g., "5/42")
- Book progress percentage
- Progress bar (book-level, chapter-level, or hidden)
- Title (book title, chapter title, or hidden)
- Battery percentage
- Clock

### Display Settings

| Setting | Options | Notes |
|---------|---------|-------|
| **Refresh Frequency** | Every 1/5/10/15/30 pages | More frequent = less ghosting but slower |
| **Fading Fix** | On/Off | Compensates for sunlight-induced fading |
| **Dark Mode** | On/Off | Inverts display colors |
| **UI Theme** | Classic, Lyra, Lyra Extended, CrossPet, CrossPet Classic | Visual theme for UI |
| **Text Darkness** | Normal, Dark, Extra Dark | Adjusts rendered text intensity |
| **Text Anti-Aliasing** | On/Off | 4-level grayscale smoothing |

---

## Important Notes

### Battery Life

- **Keep Clock Alive** drains ~3-4mA continuously. Only enable for Clock or Reading Stats sleep screens. Disable otherwise.
- **Sleep Refresh Interval** requires Keep Clock Alive to be ON. Both settings work together.
- **WiFi** uses significant power (80-200mA). Charge before OTA updates.

### Reading & Files

- **Auto page turn speed** is global, not per-book. Adjust when switching books.
- **Deleting `.crosspoint/` folder** on SD card clears ALL reading progress and cached data.
- **Moving or renaming a book file** resets its reading progress (cache is tied to file path).
- **XTC files >2GB** are now supported but may have slower page seeks on the ESP32.

### Fonts

- **Bokerlam** is optimized for Vietnamese but may lack some Unicode characters. Missing glyphs are silently hidden.
- **Lexend** is a new sans-serif option designed for readability.

### Pet

- **Reading streak** resets if you go a full day without reading at least 1 page.
- **Meals require reading**, not just opening the app. Pages read automatically feed your pet.
- **Attention calls:** 30% are fake — ignoring fakes trains discipline, but ignoring real calls makes your pet unhappy.

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

### SD card setup for sleep images

Place PNG or BMP images in `/sleep/` (or `/.sleep/`) on your SD card:

```
/sleep/
├── pikachu.png        # PNG with alpha transparency (for Page Overlay mode)
├── landscape.bmp      # BMP image (for Custom sleep screen)
└── art.png            # More images = more slideshow options
```

## Contributing

Contributions welcome! See [contributing docs](./docs/contributing/README.md).

1. Fork the repo
2. Create a branch (`feature/your-feature`)
3. Make changes
4. Submit a PR

---

CrossPet Reader is **not affiliated with Xteink or any manufacturer of the X4 hardware**.

Based on [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader). Inspired by [diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader) by atomic14.
