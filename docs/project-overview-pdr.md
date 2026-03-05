# Project Overview & Product Development Requirements (PDR)

**Last Updated:** March 1, 2026
**Version:** 1.2.0
**Status:** Active Development

## Executive Summary

**CrossPoint Reader** is an open-source firmware replacement for the Xteink X4 e-paper device (ESP32-C3 based). It provides a full-featured EPUB reader with integrated tools and interactive features, built on constrained embedded hardware (380KB RAM).

**Key Differentiator:** Purpose-built for e-paper constraints with aggressive SD caching, partial display updates, and single-threaded event loop design.

## Project Scope

### In Scope

**Core Reading:**
- EPUB 2 & 3 parsing (via libxml2)
- Full-text rendering with typography (font selection, margins, line height)
- Image support (JPEG/PNG via JPEGDEC)
- Reading progress persistence (bookmark at chapter/page)
- KOReader Sync integration for cross-device progress

**User Interface:**
- Activity-based UI (stack-based navigation)
- File browser with nested folder support
- Customizable settings (font, layout, display, theme)
- Responsive input handling (buttons, D-pad)

**Tools & Features (v1.4.0+):**
- Clock with sleep screen mode and daily quotes
- Games (2048, Sudoku, Minesweeper, Chess, Caro)
- Virtual Pet (Tamagotchi-style with reading-driven evolution)
- Pomodoro timer with pet integration
- Weather display and news reader
- Daily inspirational quote activity

**Connectivity:**
- WiFi OTA updates
- WebDAV file management
- Web-based settings interface
- Bluetooth remote control (HID)

**Internationalization:**
- 18 languages (English, Spanish, French, German, Italian, Portuguese, Russian, Ukrainian, Polish, Swedish, Norwegian, Chinese, Japanese, Korean, Vietnamese, Turkish, Hebrew, Arabic)
- Dynamic language switching without restart

### Out of Scope (Future Work)

- [ ] User-provided fonts (custom .ttf support)
- [ ] Full UTF-8 support (currently 2-byte sequences)
- [ ] EPUB3 fixed-layout support
- [ ] Cloud backup of pet state
- [ ] Pet networking/trading
- [ ] Persistent WiFi credential encryption
- [ ] HTTPS/OAuth2 for web services

## Functional Requirements

### FR1: EPUB Reading

**Requirement:** Device shall parse, render, and display EPUB books with high fidelity on e-paper display.

**Sub-requirements:**
- FR1.1: Parse EPUB 2 & 3 archive format (ZIP-based)
- FR1.2: Extract and render XHTML content with CSS styling (basic layout only)
- FR1.3: Support inline images (JPEG/PNG)
- FR1.4: Implement word-wrap and hyphenation per language (16 languages)
- FR1.5: Cache rendered chapters to SD card for fast re-access
- FR1.6: Persist reading position (chapter + page offset)
- FR1.7: Display table of contents and allow chapter jumping

**Test Cases:**
- Open various EPUB files (test_*.epub in test/epubs/)
- Navigate between chapters
- Verify reading position restored on re-open
- Test hyphenation across languages

### FR2: Settings & Customization

**Requirement:** User shall customize display layout, font, and language.

**Sub-requirements:**
- FR2.1: Font selection (Bookerly, Noto Sans, OpenDyslexic, Ubuntu)
- FR2.2: Font size (12pt - 18pt)
- FR2.3: Display orientation (0°, 90°, 180°, 270°)
- FR2.4: Page margins and line height adjustment
- FR2.5: Language selection from 18 options
- FR2.6: Theme selection (Lyra color scheme)
- FR2.7: Persistent settings storage (SD card JSON)

**Test Cases:**
- Change font and size, verify rendering
- Rotate display, check layout adaptation
- Switch languages, verify all UI text updates
- Power cycle and verify settings persist

### FR3: Virtual Pet

**Requirement:** Device shall provide an interactive virtual pet with gameplay progression and care mechanics.

**Sub-requirements:**
- FR3.1: Pet lifecycle (EGG → HATCHLING → YOUNGSTER → COMPANION → ELDER → DEAD)
- FR3.2: Core stats: hunger, happiness, health (0-100 scale)
- FR3.3: Hourly stat decay with sleep cycle skip (10 PM - 7 AM)
- FR3.4: Weight system (overweight >80, underweight <20, normal=50)
- FR3.5: Sickness mechanic (from overfeeding or neglect, cured with medicine)
- FR3.6: Waste/bathroom system (generates every 3 meals, requires cleaning)
- FR3.7: Discipline system (rewards ignoring fake calls, punishes indulgence)
- FR3.8: Attention calls every ~4 hours (30% fake for discipline testing, 2h expiry)
- FR3.9: Evolution variants (Scholar/Balanced/Wild based on reading metrics & care quality)
- FR3.10: Reading-driven progression (streaks unlock meal cost reductions, book completions reward bonuses)
- FR3.11: Daily missions (read 20 pages, pet 3x for bonuses)
- FR3.12: Care mistake tracking (hungry >6h, sick >12h, dirty >4h)
- FR3.13: 8 user actions (feed meal, feed snack, medicine, exercise, clean, scold, ignore cry, toggle lights)
- FR3.14: Mood-based sprite rendering (8 moods)
- FR3.15: Persistent state (35+ fields to JSON with reading metrics)

**Evolution Thresholds:**
| Stage | Days | Pages | Avg Hunger |
|-------|------|-------|------------|
| EGG → HATCHLING | 1 | 20 | 0 |
| HATCHLING → YOUNGSTER | 3 | 100 | 40 |
| YOUNGSTER → COMPANION | 7 | 500 | 50 |
| COMPANION → ELDER | 14 | 1500 | 60 |

**Test Cases:**
- Create pet and verify initial state
- Feed pet and check hunger decreases
- Wait >6 hours without feeding, verify care mistake
- Exercise pet and check weight decrease
- Verify evolution triggers at correct thresholds
- Test fake call detection and discipline mechanic
- Power cycle and verify state loads correctly

### FR4: Reading Stats Sleep Screen

**Requirement:** Device shall display reading statistics on sleep screen showing daily progress and lifetime metrics.

**Sub-requirements:**
- FR4.1: Sleep screen mode READING_STATS=7 displays today's reading time in minutes
- FR4.2: Display all-time total reading time across all books
- FR4.3: Show last-read book title and completion progress bar
- FR4.4: Auto-update on wake from brief power button press
- FR4.5: Binary persistence to `/.crosspoint/reading_stats.bin`
- FR4.6: Auto-load stats on device boot

**Test Cases:**
- Open EPUB and track reading time (5 minutes, verify display)
- Power cycle device and verify reading stats restored
- Brief wake (power button) and verify reading stats screen updates

### FR5: WiFi & OTA

**Requirement:** Device shall support wireless connectivity for updates and file management.

**Sub-requirements:**
- FR4.1: WiFi connection (SSID/password entry)
- FR4.2: OTA firmware update via HTTP
- FR4.3: WebDAV file upload/download
- FR4.4: Web-based settings interface
- FR4.5: KOReader Sync API integration

**Test Cases:**
- Connect to WiFi, verify stability
- Upload EPUB via WebDAV, verify on device
- Download firmware via OTA, verify integrity
- Change settings via web interface

### FR6: Input & Navigation

**Requirement:** Device shall respond to button input with clear navigation feedback.

**Sub-requirements:**
- FR5.1: Up/Down navigation (menu scrolling)
- FR5.2: Confirm selection (enter/select action)
- FR5.3: Back navigation (exit activity, return to previous)
- FR5.4: Customizable button mapping
- FR5.5: D-pad support for navigation

**Test Cases:**
- Navigate settings menu, verify highlighting
- Scroll long lists, verify clipping
- Test back button returns to previous activity
- Remap buttons and verify new mappings work

## Non-Functional Requirements

### NFR1: Performance

**Requirement:** Device shall maintain responsive UI with <500ms full refresh and <100ms partial updates.

**Specification:**
- Display refresh rate: 60 FPS target for smooth scrolling
- Activity transition: <100ms (no visible stutter)
- Pet action execution: <50ms (immediate feedback)
- EPUB pagination: <200ms between chapters
- WiFi connection: <5 seconds to establish

**Measurement:**
- Profile with serial logging
- Time critical sections with `esp_timer_get_time()`

### NFR2: Memory Efficiency

**Requirement:** Device shall operate within ESP32-C3 constraints (380KB usable RAM).

**Specification:**
- Peak heap usage: <300KB (leaving 80KB buffer for OS)
- Stack usage: <10KB per function
- PROGMEM strings: All UI text in flash
- Render buffer: ~40KB (partial updates only)

**Strategy:**
- Aggressive SD caching (EPUB chapters, pet sprites)
- Single-threaded event loop (no concurrent I/O)
- String pooling via i18n system

**Measurement:**
- Use `heap_caps_get_free_size()` API
- Test with largest EPUB files

### NFR3: Storage Efficiency

**Requirement:** Device shall manage SD card storage efficiently.

**Specification:**
- Cache directory: `.crosspoint/` at SD root
- EPUB cache: Hash-based directory per book
- Pet state: Single JSON file (200 bytes)
- Settings: Single JSON file (500 bytes)
- Automatic cleanup: Remove cache when book deleted (future)

**Measurement:**
- Monitor `.crosspoint/` size after loading 10 EPUBs

### NFR4: Reliability

**Requirement:** Device shall maintain data integrity across power cycles.

**Specification:**
- Reading position: Saved every 30 seconds
- Pet state: Saved after every action
- Settings: Saved on change
- Graceful degradation: Run without SD card (limited functionality)

**Recovery:**
- Corrupted settings.json → Reset to defaults
- Corrupted pet/state.json → Start new pet
- Missing EPUB cache → Rebuild on next read

### NFR5: Internationalization

**Requirement:** Device shall support 18 languages with complete UI coverage.

**Specification:**
- 419 total string keys
- All UI text translated (no English fallbacks in main flow)
- Language switching via settings (no restart required)
- 37 pet-specific keys for Tamagotchi content

**Coverage:**
- Activity titles, menu labels, buttons
- Error messages, confirmations
- Pet actions, moods, needs descriptions
- Settings panel text

## Architectural Constraints

### AC1: Embedded Hardware Limitations

**Constraint:** ESP32-C3 (240 MHz single-core, 380KB RAM, ~4MB flash)

**Implications:**
- Single-threaded design (no mutexes needed)
- Aggressive SD caching to minimize RAM
- Render-on-demand (no off-screen framebuffer)
- Limited font rendering (pre-baked glyphs)

### AC2: E-Paper Display Characteristics

**Constraint:** 540x960 grayscale e-ink display with ~200ms refresh time

**Implications:**
- Partial updates for speed (local content changes)
- Full refresh every 20 pages (reduce ghosting)
- 16-level grayscale (no color)
- Limited animation (single frame per second practical)

### AC3: Activity-Based UI Architecture

**Constraint:** Activities form a stack with mutual exclusion on input

**Implications:**
- No concurrent activity processing
- Clear state transitions (onEnter/onExit)
- Simple event routing (no event bubbling)

### AC4: SD Card Dependency

**Constraint:** All persistent data and caching via SD card

**Implications:**
- Device requires working SD card for full features
- Latency: ~100ms per file I/O
- Must handle missing/corrupted files gracefully

## Design Decisions

### DD1: Why Cache Everything to SD Card?

**Decision:** Use aggressive SD card caching for EPUB chapters, pet sprites, and computed data.

**Rationale:**
- ESP32-C3 only has 380KB RAM (too small for chapter buffers)
- SD card is 4GB+ (essentially unlimited for cache)
- Reduced boot time on subsequent reads
- Preserves RAM for active processing

**Trade-off:** Latency (100ms) vs. Memory (save 50KB per chapter)

### DD2: Why Single-Threaded Event Loop?

**Decision:** All processing in single task, no multi-threading.

**Rationale:**
- Eliminates race conditions (no mutexes needed)
- Simpler state management
- Lower memory overhead (no thread stacks)
- Easier to reason about timing

**Trade-off:** Responsiveness (must finish task in <16ms) vs. Simplicity

### DD3: Why JSON for Persistence?

**Decision:** Human-readable JSON format for settings and pet state.

**Rationale:**
- Easy debugging (readable on SD card)
- No schema migration needed (add fields freely)
- Standard library support (ArduinoJSON)
- Supports nested structures (future expansion)

**Trade-off:** File size (1KB) vs. Simplicity

### DD4: Why 30% Fake Attention Calls?

**Decision:** Pet generates false attention calls to test player discipline.

**Rationale:**
- Creates gameplay tension (real vs. fake?)
- Rewards correct decision-making (discipline stat increase)
- Prevents exploitation (always feed on call)
- Mirrors real Tamagotchi mechanics

**Tuning:** Set `FAKE_CALL_CHANCE_PERCENT` in PetConfig

## Success Metrics

### SM1: User Experience

- **Reading Flow:** 95% of EPUB books load and render without errors
- **Navigation:** <100ms activity transitions (no visible stutter)
- **Settings:** All UI text translated to all 18 languages
- **Pet Gameplay:** Players reach ELDER stage within 2-3 weeks

### SM2: Stability

- **Crash Rate:** <1 crash per 100 hours of use
- **Data Loss:** Zero reading position or pet state loss on power cycle
- **Cache Corruption:** Automatic recovery without user intervention

### SM3: Performance

- **Display Refresh:** <500ms full refresh, <100ms partial update
- **Memory:** Peak heap <300KB (safety margin >80KB)
- **Battery:** >1 week on full charge (depends on display refresh rate)

### SM4: Code Quality

- **Coverage:** All critical paths tested (pet evolution, EPUB parsing, persistence)
- **Warnings:** Zero compilation warnings
- **Code Style:** 100% clang-format compliant

## Timeline & Milestones

### v1.0 (Released)
- Core EPUB reader
- File browser
- Settings customization
- WiFi OTA

### v1.1 (Released)
- Clock activity
- Pomodoro timer
- Games (Snake, 2048)
- Photo frame

### v1.2 (Released - Mar 1, 2026)
- Virtual Pet with full gameplay
- Evolution system (3 variants: Good/Chubby/Misbehaved)
- Care mechanics (weight, sickness, waste, discipline)
- Attention calls & daily missions
- Enhanced sleep screen with pet indicator

### v1.3 (Released - Mar 2, 2026)
- Reading-driven pet evolution (Scholar/Balanced/Wild variants)
- Reading streaks with meal cost reductions
- Book completion bonuses and progression gates
- Daily reading goal bonuses
- Pomodoro focus rewards
- Full Vietnamese i18n

### v1.4 (In Development)
- New games (Chess, Caro/Gomoku)
- New tools (Weather, News Reader)
- Daily quotes on sleep screen
- Reading stats sleep screen mode
- Home screen clock header display
- ToolsActivity expansion to 11 items

### v2.0 (Future)
- Pet networking (trade with other devices)
- Cloud backup option
- Enhanced graphics (more variant sprites)
- User-provided fonts
- Full UTF-8 support
- EPUB3 fixed-layout support
- Plugins/extensions system

## Risk Assessment

### Risk 1: Memory Exhaustion

**Probability:** Medium | **Impact:** High

**Description:** Large EPUB with many images exceeds 380KB RAM limit.

**Mitigation:**
- Strict image size limits in renderer
- Progressive image loading
- Stress test with largest EPUBs
- Document RAM limits in user guide

### Risk 2: SD Card Corruption

**Probability:** Low | **Impact:** Medium

**Description:** Power loss during write corrupts pet state or settings.

**Mitigation:**
- Use temporary file + atomic rename for writes
- Validate JSON on load (reject corrupted files)
- Automatic fallback to defaults
- Document recovery procedure

### Risk 3: WiFi Credential Exposure

**Probability:** Medium | **Impact:** Medium

**Description:** WiFi passwords stored in plaintext JSON.

**Mitigation:**
- Document security limitation in release notes
- Plan encryption for v1.3
- Recommend unique SSID for device-only network

### Risk 4: Pet Evolution Imbalance

**Probability:** Medium | **Impact:** Low

**Description:** Evolution requirements too easy/hard, gameplay unbalanced.

**Mitigation:**
- Telemetry: Track average time to evolution
- Community feedback loop
- Game balance constants in PetConfig (easy tuning)
- Test with target player profiles

## Dependencies

### External Libraries

| Library | Purpose | License |
|---------|---------|---------|
| esp-idf | ESP32 SDK | Apache 2.0 |
| ArduinoJSON | JSON parsing | MIT |
| libxml2 | XML/EPUB parsing | MIT |
| JPEGDEC | JPEG decoding | Apache 2.0 |
| ArduinoBLE | Bluetooth support | LGPL |

### Tools

| Tool | Purpose |
|------|---------|
| PlatformIO | Build system |
| Clang | C++ compiler |
| Python 3.8+ | i18n code generation |

## Development Workflow

### Code Review Process

1. Create feature branch: `feature/my-feature`
2. Implement with tests
3. Push to GitHub
4. Create pull request
5. Code review (maintainers approve)
6. Run CI/CD (compilation, tests)
7. Merge to master
8. CI/CD builds final firmware.bin
9. Release tagged with version

### Build & Release

```bash
# Development build
pio run && pio run --target upload

# Release build
pio run -e release   # Optimized firmware
# Upload to GitHub Releases
```

## Document Control

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | Jun 2025 | Initial EPUB reader |
| 1.1 | Nov 2025 | Tools menu (clock, games) |
| 1.2 | Mar 2026 | Virtual pet full feature |

**Approval:** Project Maintainers

---

**Prepared By:** CrossPoint Development Team
**Last Modified:** March 1, 2026
**Next Review:** June 1, 2026
