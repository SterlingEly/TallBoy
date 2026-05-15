# TALLBOY — CONTEXT SEED FOR NEW THREAD
*Everything a fresh Claude session needs to resume TallBoy development*

---

## 1. WHAT IS THIS PROJECT?

**TallBoy** is a Pebble watchface built around oversized vector-drawn digits that scale dynamically — "tall boys" that stretch and squish as an animation mechanic and fill the screen with pure time. Design by Sterling Ely (2026), implemented by Sterling Ely + Claude.

**Sterling:** Design/concept lead, CloudPebble builds, Photoshop bitmap assets, device testing (Pebble Time 2 / emery).
**Claude:** Code, GitHub commits, documentation.

---

## 2. REPO

```
SterlingEly/TallBoy (branch: main)
└── src/main.c       ← entire watchface, currently v3.24
```

---

## 3. PLATFORM TARGETS

| Platform | Watch | Screen | UNIT | Notes |
|----------|-------|--------|------|-------|
| emery | Pebble Time 2 | 200×228 rect | 8px | Color, HR, touch — **primary dev device** |
| flint | Pebble 2 | 144×168 rect | 6px | B&W, HR |
| basalt | Pebble Time | 144×168 rect | 6px | Color |
| diorite | Pebble 2 SE | 144×168 rect | 6px | B&W, HR |
| aplite | Pebble Classic | 144×168 rect | 6px | B&W |

Emery has 25 units (200px / 8px = 25u exactly). All others have 24u (144px / 6px = 24u).
Pixel verification: emery first, then low-res pass after emery geometry is locked.

---

## 4. HORIZONTAL LAYOUT — THE UNIT SYSTEM

Every digit is **4u wide** (GLYPH_W) within a **5u slot** (SLOT_W).
The half-unit padding on each side creates the 1u visual gap between adjacent digits.

```
[margin] [½u pad | 4u digit | ½u pad] ×4 + [½u pad | 1u colon | ½u pad] + [margin]
       = margin + 5u×4 + 2u + 5u + margin = 23u content
```

- **Emery:** 25u total → 1u margin each side (8px each)
- **Flint/others:** 24u total → ½u margin each side (3px each)
- All strokes are exactly **1u** wide (`sw = UNIT`)
- Arc radius is always **2u** outer, **1u** inner (`ro = 2*UNIT, ri = UNIT`)

---

## 5. VERTICAL DIGIT STRUCTURE

Each digit is built from up to 4 layers. **Arc sizes never change** — only vertical spans stretch.

### Layer 1: Arc Cap (top)
Fixed size, moves with top boundary.
| Type | Description | Used by |
|------|-------------|---------|
| **Arc cap** | Top half of 2u-radius ring (2u tall, 4u wide) | 0, 2, 3, 6, 8, 9 |
| **HBAR cap** | Full-width 1u rectangle at top | 5, 7 |
| **Diagonal cap** | Bespoke 5-point pentagon path (45° serif) | 1 only |
| **None** | No top cap | 4 only |

### Layer 2: Spans
| Type | Description | Used by |
|------|-------------|---------|
| **Full span** | Runs full digit height; does not resize | 0 (sides), 1 (stem), 4 (right), 6 (left), 9 (right) |
| **Bridge span** | Connects inner ring centers; resizes as `bar = (size-1)*2u` | 3, 5, 6, 8, 9 |
| **Diagonal span** | GPath parallelogram | 2, 7 |
| **Tail span** | Stub from outer ring center toward canvas edge; `tail = (size>2)?(size-2)*u:0` | 3 (×2), 5, 6, 9 |

The **tail** variable is the key to consistent scaling: zero at size 2 (minimum), grows 1u per size step. All tail spans across all digits use the same formula.

### Layer 3: Center Elements
Anchored to `cy`; never move or resize.
| Element | Description | Used by |
|---------|-------------|---------|
| **Crossbar** | HBAR at `cy - HALF_UNIT` | 4 |
| **Ring pair** | 8's two rings centered via `t_tc/t_bc + b_tc/b_bc` | 8 |
| **Center arc** | Bottom of top ring / top of bottom ring | 6, 9 |

### Layer 4: Arc Base (bottom)
Mirror of arc cap; fixed size, moves with bottom boundary.
| Type | Description | Used by |
|------|-------------|---------|
| **Arc base** | Bottom half of 2u-radius ring | 0, 3, 5, 6, 8, 9 |
| **Flat base** | Full-width 1u HBAR | 2 only |
| **None** | Reaches canvas floor directly | 1, 4, 7 |

---

## 6. KEY GEOMETRY VARIABLES

All derived from `cy` (vertical center of digit slot):

```c
const int ro = UNIT * 2;       // arc outer radius — NEVER changes with size
const int ri = UNIT * 1;       // arc inner radius — NEVER changes with size
const int sw = UNIT;           // stroke width    — NEVER changes with size

int gx     = slot_x + HALF_SLOT_PAD;  // left stroke left edge
int gx_r   = gx + GLYPH_W - sw;       // right stroke left edge
int cap_cx = gx + ro;                 // arc center x (always gx + 2u)

int h     = (3 + 4*size) * UNIT;     // total digit height
int top_y = cy - h / 2;             // canvas top
int bot_y = cy + h / 2;             // canvas bottom

// Ring system (3, 5, 6, 8, 9):
int bar  = (size - 1) * 2 * UNIT;   // ring gap between inner cap centers
int t_bc = cy - (ro - HALF_UNIT);   // top ring inner cap center
int t_tc = t_bc - bar;              // top ring outer cap center
int b_tc = cy + (ro - HALF_UNIT);   // bottom ring inner cap center
int b_bc = b_tc + bar;              // bottom ring outer cap center

// Tail span (3, 5, 6, 9):
int tail = (size > 2) ? (size - 2) * UNIT : 0;
// size 2 (min): 0u  size 3: 1u  size 4: 2u  size 5: 3u  size 6: 4u
```

Note: `top_cy = cy - (h - ro*2)/2` and `bot_cy = cy + (h - ro*2)/2` are used only by digit 0 (the simple oval). The ring system variables (`t_tc` etc.) are used by all other multi-arc digits.

---

## 7. ANIMATION SYSTEM — LOCKED CONSTANTS

```
WIDE_FULL_SIZE  = 5   // resting display size — good margins on all platforms
ANIM_PEAK_SIZE  = 6   // overshoot only (120ms snap back to 5)
ANIM_MIN_SIZE   = 2   // squish floor — size 1 PERMANENTLY RETIRED
```

**Size 2 = 11u = 88px/66px ≈ 40% screen height** — minimum legible at all sizes.
**Size 6 = 27u = 216px/162px** — too large to rest but great for the overshoot pop.

```c
static const int SIZE_CYCLE[] = { 5, 4, 3, 2, 3, 4, 5, 6, 5 };  // shake animation
#define SIZE_CYCLE_LEN 9
```

### Timing constants
```
ANIM_FAST_MS             = 80    // normal step interval
ANIM_SNAP_MS             = 120   // hold at overshoot peak before snap to size 5
COUNTDOWN_STEP_MS        = 160   // ← 2× SLOW for verification; revert to 80
COUNTDOWN_EXPAND_HOLD_MS = 1000  // ← 2× SLOW for verification; revert to 500
COUNTDOWN_SHRINK_HOLD_MS = 500   // normal
```

### Animation phases
- **COUNTDOWN:** 9→0 wink sequence at startup; outer slots raster (red stripe), inner slots vector
- **BLINK:** 2× squish blinks after countdown before settling at size 5
- **SQUISH:** minute-change animation in WIDE/VECTOR layouts; swaps time at minimum size
- **SHAKE_CYCLE:** accel-tap triggered; runs SIZE_CYCLE twice
- All expand phases overshoot to ANIM_PEAK_SIZE, snap back after ANIM_SNAP_MS

---

## 8. LAYOUT SYSTEM (4 layouts, shake to cycle)

| # | Name | Description |
|---|------|-------------|
| 0 | `LAYOUT_WIDE` | Full-screen raster digits |
| 1 | `LAYOUT_VECTOR` | Full-screen vector digits |
| 2 | `LAYOUT_LEFT` | Stacked left raster + info column right |
| 3 | `LAYOUT_RIGHT` | Stacked right vector + info column left |

---

## 9. STEP PACE BACKGROUND COLOR (emery only)

`PBL_COLOR && PBL_HEALTH`. Compares today's steps to 7-day historical average.

| % of avg | Color | fg |
|----------|-------|----|
| no history | Black | White |
| < 60% | Red | White |
| < 80% | Orange | White |
| < 90% | Yellow | **Black** |
| ≤ 110% | Green | White |
| ≤ 130% | Cyan | **Black** |
| ≤ 160% | Blue | White |
| > 160% | White | **Black** |

---

## 10. RASTER SYSTEM — PENDING RETIREMENT

- 10 digits × 6 sizes = 60 bitmaps per platform (emery color + flint/low-res B&W)
- Split countdown: outer slots raster (H_TENS, M_ONES), inner slots vector (H_ONES, M_TENS)
- **DEBUG:** `blit()` draws 3px red stripe at top of each raster slot
- **Retirement trigger:** after pixel-level verification confirms vector geometry is correct

---

## 11. PIXEL VERIFICATION STATUS

- ✅ Emery size 2 and size 5 verified in Photoshop
- ✅ v3.23 geometry fixes applied (colon, 1, 2, 4, 7)
- ⏳ Final emery verification pass (v3.24 build)
- ⏳ Low-res (flint) verification pass
- ⏳ Raster retirement

---

## 12. PENDING FEATURES (post-raster-cleanup)

1. Revert countdown timing: `COUNTDOWN_STEP_MS→80`, `COUNTDOWN_EXPAND_HOLD_MS→500`
2. Remove raster system, red stripe debug, `LAYOUT_WIDE`, `s_demo_override` cleanup
3. Port full Radium 2 info line field system (FIELD_NONE…FIELD_STEPS_PROJ=17)
4. Shake-to-reveal info overlay in wide/vector layouts (~1min auto-dismiss)
5. Weather/solar JS fetch (port from Radium 2 index.js)
6. Hourly step pace comparison mode
7. Config page
8. Full-vector dynamic sizing: `unit = floor((available_h - 2u) / digit_h_formula)`

---

## 13. DESIGN TENET

*Watchfaces display data as atmosphere, not content. Artfully vague > precisely readable.*

Color, arc fill, dot position = mood/direction in a 2-sec glance. Cognitive friction ("why is it blue?") beats ignored stats. The watch is the barometer; the phone is the app.

---

## 14. CLOUDPEBBLE / BUILD RULES

1. Remove `resources/media` from appinfo.json (CloudPebble handles resources via UI)
2. `src/main.c` flat path (not `src/c/main.c`)
3. Full files only — never partial diffs
4. `create_or_update_file` with current SHA; **never use `push_files`** (sends empty content)
5. No tilde (`~`) in resource filenames

---

*End of context seed. Last updated: v3.24, May 2026.*
