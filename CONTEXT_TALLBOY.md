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
└── src/main.c       ← entire watchface, currently v3.20c
```

Current HEAD: `600806db99f9cc0ca3573226598d999e354326fa`

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
| gabbro | Pebble Round 2 | 260×260 round | Color, Health |

Primary dev: **emery** (UNIT=8) and **flint** (UNIT=6).

---

## 4. LAYOUT SYSTEM (v3.20c — 4 layouts)

Cycle via shake (accel tap). Shake during countdown skips to blink-in.

| # | Name | Description |
|---|------|-------------|
| 0 | `LAYOUT_WIDE` | Full-screen raster digits, winks on minute change |
| 1 | `LAYOUT_VECTOR` | Full-screen vector digits, winks on minute change |
| 2 | `LAYOUT_LEFT` | Stacked left raster + info column right |
| 3 | `LAYOUT_RIGHT` | Stacked right vector + info column left |

`LAYOUT_WIDE_COMPS` (raster + companion text above/below) is implemented in code but removed from the shake cycle. Will be re-added later with info line system.

---

## 5. ANIMATION SYSTEM (v3.20c)

### Size constants
- `WIDE_FULL_SIZE = 5` — resting/display size
- `ANIM_PEAK_SIZE = 6` — animation overshoot only (not used as resting size)
- Sizes 1–6 are integer steps; full-vector future will replace this with continuous unit scaling

### Animations
- **Countdown wink:** Boot shows digits 9→0, each with shrink→hold→expand overshoot→hold cycle (500ms holds, 80ms/step animation)
- **Squish:** On minute change in WIDE/VECTOR layouts: shrink to 1 → swap time → expand with overshoot to 6 → snap to 5
- **Blink:** After countdown, 2× squish blinks before settling
- **Shake cycle:** `SIZE_CYCLE = {5,4,3,2,1,2,3,4,5,6,5}` — bounces down to 1 and back up with overshoot snap
- **Overshoot:** All expand phases go to size 6 briefly (`ANIM_SNAP_MS=120ms`), then snap to size 5
- **`s_overshot` bool** tracks whether we're at peak waiting to snap

### Notes on size extremes
- Size 1 is used only for animation squish minimum — looks bad as a resting size
- Size 6 is animation-only overshoot — too tight margins for resting
- Size 5 is the "correct" display size with good margins
- Future full-vector mode: sizes become continuous, computed from available height with 2u canvas margin + 1u inner spacing

---

## 6. VECTOR DIGIT GEOMETRY (v3.20c — LOCKED)

**Fundamental rule:** Circles NEVER change size. `ro = UNIT*2`, `ri = UNIT*1`, `sw = UNIT`. Only VBARs stretch.

### Key variables
```c
const int ro = UNIT * 2;   // arc outer radius
const int ri = UNIT * 1;   // arc inner hole radius
const int sw = UNIT;       // stroke width

int h      = (3 + 4*size) * UNIT;
int body_h = h - ro * 2;
int gx     = slot_x + HALF_SLOT_PAD;         // left edge of glyph
int gx_r   = gx + GLYPH_W - sw;             // right VBAR x
int cap_cx = gx + ro;                        // arc center x

int top_cy = cy - body_h / 2;               // 0-style: top cap center
int bot_cy = cy + body_h / 2;               // 0-style: bottom cap center
int top_y  = cy - h / 2;                    // top canvas edge
int bot_y  = cy + h / 2;                    // bottom canvas edge

// 8-style ring positions (used by 3,5,6,8,9)
int bar  = (size - 1) * 2 * UNIT;
int t_bc = cy - (ro - HALF_UNIT);           // top ring inner cap center
int t_tc = t_bc - bar;                      // top ring outer cap center
int b_tc = cy + (ro - HALF_UNIT);           // bottom ring inner cap center
int b_bc = b_tc + bar;                      // bottom ring outer cap center

// Universal tail (scales with size)
int tail = (size > 2) ? (size - 2) * UNIT : 0;
// sizes 1-2: 0u  size 3: 1u  size 4: 2u  size 5: 3u  size 6: 4u
```

### Digit constructions (LOCKED v3.20c)

**0:** top/bottom caps + left/right VBARs center-to-center

**1:** Base HBAR + centered stem + 5-point pentagon cap clipped at `top_y-1`:
```c
GPoint pts[5] = {
  {cap_right, top_y - 1},
  {cap_right, top_y + sw - HALF_UNIT - 1},
  {gx,        top_y + sw + diag_h - HALF_UNIT - 1},
  {gx,        top_y + diag_h - sw - 1},
  {stem_x,    top_y - 1},
};
```

**2:** Top semi + symmetric tail VBARs + GPath diagonal + base HBAR:
```c
GPoint pts[4] = {
  {gx_r - 1,         top_cy + tail},
  {gx_r + sw,        top_cy + tail},
  {gx  - 1 + sw + 2, bot_y - sw},
  {gx  - 1,          bot_y - sw},
};
```

**3:** LOCKED — NUB at `(gx+sw, t_bc+sw)`:
```c
fill_arc(t_tc, 270,450); VBAR(gx, t_tc, t_tc+tail); VBAR(gx_r, t_tc, t_bc);
fill_arc(t_bc, 90,180); NUB(gx+sw, t_bc+sw);
fill_arc(b_tc, 360,450);
VBAR(gx, b_bc-tail, b_bc); VBAR(gx_r, b_tc, b_bc);
fill_arc(b_bc, 90,270);
```

**4:** Left short VBAR to `cy+sw` + right full VBAR + crossbar at `cy`

**5:** Top HBAR + left bar to `b_tc-ro` + bottom ring + right bar c-to-c + bottom-left tail:
```c
HBAR(top_y);
VBAR(gx,   top_y + sw, b_tc - ro);         // top-left bar stops at TOP of ring arc
fill_arc(b_tc, 270,450); fill_arc(b_bc, 90,270);
VBAR(gx_r, b_tc, b_bc);                    // right bar center-to-center
VBAR(gx,   b_bc - tail, b_bc);             // bottom-left tail: same as digit 3
```
**Key:** top-left bar ends at `b_tc - ro` (not `b_tc`) to keep mouth open at all sizes.

**6:** Top semi + right arm tail from `top_cy` + left full + bottom ring:
```c
fill_arc(top_cy, 270,450);
VBAR(gx_r, top_cy, top_cy + tail);
VBAR(gx, top_cy, b_bc);
fill_arc(b_tc, 270,450); fill_arc(b_bc, 90,270);
VBAR(gx_r, b_tc, b_bc);
```

**7:** HBAR + GPath parallelogram:
```c
GPoint pts[4] = {
  {gx_r - 1,    top_y + sw},
  {gx_r + sw,   top_y + sw},
  {gx + sw + 1, bot_y},
  {gx,          bot_y},
};
```

**8:** Two stacked rings, all bars center-to-center. Perfect. Don't change.

**9:** Full top ring + right bar to `bot_cy` + left tail at `(bot_cy-tail, bot_cy)` + bottom semi:
```c
fill_arc(t_tc, 270,450); fill_arc(t_bc, 90,270);
VBAR(gx, t_tc, t_bc);
VBAR(gx, bot_cy - tail, bot_cy);           // left tail anchored at bot_cy
VBAR(gx_r, t_tc, bot_cy);
fill_arc(bot_cy, 90,270);
```

---

## 7. STEP PACE BACKGROUND COLOR (v3.20c — emery only)

Active on `PBL_COLOR && PBL_HEALTH` platforms.

### Spectrum
`steps_avg <= 0` → black (no history), then:
- `pct < 60%` → Red
- `pct < 80%` → Orange
- `pct < 90%` → Yellow
- `pct <= 110%` → Green
- `pct <= 130%` → Cyan
- `pct <= 160%` → Blue
- `pct > 160%` → White

### Foreground inversion
`prv_bg_needs_dark_fg()` returns true for White, Yellow, Cyan → `s_fg = GColorBlack`.
Vector digits use `s_fg` directly. Raster digits use `GCompOpAssignInverted` when `s_fg == GColorBlack`.

### Historical average calculation
`prv_calc_steps_avg()` — static buffer `s_minute_buf[120]`, up to 7-day lookback via `health_service_get_minute_history()`. Capped at 120 minutes per day (scales result if elapsed > window). Recalculates every minute. Debug info line "avg XXXX" shown until confirmed working.

### Two comparison modes (planned)
1. **Cumulative daily** — today's total vs. historical average total at same time (current implementation)
2. **Hourly** — current hour vs. historical average for this specific hour (planned option)

---

## 8. RASTER DIGIT SYSTEM

- 10 digits × 6 sizes = 60 bitmaps per platform
- Emery: `TALLBOY_01`…`TALLBOY_96`; Low-res: `TALLBOY_L01`…`TALLBOY_L96`
- Colons: `TALLBOY_COLON1-6` / `TALLBOY_LCOLON1-6`
- `free_digit_bitmaps(digit)` called between countdown digits
- Raster system will be deprecated once full-vector is validated

---

## 9. INFO LINES (current — basic)

Font: `FONT_KEY_GOTHIC_18_BOLD` (flint), `FONT_KEY_GOTHIC_24_BOLD` (emery).
Available Gothic sizes: 14, 18, 24, 28 only.

Current fields: day name, date, steps, distance, avg steps, battery %.
Full Radium 2 field system planned (FIELD_NONE through FIELD_STEPS_PROJ=17).

---

## 10. FUTURE VISION — FULL VECTOR MODE

### Sizing becomes dynamic
- No more integer size 1–6 slots
- `unit = floor((available_h - 2*canvas_margin) / digit_h_formula)`
- All outer margins = 1u canvas padding; inner element margins = 1u
- Max size fills screen to 1u margin top/bottom; min size = animation floor (maybe 3–4px)
- Animation terms change: "squish to min_unit," "expand past target_unit"

### Raster system retired
- Currently: LAYOUT_WIDE uses raster, LAYOUT_VECTOR uses vector
- Future: all layouts use vector
- Raster PNG assets kept as archive but not loaded

### Shake-to-reveal info
- Wide/vector layouts: shake brings up configured info lines for ~1 min, auto-dismisses
- Second shake dismisses early
- Stacked layouts: info column always visible

---

## 11. PLANNED FEATURES

1. Port full info line field system from Radium 2 (FIELD_NONE…FIELD_STEPS_PROJ=17)
2. Port icon drawing functions from Radium 2
3. Implement shake-to-reveal info overlay (wide/vector layouts, ~1min auto-dismiss)
4. Weather/solar JS fetch (port from Radium 2 index.js)
5. Hourly step pace comparison mode (vs. current cumulative daily)
6. Config page
7. Full vector mode: retire raster, compute size dynamically from available space
8. Pixel-level digit verification at all sizes (sizes 2–5 need review after full-vector)

---

## 12. DESIGN TENET — AMBIENT INFORMATION

*Watchfaces display data as atmosphere, not content. Artfully vague > precisely readable.*

Color, arc fill, dot position = mood/direction in a 2-sec glance. Cognitive friction ("why is it blue?") beats ignored stats. The watch is the barometer; the phone is the app. Every face rewards attention without demanding it.

---

## 13. CLOUDPEBBLE / BUILD RULES

1. Remove `resources/media` block from appinfo.json (causes "Unsupported published resource type" error)
2. Menu icons via CloudPebble UI only (not via appinfo)
3. No tilde (`~`) in resource filenames (breaks GitHub import)
4. Font resources via CloudPebble UI "Another Font" button (not appinfo)
5. Always provide full files for copy-paste — never partial diffs
6. `src/main.c` flat path expected by CloudPebble (not `src/c/main.c`)
7. `#define WIDE_FULL_SIZE 5` — resting size; `#define ANIM_PEAK_SIZE 6` — animation only

---

## 14. GITHUB MCP NOTES

- `create_or_update_file` with current SHA for all pushes — prior session SHAs may be stale
- `push_files` sends EMPTY content — **never use it**
- TallBoy repo uses `main` branch
- `get_file_contents` times out on files above ~15KB
- Empty blob SHA `e69de29bb2d1d6434b8b29ae775ad8c2e48c5391` means `push_files` silently emptied a file

---

*End of context seed. Last updated: v3.20c, May 2026.*
