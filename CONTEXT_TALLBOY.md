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
└── src/main.c       ← entire watchface, currently v3.22
```

---

## 3. PLATFORM TARGETS

| Platform | Watch | Screen | Notes |
|----------|-------|--------|-------|
| aplite | Pebble Classic | 144×168 rect | B&W |
| basalt | Pebble Time | 144×168 rect | Color |
| chalk | Pebble Time Round | 180×180 round | Color |
| diorite | Pebble 2 SE | 144×168 rect | B&W, HR |
| emery | Pebble Time 2 | 200×228 rect | Color, HR, touch — Sterling's primary device |
| flint | Pebble 2 | 144×168 rect | B&W, HR |

Primary dev: **emery** (UNIT=8) and **flint** (UNIT=6).

---

## 4. LAYOUT SYSTEM (v3.22 — 4 layouts)

Cycle via shake (accel tap). Shake during countdown skips to blink-in.

| # | Name | Description |
|---|------|-------------|
| 0 | `LAYOUT_WIDE` | Full-screen raster digits, winks on minute change |
| 1 | `LAYOUT_VECTOR` | Full-screen vector digits, winks on minute change |
| 2 | `LAYOUT_LEFT` | Stacked left raster + info column right |
| 3 | `LAYOUT_RIGHT` | Stacked right vector + info column left |

`LAYOUT_WIDE_COMPS` is in code but removed from cycle.

---

## 5. ANIMATION SYSTEM (v3.22) — LOCKED

### Size constants
```
WIDE_FULL_SIZE  = 5   // resting/display size
ANIM_PEAK_SIZE  = 6   // overshoot only, never resting
ANIM_MIN_SIZE   = 2   // squish floor — size 1 RETIRED
```

**Size 1 is permanently retired.** All animations floor at size 2.

### Size range
- **Size 2:** `11u` = 88px emery / 66px flint ≈ 40% screen height — squish minimum
- **Size 5:** resting display, good margins
- **Size 6:** animation overshoot only (120ms snap back to 5)

### SIZE_CYCLE
```c
static const int SIZE_CYCLE[] = { 5, 4, 3, 2, 3, 4, 5, 6, 5 };
#define SIZE_CYCLE_LEN 9
```

### Animation timing constants
```
ANIM_FAST_MS              = 80    // normal step speed
COUNTDOWN_STEP_MS         = 160   // countdown step speed (2× slow for verification)
COUNTDOWN_EXPAND_HOLD_MS  = 1000  // hold at full size during countdown
COUNTDOWN_SHRINK_HOLD_MS  = 1000  // hold at min size during countdown
ANIM_SNAP_MS              = 120   // overshoot hold before snap to WIDE_FULL_SIZE
```
**Note:** COUNTDOWN_STEP_MS and hold times are doubled for pixel-level screenshot verification. Revert to normal (160→80, 1000→500) after verification pass.

---

## 6. VECTOR DIGIT GEOMETRY (v3.22 — LOCKED)

**Fundamental rule:** Circles NEVER change size. `ro = UNIT*2`, `ri = UNIT*1`, `sw = UNIT`. Only VBARs stretch.

### Key variables
```c
const int ro = UNIT * 2;
const int ri = UNIT * 1;
const int sw = UNIT;
int h      = (3 + 4*size) * UNIT;
int gx     = slot_x + HALF_SLOT_PAD;
int gx_r   = gx + GLYPH_W - sw;
int cap_cx = gx + ro;
int top_cy = cy - (h - ro*2) / 2;
int bot_cy = cy + (h - ro*2) / 2;
int top_y  = cy - h / 2;
int bot_y  = cy + h / 2;
int bar  = (size - 1) * 2 * UNIT;
int t_bc = cy - (ro - HALF_UNIT);
int t_tc = t_bc - bar;
int b_tc = cy + (ro - HALF_UNIT);
int b_bc = b_tc + bar;
int tail = (size > 2) ? (size - 2) * UNIT : 0;
// tail: sizes 1-2=0u, 3=1u, 4=2u, 5=3u, 6=4u
```

### Digit constructions

**0:** top/bottom caps + left/right VBARs center-to-center

**1:** Base HBAR + centered stem + 5-point pentagon cap

**2:** Top semi + tail VBARs + GPath diagonal + base HBAR

**3:** Two rings, NUB at `(gx+sw, t_bc+sw)`, lower-left tail `VBAR(gx, b_bc-tail, b_bc)`

**4:** Left short VBAR to `cy+sw` + right full VBAR + crossbar at `cy`

**5:**
```c
HBAR(top_y);
VBAR(gx,   top_y + sw, b_tc);        // top-left bar to ring center
fill_arc(b_tc, 270, 450);            // top of bottom ring
fill_arc(b_bc, 90, 270);             // bottom of bottom ring
VBAR(gx_r, b_tc, b_bc);
if (tail > 0) VBAR(gx, b_bc-tail, b_bc);  // lower-left tail (sizes 3+ only)
```

**6:** Top semi + right arm tail from `top_cy` + left full VBAR + bottom ring

**7:** HBAR + GPath parallelogram

**8:** Two stacked rings, all bars center-to-center

**9:** Full top ring + right bar to `bot_cy` + left tail `VBAR(gx, bot_cy-tail, bot_cy)` + bottom semi

---

## 7. STEP PACE BACKGROUND COLOR (emery only)

`PBL_COLOR && PBL_HEALTH` only.

Spectrum: no-history→black, <60%→red, <80%→orange, <90%→yellow, ≤110%→green, ≤130%→cyan, ≤160%→blue, >160%→white.

White/Yellow/Cyan → `s_fg = GColorBlack`; others → `s_fg = GColorWhite`.

---

## 8. RASTER SYSTEM (to be retired after pixel verification)

- 10 digits × 6 sizes = 60 bitmaps per platform
- **DEBUG active:** `blit()` draws 3px red stripe at top of each raster slot for raster/vector comparison
- **Retirement plan:** after pixel-level verification in Photoshop, raster removed entirely

---

## 9. NEXT UP: PIXEL VERIFICATION & RASTER RETIREMENT

1. Screenshot all digits at all sizes using slow countdown
2. Compare raster (red-tagged outer slots) vs vector (inner slots) in Photoshop
3. Make any pixel-level geometry corrections
4. Remove raster system, debug stripe, `LAYOUT_WIDE`, cleanup
5. Revert countdown timing to normal speed

---

## 10. PENDING FEATURES (post-cleanup)

- Full Radium 2 info line field system (FIELD_NONE…FIELD_STEPS_PROJ=17)
- Shake-to-reveal info overlay (~1min auto-dismiss)
- Weather/solar JS fetch
- Hourly step pace comparison mode
- Config page
- Full-vector dynamic sizing: `unit = floor((available_h - 2*margin) / digit_h_formula)`

---

## 11. DESIGN TENET

*Watchfaces display data as atmosphere, not content. Artfully vague > precisely readable.*

---

## 12. CLOUDPEBBLE / BUILD RULES

1. Remove `resources/media` from appinfo.json
2. `src/main.c` flat path
3. Full files only — never partial diffs
4. `create_or_update_file` with current SHA; **never use `push_files`** (sends empty content)
5. No tilde in resource filenames

---

*End of context seed. Last updated: v3.22, May 2026.*
