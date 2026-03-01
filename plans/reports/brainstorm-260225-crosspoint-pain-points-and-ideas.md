# CrossPoint Reader: Pain Points & Ideas Brainstorm

**Date:** 2026-02-25
**Scope:** ESP32-C3 e-ink reader firmware analysis
**Source:** Codebase analysis + 30 open GitHub issues + 20 recent closed bugs + TODO/FIXME audit

---

## Part 1: Current Pain Points

### P1. Stability & Crashes (Critical)

| Issue | Source | Impact |
|-------|--------|--------|
| SPI mutex released from wrong task | #1137 | Device crash |
| Certain EPUBs crash device | #1066 | Data loss risk, bad UX |
| Memory errors on boot with XTC books | #814 | Can't read certain books |
| Regression from new framework | #1092 | Multiple breakages |
| Potential infinite loop on page load fail | `EpubReaderActivity.cpp:620` | Device hangs |
| `requestUpdateAndWait()` uses `delay(100)` instead of FreeRTOS notification | `Activity.cpp:51` | Race conditions, wasted cycles |

**Root cause pattern:** Single-core ESP32-C3 with ~380KB RAM makes concurrency bugs (SPI mutex, render task sync) particularly dangerous. No proper error recovery framework exists - failures cascade.

### P2. WiFi & File Transfer (High)

| Issue | Source | Impact |
|-------|--------|--------|
| Uploads extremely slow, often fail | #1160 | Primary file transfer broken |
| WiFi password with spaces won't connect | #899 | Can't join certain networks |
| OTA update check fails | #943 | Can't update firmware |
| OPDS non-latin filenames corrupted | #1118 | Broken for non-English users |
| OPDS missing pagination | #1048 | Large catalogs unusable |
| OPDS Komga fails | #901 | Major OPDS provider broken |

**Root cause pattern:** Network code has `WiFi.setSleep(false)` for responsiveness but delay-based polling loops. Upload reliability likely suffers from ESP32-C3's single-core blocking the WiFi stack during SD writes.

### P3. E-ink Rendering Quality (High)

| Issue | Source | Impact |
|-------|--------|--------|
| Significant ghost text after page turns | #1106 | Degrades readability |
| Disabling AA makes inline images worse | #1113 | Lose-lose choice for users |
| PNG artifacts with embedded styles | #1025 | Broken images |
| CSS centering broken with justify | #1029 | Layout bugs |
| Justification inconsistency | #1161 | Visual quality |
| Bullet point rendering broken | #956 | Common EPUB pattern |
| Problematic images in v1.1.0 | #1088 | Regression |

**Root cause pattern:** The double-refresh technique for AA image pages is clever but fragile. CSS parser has ~10 TODO comments about missing selectors (sibling, nested, attribute, pseudo, ID, wildcard, class combinations). Ghosting management is coarse-grained (every N pages).

### P4. Reading Experience Bugs (Medium)

| Issue | Source | Impact |
|-------|--------|--------|
| Reading position lost after sleep | #905 | Frustrating for daily use |
| Incorrect chapter length calculation | #1131 | Wrong progress display |
| Long-press back goes to home, not book's dir | #1117 | Navigation confusion |
| Two short presses wakes from sleep | #1114 | Accidental wakeups |
| "End of book" screen lacks options | #1034 | Dead-end UX |
| Progress bar adds empty space | #1024 | Wasted screen real estate |

**Root cause pattern:** Progress tracking uses path-based cache keying - moving a book resets progress. Binary progress format stores only chapter+page (6 bytes) with no integrity checks.

### P5. Architecture & Code Health (Medium)

| Area | Issue | Location |
|------|-------|----------|
| Blocking delays | 40+ `delay()` calls throughout, many could be event-driven | Entire codebase |
| Slow manifest lookup | Linear O(n) scan for each item in spine | `ContentOpfParser.cpp:277` |
| Auto-indexing large sections | No progress feedback or chunking for large chapters | #1067 |
| Font system rigidity | Fonts compiled into firmware, 4 sizes x 4 weights = 48+ font objects in `main.cpp` | `main.cpp:44-118` |
| Cache invalidation | Cache not cleared on book deletion; move breaks progress | README known limitation |
| CSS parser gaps | 10+ missing selector types (sibling, attribute, pseudo, etc.) | `CssParser.cpp:368-617` |
| Credential storage | XOR obfuscation + base64 for passwords (not secure) | `WifiCredentialStore.h`, `JsonSettingsIO.cpp` |

### P6. Feature Gaps (from issues + README checklist)

- No bookmarks or annotations
- No cover art in file picker (README checkbox unchecked)
- No user-provided fonts (README checkbox unchecked)
- Incomplete UTF support (README checkbox unchecked)
- No dictionary lookup (in-scope per SCOPE.md but not implemented)
- No dark/inverted reading mode
- No Thai script support (#1126)
- No Danish translation (#1102)
- Limited accessibility (#1064) - font scaling, boldness toggle, inverted mode, enlarged UI

---

## Part 2: Ideas

### Tier 1: Quick Wins (1-3 days each, high impact)

#### 1.1 Infinite Loop Guard for Page Loading
**Problem:** `EpubReaderActivity.cpp:620` can loop forever on corrupted cache.
**Solution:** Add retry counter (max 2 attempts). After 2 failures, show error message and skip to next chapter.
**Impact:** Prevents device hangs. Zero risk.

#### 1.2 WiFi Password Space/Special Char Fix
**Problem:** #899 - passwords with spaces can't connect.
**Solution:** Likely URL-encoding or null-termination issue in `WifiCredentialStore`. Audit string handling.
**Impact:** Unblocks users on many networks.

#### 1.3 Smart Ghosting Management
**Problem:** #1106 - ghost text accumulates between full refreshes.
**Solution:** Instead of fixed N-page refresh cadence, detect ghosting buildup heuristically:
- Force HALF_REFRESH when chapter changes (new layout = more ghosts)
- Force HALF_REFRESH after images
- Reduce default refresh frequency from user setting to be more aggressive (e.g., every 3 pages default)
**Impact:** Single biggest readability improvement possible.

#### 1.4 End-of-Book Screen Improvements
**Problem:** #1034 - "End of Book" is a dead-end.
**Solution:** Add options: "Go to Library", "Read Again", show book title/author, reading stats (time, pages).
**Impact:** Natural completion flow instead of confusion.

#### 1.5 Back Button Navigation Fix
**Problem:** #1117 - long-press back should go to book's parent folder, not root.
**Solution:** Store parent folder path in `APP_STATE` when opening a book. Use it for long-press back target.
**Impact:** Much better navigation for organized libraries.

---

### Tier 2: Medium Effort (1-2 weeks each, strong ROI)

#### 2.1 Bookmarks System
**Problem:** No way to mark pages. This is the #1 missing feature for a reader.
**Solution:**
- Store bookmarks in `progress.bin` (extend format) or separate `bookmarks.bin`
- Max 50 bookmarks per book (keeps memory bounded)
- Each bookmark: spine_index (2B) + page (2B) + timestamp (4B) = 8 bytes
- UI: Long-press Confirm to add bookmark; menu option to view/jump bookmarks
- Visual indicator: small triangle in corner of bookmarked pages
**Constraint:** 50 bookmarks x 8 bytes = 400 bytes per book. Trivial on SD.
**Impact:** Essential reading feature. Aligns with SCOPE.md "User Experience" goals.

#### 2.2 Cover Art in File Picker
**Problem:** README checkbox unchecked. Browsing books without covers is like browsing a spreadsheet.
**Solution:**
- Cover BMPs already cached at `.crosspoint/epub_<hash>/cover.bmp`
- For uncached books, generate covers lazily (show placeholder, then replace)
- Grid view: 2x3 or 3x4 covers depending on orientation
- List view: small thumbnail + title + author
**Constraint:** Loading 6-12 cover BMPs needs ~100KB RAM. Use partial loading + downsample.
**Impact:** Transforms the browsing experience. Highly visible improvement.

#### 2.3 Upload Reliability Overhaul
**Problem:** #1160 - uploads slow and fail.
**Solution:**
- Replace synchronous SD write during upload with buffered write (write in chunks)
- Add upload progress feedback via WebSocket
- Implement resume support for interrupted uploads (check file size, append)
- Disable WiFi power saving during active transfer (already done, verify)
- Consider increasing SPI clock for SD during transfers
**Impact:** File transfer is the primary way to get books on the device. Broken = unusable.

#### 2.4 Manifest Lookup Speedup
**Problem:** `ContentOpfParser.cpp:277` - O(n) linear scan per spine item. Slow for books with 1500+ items.
**Solution:** Build a `std::unordered_map<std::string, int>` from href to index during initial parse. O(1) lookup.
**Constraint:** Map overhead ~24 bytes per entry. 1500 entries = ~36KB. Acceptable if built during parse, freed after.
**Impact:** Faster chapter loads. Directly improves user-perceived responsiveness.

#### 2.5 Reading Position Robustness
**Problem:** #905 - position lost after sleep. Also, moving a book resets progress.
**Solution:**
- Add a content hash (CRC32 of first 1KB) to the progress file for integrity verification
- On position restore failure, fall back to spine start rather than crashing
- For moved books: consider content-hash-based cache lookup (hash the EPUB content, not path)
**Tradeoff:** Content hashing on open adds ~50ms. Worth it for reliability.
**Impact:** Prevents the most frustrating daily-use bug.

#### 2.6 Replace `delay()` Polling with FreeRTOS Notifications
**Problem:** `Activity.cpp:51` FIXME - `requestUpdateAndWait()` uses busy-wait.
**Solution:**
- Use `ulTaskNotifyTake()` / `xTaskNotifyGive()` between render task and main loop
- Already partially implemented in main.cpp render task - extend to Activity base class
- Eliminates wasted CPU cycles and potential race conditions
**Impact:** Better power efficiency, eliminates a class of timing bugs.

---

### Tier 3: Ambitious but Valuable (2-4 weeks, high differentiation)

#### 3.1 User-Loadable Fonts from SD Card
**Problem:** Fonts are compiled into firmware (48+ font objects in `main.cpp`). Users can't add their own.
**Solution:**
- Support TTF/OTF files in a `.crosspoint/fonts/` directory on SD
- At boot, scan for font files and generate bitmap font data (or use a lightweight rasterizer)
- Alternative: pre-process fonts to the compressed format used by `EpdFont` via a companion tool
**Constraint:** Runtime TTF rasterization on ESP32-C3 is likely too slow. Better approach: provide a PC/web tool that converts TTF to CrossPoint's compressed font format, then load from SD.
**Impact:** Unlocks personalization. Makes the reader truly customizable.

#### 3.2 Offline Dictionary Lookup
**Problem:** In-scope per SCOPE.md but unimplemented. Dictionary is a core reading feature.
**Solution:**
- StarDict format is compact and well-documented
- Store dictionary files on SD in `.crosspoint/dict/`
- Word selection: highlight word on current page using Left/Right buttons
- Popup overlay showing definition
**Constraint:** Dictionary files can be 10-50MB. SD card has ample space. RAM concern: stream dictionary entries, don't load entire file.
**Impact:** Transforms the reader from "displays books" to "helps you read." Major differentiator.

#### 3.3 Adaptive E-ink Refresh Engine
**Problem:** Current approach uses fixed-cadence full refreshes. Doesn't adapt to content complexity.
**Solution:**
- Track pixel change density between frames
- High-change pages (new chapter, images) get HALF_REFRESH automatically
- Low-change pages (same layout, just text) can use more FAST_REFRESH
- User setting becomes "quality preference" (crisp vs. fast) rather than raw page count
**Impact:** Best possible reading experience for the hardware. Addresses #1106 systematically.

#### 3.4 Streaming Chapter Parser for Large Sections
**Problem:** #1067 - extremely large sections cause long indexing times with no feedback.
**Solution:**
- Parse chapters in streaming fashion with progress callbacks
- Show progress bar during initial indexing: "Indexing chapter... 45%"
- Cache intermediate parse state so a crash doesn't lose all work
- Consider splitting mega-chapters into virtual sub-sections for memory
**Impact:** Makes the device usable for books with enormous chapters (academic texts, etc.).

#### 3.5 CSS Parser Improvements
**Problem:** 10+ missing CSS selector types limit EPUB rendering fidelity.
**Solution:** Prioritize by frequency in real EPUBs:
1. Class combinations (`.foo.bar`) - most common gap
2. Descendant selectors (`div p`) - critical for nested styling
3. ID selectors (`#id`) - used in some EPUBs
4. Attribute selectors (`[attr]`) - rare but needed for complex EPUBs
**Approach:** Incremental - each selector type is independent work.
**Impact:** Better rendering of complex EPUBs. Reduces "why doesn't my book look right?" reports.

---

### Tier 4: Out-of-Scope but Worth Noting

These don't fit the project's mission but are frequently requested:

| Request | Why Out-of-Scope | Alternative |
|---------|-------------------|-------------|
| PDF support | Reflow on 480px is terrible; different rendering engine needed | Convert to EPUB externally |
| Audiobook support | Explicitly out-of-scope per SCOPE.md | Separate device |
| Note-taking | Out-of-scope per SCOPE.md; button-only input is painful | KOReader sync for annotations |
| Web browser | Out-of-scope; WiFi drain, single-core | OPDS catalog covers this use case |

---

## Part 3: Prioritized Roadmap Suggestion

### Phase 1: Stability (Weeks 1-2)
- Fix SPI mutex crash (#1137)
- Add page loading infinite loop guard (1.1)
- Fix reading position loss (#905 / 2.5)
- Fix WiFi password spaces (#899 / 1.2)

### Phase 2: Core Experience (Weeks 3-6)
- Smart ghosting management (1.3)
- Bookmarks system (2.1)
- Upload reliability overhaul (2.3)
- End-of-book screen (1.4)
- Back button navigation (1.5)

### Phase 3: Polish (Weeks 7-10)
- Cover art in file picker (2.2)
- Manifest lookup speedup (2.4)
- FreeRTOS notification cleanup (2.6)
- CSS class combinations support

### Phase 4: Differentiation (Weeks 11+)
- Offline dictionary (3.2)
- User-loadable fonts (3.1)
- Adaptive refresh engine (3.3)
- Streaming chapter parser (3.4)

---

## Unresolved Questions

1. **SPI mutex crash (#1137):** Is this reproducible? Which task is incorrectly releasing? Need serial logs.
2. **Upload performance (#1160):** Is the bottleneck WiFi throughput, SD write speed, or single-core contention? Need profiling data.
3. **Font loading feasibility:** Has anyone benchmarked TTF rasterization on ESP32-C3? Or is a pre-processing tool the only viable path?
4. **Dictionary RAM budget:** What's the typical free heap during reading? Need to know if dictionary popup is feasible without OOM.
5. **OPDS pagination (#1048):** Does the OPDS spec define pagination, or is this server-specific (Calibre, Komga)?
