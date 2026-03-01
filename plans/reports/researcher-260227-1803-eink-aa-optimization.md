# E-Ink Anti-Aliasing Optimization Research

**Date:** 2026-02-27
**Status:** Complete
**Target Device:** ESP32-C3 + Xteink X4 (800x480 e-ink, 48KB single framebuffer)

---

## Executive Summary

Text anti-aliasing on the Xteink X4 is implemented via **2-bit grayscale font glyphs + dual-pass refresh** using the SSD1677 controller's grayscale waveform. The current bottleneck is **display refresh time**, not rendering. Performance gains require hardware-level optimizations; software improvements are limited by the e-ink refresh physics.

**Key finding:** Current implementation already uses near-optimal strategies. Further speed gains demand accepting trade-offs in image quality or exploiting partial-refresh mode (which degrades text quality over time).

---

## Current Implementation Analysis

### 1. **Grayscale Text Rendering Architecture**

**Location:** `lib/GfxRenderer/GfxRenderer.cpp` (lines 97-130 for 2-bit glyph rendering)

Fonts come in two formats:
- **1-bit fonts:** Standard black/white bitmaps (fastest)
- **2-bit fonts:** 4-level grayscale (00=white, 01=light gray, 10=dark gray, 11=black)

When `textAntiAliasing` is enabled:
1. Initial B&W render with black text (single-pass FAST_REFRESH ~200ms)
2. **LSB pass:** Text re-rendered in GRAYSCALE_LSB mode, buffer saved to display
3. **MSB pass:** Text re-rendered in GRAYSCALE_MSB mode, buffer saved to display
4. **Grayscale refresh:** Custom LUT applied (lut_grayscale), displays combined LSB+MSB as 4-level gray

**Rendering mode logic** (lines 118-128):
```cpp
// BW mode: Only black pixels painted
if (renderMode == BW && bmpVal < 3) { renderer.drawPixel(..., true); }

// GRAYSCALE_MSB: Light/dark gray pixels marked
else if (renderMode == GRAYSCALE_MSB && (bmpVal == 1 || bmpVal == 2)) { renderer.drawPixel(..., false); }

// GRAYSCALE_LSB: Only dark gray marked
else if (renderMode == GRAYSCALE_LSB && bmpVal == 1) { renderer.drawPixel(..., false); }
```

**Result:** 2-bit glyph values (0-3) map to 4 visual levels via LSB/MSB bit planes.

### 2. **Display Refresh Implementation**

**Location:** `open-x4-sdk/libs/display/EInkDisplay/src/EInkDisplay.cpp`

Three refresh modes available:
| Mode | Duration | Quality | Use Case |
|------|----------|---------|----------|
| **FAST_REFRESH** | ~200-250ms | Low (ghosting/fading) | Rapid updates, partial diffs |
| **HALF_REFRESH** | ~1720ms | Medium | Balanced full-screen |
| **FULL_REFRESH** | ~3000ms+ | High (pristine) | Infrequent updates |

**Current strategy** (EpubReaderActivity.cpp:604-663):
- **Page load:** B&W via FAST_REFRESH (or HALF_REFRESH per cadence)
- **Anti-aliasing:** Two additional refreshes:
  - LSB buffer write + display (implicit FAST_REFRESH in `displayGrayBuffer`)
  - MSB buffer write + display (implicit FAST_REFRESH in `displayGrayBuffer`)
  - Custom grayscale LUT activated during these refreshes

**Total page turn time with AA:** ~200ms (B&W) + 2×(buffer write + custom LUT refresh ~300-400ms each) = **~800-1000ms total**

### 3. **Memory Constraints**

- **Total framebuffer:** 48KB (800×480 ÷ 8 bits per pixel = 48,000 bytes)
- **No PSRAM available** (ESP32-C3 limitation)
- **Dual-buffer solution:** Chunked allocation (8KB chunks × 6 chunks) to avoid 48KB contiguous requirement
- **Grayscale technique:** Uses same 48KB buffer with LSB/MSB interpretations

**Code reference:** `lib/GfxRenderer/GfxRenderer.cpp` lines 1109-1134 (storeBwBuffer uses 6×8KB chunks)

### 4. **Dithering for Grayscale**

**Location:** `lib/Epub/Epub/converters/DitherUtils.h`

Current dithering **only used for image conversion**, not text:
- **4×4 Bayer matrix** (ordered dithering) for image downsampling
- **Algorithm:** `applyBayerDither4Level()` — stateless, spatially-dependent
- **Not applied to text:** Fonts already have 2-bit anti-aliased glyphs from generation

**Key insight:** Text uses pre-rendered 2-bit glyphs (better quality), while images use Bayer dithering (faster, lossy).

---

## Performance Bottlenecks & Optimization Analysis

### Critical Path Analysis

```
Page Turn Sequence:
├─ Render B&W text           [~50-100ms CPU]
├─ Clear framebuffer         [~5ms]
├─ Write BW RAM              [~20ms SPI @ 40MHz]
├─ Display refresh (B&W LUT) [~200ms e-ink waveform]
│
├─ (Grayscale AA if enabled)
│  ├─ Render LSB pass        [~50-100ms CPU]  ← Redundant glyph decoding/rendering
│  ├─ Write LSB buffer       [~20ms SPI]
│  ├─ Refresh (custom LUT)   [~250-350ms e-ink]
│  │
│  ├─ Render MSB pass        [~50-100ms CPU]  ← Redundant glyph decoding/rendering
│  ├─ Write MSB buffer       [~20ms SPI]
│  ├─ Refresh (custom LUT)   [~250-350ms e-ink] ← LARGEST SINGLE BOTTLENECK
│  │
│  └─ Grayscale finish       [~20ms cleanup]
│
└─ Total: 400-600ms (B&W) + 600-800ms (AA) = 1000-1400ms per page
```

### 1. **Refresh Time Bottleneck (PRIMARY)**

**Issue:** E-ink waveforms take 250-350ms regardless of software optimization.

**Hardware limitation:** SSD1677 controller firmware (not our code) controls waveform timing.
- Custom LUT can tune timing, but minimal headroom
- Cannot make physics of electrophoretic ink faster

**Feasibility:** ⚠️ **LIMITED** — Hardware-constrained

**Possible mitigations:**
- ✅ **Temperature-based LUT adjustment:** Warmer = faster, but screen flashes more (lut_grayscale uses fixed values)
- ✅ **Aggressive voltage boost:** Some e-ink controllers support higher voltages → faster refresh, but risk hardware damage
- ❌ **Skip grayscale entirely:** Saves ~600ms but loses anti-aliasing quality

**Code reference:** `EInkDisplay.cpp` lines 44-75 (lut_grayscale fixed timing) and lines 574-628 (refreshDisplay method)

### 2. **Redundant Glyph Rendering (SECONDARY)**

**Issue:** Text is rendered **three times** (B&W, LSB pass, MSB pass).

**Current code:**
```cpp
// EpubReaderActivity.cpp:610-657
page->render(...);  // Initial B&W
renderer.displayBuffer(FAST_REFRESH);

// Now grayscale
renderer.setRenderMode(GRAYSCALE_LSB);
page->render(...);  // Re-render ENTIRE page for LSB
renderer.copyGrayscaleLsbBuffers();

renderer.setRenderMode(GRAYSCALE_MSB);
page->render(...);  // Re-render ENTIRE page for MSB
renderer.copyGrayscaleMsbBuffers();
```

**Impact:** Each glyph decode/render costs ~1-2 CPU cycles × ~2000 glyphs per page = 4-8ms overhead

**Feasibility:** ⚠️ **MODERATE** — Requires architectural refactoring

**Optimization strategy:**
```
Instead of 3 full renders:
1. Render glyph bitmaps with gray metadata (single pass)
   - Store per-pixel gray level (0-3) in temporary buffer
2. Decompose to LSB/MSB in post-processing (32-bit SIMD)
3. Two display updates (much faster than re-rendering)
```

**Estimated gain:** Save 100-150ms per page (~12-15% total)

### 3. **SPI Buffer Writes (TERTIARY)**

**Issue:** Writing 48KB via SPI @ 40MHz takes ~20-25ms per buffer.

**Current:** BW + LSB + MSB = 3 writes × 20ms = 60ms

**Feasibility:** ✅ **LOW-HANGING FRUIT**

**Optimizations:**
- ✅ **Increase SPI clock:** Test 60-80MHz (if stable with current wire/hardware)
  - Estimated speedup: 1.3-2x reduction in SPI time → Save 10-15ms
- ✅ **Use DMA for SPI transfers:** Reduce CPU blocking, allow parallel prep work
  - Already used (SPI.writeBytes), but could pipeline with CPU rendering
- ✅ **Compress buffer on-the-fly:** RLE for uniform regions
  - Complex; e-ink text typically sparse, so marginal gains

**Code reference:** `EInkDisplay.cpp` lines 203-210 (sendData) and 378-388 (writeRamBuffer)

### 4. **Memory Efficiency (QUATERNARY)**

**Issue:** Chunked allocation workaround (6×8KB) is necessary but adds complexity.

**Current:** `storeBwBuffer()` allocates 48KB in 8KB chunks to avoid fragmentation

**Feasibility:** ⚠️ **LOW PRIORITY** — Already optimized

**Why not worth optimizing:**
- Chunked approach adds ~5-10ms for malloc/memcpy
- Can't use true dual-buffer (would need 96KB, exceeds RAM)
- PSRAM option: Would solve it, but hardware doesn't have PSRAM slot

---

## Specific Optimization Opportunities

### Opportunity 1: Parallel LSB/MSB Decomposition (15% gain, MEDIUM effort)

**Current workflow:**
```
Render LSB → Display → Render MSB → Display
```

**Optimized workflow:**
```
Render once with gray metadata → Decompose to LSB/MSB → Display LSB → Display MSB
```

**Implementation:**
1. Create `renderWithGrayMetadata()` that stores per-pixel gray (0-3) in a temporary 16-byte-per-pixel buffer
2. After page render, decompose in one SIMD loop:
   ```cpp
   for (int i = 0; i < 48000; i += 4) {
     uint8_t gray0 = grayBuffer[i], gray1 = grayBuffer[i+1], ...;
     lsbBuffer[i/8] = ((gray0 & 1) << 7) | ((gray1 & 1) << 6) | ...;
     msbBuffer[i/8] = ((gray0 & 2) << 6) | ((gray1 & 2) << 5) | ...;
   }
   ```
3. Skip grayscale rendering, jump directly to display LSB + MSB

**Estimated time saved:** 100-150ms (two full page renders)
**Total page turn impact:** 1000-1400ms → 900-1250ms (12-15% reduction)
**Code files to modify:**
- `lib/GfxRenderer/GfxRenderer.cpp` (add decomposition loop)
- `lib/GfxRenderer/GfxRenderer.h` (new method signature)
- `src/activities/reader/EpubReaderActivity.cpp` (call new workflow)

**Risk:** Requires testing that decomposition correctly maps gray values to LSB/MSB

---

### Opportunity 2: Aggressive Custom LUT Tuning (5-10% gain, LOW effort)

**Current:** `lut_grayscale` uses conservative timing (1720ms implied in comments)

**Optimization:**
1. Analyze SSD1677 datasheet for frame-count (TP/RP) aggressiveness
2. Modify LUT to reduce frame count from 4 → 3 or 2 (lines 57-66)
   ```cpp
   const unsigned char lut_grayscale[] PROGMEM = {
       // ...
       0x01, 0x01, 0x01, 0x01, 0x00,  // G0: Currently 4 frames
       0x01, 0x01, 0x01, 0x01, 0x00,  // Try reducing to 2
   ```
3. Test on actual hardware for fading/ghosting trade-off

**Estimated gain:** 50-100ms per page (5-10% reduction)
**Files to modify:**
- `open-x4-sdk/libs/display/EInkDisplay/src/EInkDisplay.cpp` (lines 44-75)

**Risk:** May cause visible fading artifacts; requires validation

**Testing needed:**
- Measure actual refresh times with modified LUT
- Visual inspection for ghosting on multiple pages

---

### Opportunity 3: Partial Refresh for Text-Only Pages (20-30% gain, HIGH complexity, RISKY)

**Concept:** Use windowed update instead of full-screen for pages without images.

**Current code hint:** `displayWindow()` exists (EInkDisplay.cpp:491-562) but marked experimental.

**Implementation:**
1. Track dirty regions during page render (bounding boxes for each text block)
2. Instead of full-screen refresh, use `displayWindow()` on dirty regions only
3. Constraints:
   - X and width must be byte-aligned (multiples of 8)
   - Y can be any pixel

**Estimated gain:** 200-300ms (full refresh ~800ms → partial ~500ms)
**Complication:** Requires page layout engine to track text block positions

**Code reference:** `EInkDisplay.cpp:491-562` (displayWindow implementation)

**⚠️ Critical caveat:**
- Partial refresh uses differential comparison (only changes pixels that differ from previous frame)
- Over many partial refreshes, residual ghosting accumulates
- **Trade-off:** Speed vs image quality
- **Not recommended for rapid page turns** (would degrade display quality)
- **Viable for slow scrolling** within a page

---

### Opportunity 4: Skip AA for Small/Display-Sized Fonts (10% gain, LOW effort)

**Observation:** Anti-aliasing benefits diminish at small sizes (< 8pt) and very large sizes (> 48pt).

**Strategy:**
```cpp
if (fontSize >= 8 && fontSize <= 24) {
    enableGrayscaleAA();  // Only apply AA for readable sizes
} else {
    skipGrayscaleAA();    // Use B&W only
}
```

**Estimated gain:** 20-30% for books using large fonts (but trade-off: AA disabled)
**Feasibility:** ✅ **EASY** — Single conditional check

**Code location:** `src/activities/reader/EpubReaderActivity.cpp` line 644

**Trade-off:** Loses quality benefit of AA for large text

---

### Opportunity 5: Pre-compute Grayscale at Font Load (Minor, future)

**Concept:** Instead of on-the-fly 2-bit font rendering with decomposition, pre-generate LSB/MSB versions when loading fonts.

**Why not now:**
- Fonts already in PROGMEM; adding LSB/MSB versions would double font storage
- Only valuable if font storage is abundant (not the case with 48KB constraint)

**Feasibility for future:** ⚠️ **Only if additional PSRAM added**

---

## Unresolved Questions & Gaps

1. **SPI clock headroom:** What's the maximum stable clock frequency for the SPI bus in the hardware? Testing at 60-80MHz could save 5-15ms.

2. **LUT aggressiveness limits:** SSD1677 datasheet limits on frame-count reduction without temporal artifacts?

3. **Partial refresh accumulation:** How many partial refreshes can run before ghosting becomes unacceptable? (Enables dirty-region optimization feasibility)

4. **Custom LUT temperature dependence:** Can we auto-adjust LUT based on internal temperature sensor to speed up warm environments?

5. **Font file format:** Do existing font files already contain separate LSB/MSB bit planes, or are they only 2-bit per glyph?

---

## Recommended Action Plan

### Phase 1: Quick Wins (Days 1-2)
1. ✅ **Test SPI clock increase** (10-15ms gain)
   - Modify `SPISettings` in EInkDisplay.cpp:147
   - Test at 60MHz, 80MHz
   - Measure refresh times with timing logs

2. ✅ **Aggressive LUT tuning** (5-10ms gain)
   - Experiment with frame-count reduction in lut_grayscale
   - Add new LUT variants (fast, balanced, quality)
   - User-selectable via settings

### Phase 2: Medium Effort (Days 3-5)
3. ⚠️ **Parallel decomposition** (100-150ms gain)
   - Requires refactoring rendering path
   - Tests: Verify LSB/MSB mapping correctness
   - Complexity: **MEDIUM** — Significant code changes

### Phase 3: Long-term (Future)
4. ❌ **Partial refresh** (200-300ms potential)
   - High risk of quality degradation
   - Requires dirty-region tracking
   - Only pursue if full-page performance becomes critical

---

## Hardware Limitations Summary

| Factor | Current | Limit | Why |
|--------|---------|-------|-----|
| E-ink refresh | 250-350ms | Physical | Electrophoretic particle movement time |
| Glyph rendering | 50-100ms | CPU clock (160 MHz) | ESP32-C3 single-core throughput |
| SPI bandwidth | 40 MHz | Stable freq | Wire parasitic capacitance, IC rating |
| Memory | 48 KB | Hardware | No PSRAM available |
| Font storage | Compressed | PROGMEM size | Limited flash space |

**Conclusion:** Software optimizations max out at **10-15% speedup** without accepting image quality trade-offs. Hardware upgrades (PSRAM, faster SPI, higher CPU clock) would unlock 30-50% gains but are infeasible on this device.

---

## Conclusion

The current anti-aliasing implementation uses sophisticated 2-bit glyph rendering + dual-buffer grayscale display. It's **well-optimized** for the hardware constraints.

**Realistic performance ceiling:**
- Current: 1000-1400ms per page with AA enabled
- With all quick wins (SPI + LUT): 850-1200ms (**~15% improvement**)
- With decomposition refactor: 750-1100ms (**~20-25% improvement**)

**Recommendation:** Prioritize **Opportunity 1 (Decomposition)** and **Opportunity 2 (LUT tuning)** for measurable gains. Avoid partial refresh without extensive user testing for ghosting acceptance.
