# TallBoy

A Pebble watchface built around oversized vector-drawn digits that scale dynamically — stretching to fill the screen and squishing as an animation mechanic on every minute change.

**Design:** Sterling Ely  
**Implementation:** Sterling Ely + Claude  
**Primary device:** Pebble Time 2 (emery, 200×228, color)

---

## Structure

```
src/main.c          — entire watchface (digits, layout, animation, health, info lines)
src/pkjs/index.js   — PebbleKit JS: weather fetch, UV index, solar times, config page
appinfo.json        — app metadata, message keys
```

Everything lives in `src/main.c`. Digits are drawn entirely from geometric primitives (`fill_arc` + `graphics_fill_rect`) — no bitmaps, no system fonts for digit rendering.

---

## Digit geometry

Each digit is drawn with a UNIT-based grid (8px on emery, 6px on others):

- **Stroke width:** 1u
- **Arc caps:** outer radius 2u, inner radius 1u — invariant, don't scale with height
- **Vertical bars:** stretch with digit height `h`
- **Minimum height** `H_MIN = 11u` — at this size, arcs just touch, bars disappear
- **Digit slot:** 5u wide (GLYPH_W = 4u content + 0.5u padding each side)

The digit `1` uses a diagonal chamfer serif at top-left. Digits `1`, `2`, `7` use `gpath_create` for diagonal strokes — these allocate and free heap on every draw frame. NULL checks are in place.

---

## Layouts

Three display modes, triggered by shake (or always-on via config):

| Layout | Description |
|--------|-------------|
| Full | Full-screen time, squish animation on every minute change |
| Wide | Up to 3 info lines above + 3 below the time; time scales to fill remaining space |
| Stacked Left/Right | Stacked hour/minute digits on one side, 8 info lines in a column on the other |

The UP button (emery only) cycles the background corner radius: 16px → 24px → 32px.

---

## Info line slots

Info lines display health, weather, solar, and status data. Each slot has wide and stacked variants. The stacked column on emery has ~99px available (no icon) or ~75px (with 16px icon + 8px gap).

**Data caching philosophy: hide > mislead.** If data is unavailable or stale, the slot returns nothing and the line disappears entirely rather than showing zeros or stale values. Weather and UV data ages out after 3 hours without a phone update; sunrise/sunset never ages out (astronomical, valid all day).

Slot IDs and their display strings:

| ID | Name | Stacked | Wide |
|----|------|---------|------|
| 0 | Empty | — | — |
| 1 | Day | "Wednesday" | "Wednesday" |
| 2 | Date | "Jun 16" | "June 16" |
| 3 | Day & Date | "Wed, Jun 16" | "Wednesday, June 16" |
| 4 | Temp | "72F" | "72F" |
| 5 | Weather | "72F" | "72F, partly cloudy" |
| 6 | Steps | "1,234" | "1,234 steps · 0.8mi" |
| 7 | Distance | "0.8mi" | "0.8mi" |
| 8 | Typical Steps | "exp 8,000" | "exp 8,000" |
| 9 | Step Pace % | "85%" | "exp 8,000 · 85%" |
| 10 | Calories | "312" | "312 cal · 72 bpm" |
| 11 | Heart Rate | "72" | "72 bpm" |
| 12 | Sunrise | "6:15am" | "6:15am" |
| 13 | Sunset | "8:22pm" | "8:22pm" |
| 14 | Daylight | "14h07m" | "14h07m" |
| 15 | Battery | "84%" | "84%" |
| 16 | Bluetooth | "no bt" (only when disconnected) | — |
| 17 | Sunrise & Sunset | "6:15am" | "6:15am · 8:22pm" |
| 18 | Steps & Typical | — | "1,234 steps · exp 8,000" |
| 19 | Typical & Pace | — | "exp 8,000 · 85%" |
| 20 | Battery & BT | — | "84% · no bt" |
| 21 | Debug | "[ Debug ]" | "[ Tallboy Debug Text ]" |
| 22 | UV Index | "UV 5" | "UV index 5" |
| 23 | Light Remaining | "3h 20m light/dark" | "3h 20m daylight left / til sunrise" |
| 24 | UV & Light | — (wide only) | "UV 5 · 3h 20m light" |
| 25 | Temp & UV Index | — (wide only) | "72F · UV 5" |

**Note:** Stacked `Light Remaining` text currently runs long (~130px estimated vs ~75px budget with icon). To fix: use `"%dh%02dm"` format (icon conveys day/night context). Tracked for a future update.

---

## Step pace background color

On color Pebbles with health data, the background reflects today's step pace vs. 7-day expected average:

| % of expected | Color |
|---------------|-------|
| 0 / no data | Black |
| 1–30% | Red |
| 31–60% | Orange |
| 61–90% | Yellow |
| 91–200% | Green |
| 201–400% | Blue |
| 401–700% | Indigo |
| 701–1000% | Vivid Violet |
| 1001%+ | Black |

---

## Animation

All animation uses `AppTimer` at 16ms (~60fps):

- **Minute change (full/wide):** anticipation nudge → squish to H_MIN → digit swap → re-expand with 1u overshoot snap
- **Stacked layouts:** no squish animation; digits update in place
- **Layout transitions:** split-V (digits separate vertically, colon exits) → split-H (digits slide to stacked positions, info column slides in) → reverse on return
- **Shake to full:** digits pulse with a 7-step sinusoidal wave

---

## Font metrics (emery, Gothic 24 Bold)

| Constant | Value | Notes |
|----------|-------|-------|
| `INFO_LINE_H` | 28px | Full font cell height |
| `INFO_TOP_PAD` | 10px | Dead space above cap height |
| `INFO_GLYPH_H` | 18px | Cap-top to descender-bottom |
| `INFO_LINE_STEP` | 26px | Line spacing (1px gap between glyphs) |
| `ICON_W` | 16px on emery, 12px on others | Icon box size (2 × UNIT) |

Approximate character widths for pixel budget estimation (Gothic 24 Bold, emery):
digits ~14px, `h` ~12px, `m` ~17px, lowercase avg ~12px, space ~5px, colon ~7px.

---

## Platform notes

### Button mapping (emery)
- **UP:** cycles background corner radius (16 → 24 → 32 → 16px)
- **SELECT:** OS-reserved, cannot be intercepted by watchfaces
- **DOWN:** no-op (subscribed to suppress default OS behavior)

### Touch API (emery) — non-functional
The emery (Pebble Time 2) SDK headers expose `touch_service_subscribe` / `touch_service_unsubscribe` and `TouchEvent`, suggesting touchscreen support. We implemented and tested a touch handler that was intended as a debug shortcut to cycle the corner radius.

**Finding:** `touch_service_subscribe` compiled and ran without errors, but the touch handler was never called during any device testing. Touch events appear to be non-functional at the firmware level on shipping emery hardware — the API exists in the SDK but is not wired up. All `PBL_TOUCH` code has been removed from this project.

This is consistent with the Pebble community's general understanding that the Time 2's touchscreen was not exposed to third-party apps before Pebble's shutdown.

---

## Weather & solar data

Fetched by PebbleKit JS (`src/pkjs/index.js`) on watch connect and every 30 minutes thereafter, using the [Open-Meteo API](https://open-meteo.com/). Sends to watch:

- `WeatherTempF` / `WeatherTempC` / `WeatherCode` — current conditions
- `UvIndex` — current UV index (0 is a valid value; -1 sentinel used on watch before first receipt)
- `SunriseTime` / `SunsetTime` — today's solar times
- `SunriseTomorrow` — for overnight "time until sunrise" calculation

Weather code mapping follows WMO standards (0 = clear, 1–3 = partly cloudy, 45–48 = fog, 51–69 / 80–82 = rain, 71–77 / 85–86 = snow, 95–99 = storm).

---

## Known issues / future work

- Stacked `Light Remaining` text too wide with icon (~130px, budget ~75px) — shorten to `"%dh%02dm"` format
- Legacy source files present but unused: `src/digit.c`, `src/digit.h`, patch `.txt` files — can be deleted
- Legacy resource images in `resources/` — can be pruned
