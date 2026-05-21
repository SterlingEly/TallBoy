# TallBoy

A Pebble watchface built around oversized vector-drawn digits that scale dynamically — stretching to fill the screen and squishing as an animation mechanic on every minute change.

**Design:** Sterling Ely  
**Implementation:** Sterling Ely + Claude  
**Primary device:** Pebble Time 2 (emery, 200×228, color, touch)

---

## Architecture

Everything lives in `src/main.c`. Digits are drawn entirely from geometric primitives (`fill_arc` + `graphics_fill_rect`) — no bitmaps, no system fonts for digit rendering.

```
src/main.c      — entire watchface (digits, layout, animation, health, info lines)
appinfo.json    — app metadata, message keys
resources/      — unused legacy image resources (to be pruned)
```

> **Note:** `src/digit.c`, `src/digit.h`, `src/case8_patch.txt`, and `src/main_digit1_fix.txt` are legacy files that should be deleted. `digit.c` is still compiled by the build system and produces a harmless `hstroke` warning on every build.

---

## Digit geometry

Each digit is drawn with a UNIT-based grid (8px on emery, 6px on flint):

- **Stroke width:** 1u (UNIT = 8px emery)
- **Arc caps:** outer radius 2u, inner radius 1u — invariant, never change with height
- **Vertical bars:** stretch with digit height `h`
- **Minimum height** `H_MIN = 11u` — at this size, arcs just touch, bars disappear
- **Digit slot:** 5u wide (GLYPH_W = 4u content + 0.5u padding each side)

The digit `1` uses a diagonal chamfer serif at top-left. Digits `1`, `2`, `7` use `gpath_create` for diagonal strokes — these allocate and free heap on every draw frame. NULL checks are in place.

---

## Layout system

Shake wrist to cycle through 6 layouts:

| # | Name | Description |
|---|------|-------------|
| 0 | Full | Full-screen time, squish animation on minute change |
| 1 | Info 1+1 | 1 info line above + 1 below, time fills middle |
| 2 | Info 2+2 | 2 above + 2 below |
| 3 | Info 3+3 | 3 above + 3 below |
| 4 | Stack R | Stacked digits right, info column left |
| 5 | Stack L | Stacked digits left, info column right |

Touch the screen (emery only) to cycle the background corner radius: 2u → 3u → 4u → 2u.

---

## Info lines

**Wide layouts (Info 1–3):** lines anchor to screen edges (top/bottom), time digits fill remaining space and scale dynamically down to H_MIN.

**Stacked layouts:** 8 lines distributed evenly in a fixed column beside the digits.

Content (in order): weather · sunrise/sunset · steps · expected steps/pace · HR & calories · battery

---

## Step pace background color

On emery with health data, the background reflects today's step pace vs. a 7-day expected average:

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
| 1001%+ | Black (easter egg) |

---

## Animation

All animation uses `AppTimer` at 16ms (~60fps), 8px per step.

- **Launch countdown:** 9→0, digits squish and re-expand between each digit
- **Minute change:** squish to H_MIN, swap digits, re-expand with 1u overshoot snap
- **Layout change:** squish transition to new layout
- **Shake to Full:** 9-point pulse wave animation

---

## Font metrics (emery, Gothic 24 Bold — measured on device)

| Constant | Value | Description |
|----------|-------|-------------|
| `INFO_LINE_H` | 28px | Full font cell height |
| `INFO_TOP_PAD` | 10px | Dead space above glyph top |
| `INFO_GLYPH_H` | 18px | Cap-top to descender-bottom |
| `INFO_DESCENDER` | 4px | Descender depth (g, y, comma) |
| `INFO_CAP_OFFSET` | 3px | Cap height above x-height |
| `INFO_LINE_STEP` | 26px | Rect-top spacing for 1u glyph gap |

Text is positioned using `draw_text_at_glyph_y()` which shifts each rect up by `INFO_TOP_PAD` so the visual glyph lands at the intended pixel.

---

## Pending

- Weather/solar JS fetch (port from Radium2 `src/pkjs/`)
- `MESSAGE_KEY_SunriseMin` / `MESSAGE_KEY_SunsetMin` inbox handling (stub present)
- Config page (Clay vs custom HTML)
- Touch easter egg: per-digit squish using touch x/y coordinates
- Prune legacy resource images from `resources/`
- Delete dead source files: `digit.c`, `digit.h`, patch `.txt` files
