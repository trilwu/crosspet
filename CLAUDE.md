# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

CrossPet is a Vietnamese fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) — open-source ESP32-C3 firmware for the Xteink X4 e-paper reader. Built with PlatformIO, C++17 (gnu++2a), Arduino framework. Current version: defined in `platformio.ini` under `[crosspoint] version`.

**Hardware constraints:** ESP32-C3 RISC-V @ 160MHz, ~380KB RAM (no PSRAM), 16MB flash, 800x480 E-Ink display (SSD1677), SD card storage.

## Build Commands

```bash
# Build (default env with serial logging)
pio run

# Build specific environment
pio run -e default        # Development build (serial log enabled)
pio run -e slim           # Release build (no serial, smaller)
pio run -e ble            # Bluetooth-enabled build
pio run -e gh_release     # GitHub release build

# Upload to device via USB
pio run -t upload

# Serial monitor
pio run -t monitor        # or: pio device monitor

# Static analysis
pio check

# Clean build artifacts
pio run -t clean
```

**Pre-build scripts** (run automatically by PlatformIO):
- `scripts/build_html.py` — generates HTML headers from web UI templates
- `scripts/gen_i18n.py` — generates I18n string tables from YAML translations
- `scripts/patch_jpegdec.py` — patches JPEGDEC library for ESP32-C3

## Testing

Tests live in `test/` but are native evaluation scripts, not unit test suites:
- `test/hyphenation_eval/` — hyphenation quality evaluation
- `test/utf8_nfc/` — UTF-8 NFC normalization tests
- `test/epubs/` — sample EPUB files for manual testing

```bash
# Run hyphenation evaluation
cd test && bash run_hyphenation_eval.sh
```

There is no automated test framework (no `pio test`). Verify changes by building successfully and testing on device.

## Architecture

### Layer Model (top to bottom)

```
UI Activities → ActivityManager → Subsystems (Pet, EPUB, Network) → Settings/State → HAL → Hardware
```

### Key Architectural Patterns

- **Activity system**: Android-like stack-based navigation. All screens inherit from `Activity` base class. `ActivityManager` is a singleton managing lifecycle, render task, and a shared FreeRTOS mutex. Activities use `startActivityForResult()` / `setResult()` / `finish()` pattern.
- **Singleton pattern**: Settings (`CrossPointSettings`, `CrossPetSettings`), `CrossPointState`, `PetManager`, `ActivityManager` all use `getInstance()`.
- **Render pipeline**: Single FreeRTOS task with mutex (`RenderLock` RAII). Activities call `requestUpdate()` to trigger redraws. Display uses partial/full refresh modes for e-ink.
- **Persistence**: JSON via `JsonSettingsIO` for settings, binary serialization for reading stats and pet state, all stored on SD card.

### Source Layout

**`src/`** (application code, ~33K LOC):
- `main.cpp` — entry point, HAL init, activity/pet manager setup, main loop
- `activities/` — UI screens organized by domain: `reader/`, `home/`, `settings/`, `tools/`, `network/`, `boot_sleep/`, `browser/`
- `pet/` — virtual pet engine: `PetManager`, `PetDecayEngine`, `PetEvolution`, `PetActions`, `PetCareTracker`, `PetSpriteRenderer`
- `components/` — `UITheme`, icon bitmaps, theme assets
- `network/` — `CrossPointWebServer` (HTTP + WebSocket), `OtaUpdater`, `WebDAVHandler`, generated HTML in `html/`
- `util/` — `LunarCalendar`, `ButtonNavigator`, `QrUtils`, `SleepScreenCache`

**`lib/`** (libraries, ~419K LOC — mostly font glyph data):
- `EpdFont` — font engine with pre-compiled glyph tables (~299K LOC, mostly data)
- `GfxRenderer` — e-ink graphics abstraction (orientation, dithering, grayscale)
- `Epub` — EPUB 2/3 parser with caching, CSS, cover generation
- `I18n` — internationalization (18 languages, 419+ keys, generated from YAML)
- `hal/` — hardware abstraction: `HalDisplay`, `HalStorage`, `HalPowerManager`, `HalGPIO`, `HalSystem`
- Third-party: `expat` (XML), `picojpeg`, `uzlib` (deflate)

### Flash Partitions

Dual OTA scheme: two 6.25MB app partitions (`app0`/`app1`), 3.375MB SPIFFS, 64KB coredump. Max firmware size: ~6.25MB.

### Memory Constraints

~380KB total RAM, no PSRAM. TLS buffers reduced to 4KB (from 16KB default) to fit WiFi + TLS in ~46KB free heap. PNG buffer capped at 1024px width. Single display buffer mode.

## Key Files for Common Tasks

| Task | Files |
|------|-------|
| Add new activity/screen | `src/activities/`, inherit from `Activity`, register in `ActivityManager` |
| Add new setting | `src/CrossPointSettings.h`, `src/SettingsList.h`, `src/activities/settings/` |
| Add pet feature | `src/pet/PetManager.h`, `src/pet/PetActions.h` |
| Add i18n string | `lib/I18n/translations/*.yaml`, then build (auto-generates) |
| Modify web UI | `src/network/html/` templates, `scripts/build_html.py` regenerates headers |
| Add sleep screen mode | `src/activities/boot_sleep/SleepActivity.cpp`, `src/CrossPointSettings.h` |
| Change button behavior | `src/MappedInputManager.cpp`, `src/CrossPointSettings.h` |

## Workflows

- Primary workflow: `./.claude/rules/primary-workflow.md`
- Development rules: `./.claude/rules/development-rules.md`
- Orchestration protocols: `./.claude/rules/orchestration-protocol.md`
- Documentation management: `./.claude/rules/documentation-management.md`

## Hook Response Protocol

### Privacy Block Hook (`@@PRIVACY_PROMPT@@`)

When a tool call is blocked by the privacy-block hook, the output contains a JSON marker between `@@PRIVACY_PROMPT_START@@` and `@@PRIVACY_PROMPT_END@@`. **You MUST use the `AskUserQuestion` tool** to get proper user approval.

1. Parse the JSON from the hook output
2. Use `AskUserQuestion` with the question data from the JSON
3. Based on user's selection:
   - **"Yes, approve access"** → Use `bash cat "filepath"` to read the file
   - **"No, skip this file"** → Continue without accessing the file

## Python Scripts (Skills)

When running Python scripts from `.claude/skills/`, use the venv Python interpreter:
- **Linux/macOS:** `.claude/skills/.venv/bin/python3 scripts/xxx.py`
- **Windows:** `.claude\skills\.venv\Scripts\python.exe scripts\xxx.py`

## Documentation

Docs live in `./docs/` — see `docs/system-architecture.md` for detailed architecture, `docs/code-standards.md` for C++ conventions and pet game balance constants, `docs/activity-manager.md` for the activity lifecycle migration guide.
