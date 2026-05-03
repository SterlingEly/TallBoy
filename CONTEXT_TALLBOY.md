# TALLBOY — CONTEXT SEED FOR NEW THREAD
*Everything a fresh Claude session needs to resume TallBoy development*

---

## 1. WHAT IS THIS PROJECT?

**TallBoy** is a Pebble watchface built around oversized vector-drawn digits that scale dynamically — "tall boys" that stretch and squish as an animation mechanic and as a way to fill the screen with pure time. Design by Sterling Ely (2026), implemented by Sterling Ely + Claude.

**Sterling:** Design/concept lead, CloudPebble builds, Photoshop bitmap assets.
**Claude:** Code, GitHub commits, documentation.

---

## 2. REPO

```
SterlingEly/TallBoy (branch: main)
└── src/main.c       ← entire watchface, currently ~v3.6
```

---

## 3. PLATFORM TARGETS

| Platform | Watch | Screen | Notes |
|----------|-------|--------|-------|
| aplite | Pebble Classic | 144×168 rect | B&W |
| basalt | Pebble Time | 144×168 rect | Color |
| chalk | Pebble Time Round | 180×180 round | Color |
| diorite | Pebble 2 SE | 144×168 rect | B&W, HR |
| emery | Pebble Time 2 | 200×228 rect | Color, HR, touch |
| flint | Pebble 2 | 144×168 rect | B&W, HR |
| gabbro | Pebble Round 2 | 260×260 round | Color, Health |

Primary dev platform: **emery** (UNIT=8) and **flint** (UNIT=6).

---

## 4. LAYOUT SYSTEM

Five layouts cycle via shake (accel tap):

| # | Name | Description |
|---|------|-------------|
| 0 | `LAYOUT_WIDE` | Full-screen raster digits, no info |
| 1 | `LAYOUT_WIDE_COMPS` | Raster digits + companion info above/below |
| 2 | `LAYOUT_LEFT` | Stacked raster left, info column right |
| 3 | `LAYOUT_RIGHT` | Stacked raster right, info column left |
| 4 | `LAYOUT_VECTOR` | Full-screen vector digits |

---

## 5. ANIMATION SYSTEM

- **Squish:** Digits shrink to size 1 at minute change, then expand back. Time updates at size 1 midpoint.
- **Shake cycle:** SIZE_CYCLE = {6,5,4,3,2,1,2,3,4,5,6} — bounces sizes on layout change.
- **Countdown:** On boot, digits 9→0 displayed at size 6 for 1500ms each, alternating raster (outer slots) and vector (inner slots).
- **Blink:** After countdown, 2× squish blinks before settling at full size.

---

## 6. VECTOR DIGIT GEOMETRY

**Fundamental rule:** Circles NEVER change size. `ro = UNIT*2` (outer radius), `ri = UNIT*1` (inner hole), `sw = UNIT` (stroke). ONLY straight spans (VBARs) stretch with size.

### Unit grid
- emery: `UNIT=8`, `HALF_UNIT=4`
- flint/others: `UNIT=6`, `HALF_UNIT=3`

### Standard 0-style cap positions
```c
int h      = (3 + 4*size) * UNIT;   // total digit height
int body_h = h - ro * 2;
int top_cy = cy - body_h / 2;       // top cap center
int bot_cy = cy + body_h / 2;       // bottom cap center
int top_y  = cy - h / 2;            // top edge
int bot_y  = cy + h / 2;            // bottom edge
```

### 8-style ring positions (shared by 3, 5, 6, 8, 9)
```c
int bar  = (size - 1) * 2 * UNIT;
int t_bc = cy - (ro - HALF_UNIT);   // top ring inner cap  (cy - 1.5u)
int t_tc = t_bc - bar;              // top ring outer cap
int b_tc = cy + (ro - HALF_UNIT);   // bottom ring inner cap (cy + 1.5u)
int b_bc = b_tc + bar;              // bottom ring outer cap
```

### Universal tail formula
```c
int tail = (size > 2) ? (size - 2) * UNIT : 0;
// sizes 1-2: 0u  size 3: 1u  size 4: 2u  size 5: 3u  size 6: 4u
```
Tail bars anchor to arc EDGES (cap_center ± ro), not cap centers.

### Digit constructions (v3.6 — in progress, some still being refined)
| Digit | Construction |
|-------|-------------|
| 0 | top/bottom caps + left/right VBARs |
| 1 | base HBAR + centered stem + diagonal stroke (left-clipped) |
| 2 | top semi + symmetric tail VBARs + diagonal + base HBAR |
| 3 | two mirrored right-C bumps with tails (in progress) |
| 4 | left short VBAR + right full VBAR + horizontal crossbar |
| 5 | top HBAR + left full stroke + right tail + bottom ring |
| 6 | top semi + right arm (upward, tail) + left full + bottom ring |
| 7 | top HBAR + scanline diagonal (full height) |
| 8 | two stacked 0-rings sharing inner caps, VBARs per half |
| 9 | full top ring + right full stroke + left tail + bottom semi |

### draw_diagonal function
```c
// Scanline diagonal: 1px-high rows, sw wide. Smooth solid stroke.
static void draw_diagonal(GContext *ctx, int x_start, int y_start,
                           int x_end, int y_end, int sw) {
  int dh = y_end - y_start;
  int dx = x_end - x_start;
  if (dh <= 0) return;
  for (int row = 0; row <= dh; row++) {
    int x = x_start + (row * dx / dh);
    graphics_fill_rect(ctx, GRect(x, y_start + row, sw, 1), 0, GCornerNone);
  }
}
```

---

## 7. RASTER DIGIT SYSTEM

- 10 digits × 6 sizes = 60 bitmaps per platform
- Emery: `TALLBOY_01` … `TALLBOY_96` (high-res)
- Low-res (flint etc): `TALLBOY_L01` … `TALLBOY_L96`
- Colons: `TALLBOY_COLON1`…`TALLBOY_COLON6` / `TALLBOY_LCOLON1`…`TALLBOY_LCOLON6`
- Bitmaps loaded on demand, freed aggressively to avoid OOM
- `free_digit_bitmaps(digit)` called between countdown digits
- Slot layout: H_TENS, H_ONES, COLON, M_TENS, M_ONES

---

## 8. INFO LINES (current — basic)

Current implementation has minimal info lines in stacked/wide_comps layouts.
Full info line system planned (see Section 11 — Future Vision).

### Current fields rendered
- Day name (short): "MON"
- Date: "MAY 3"
- Steps (health platforms)
- Distance (health platforms)
- Battery %

---

## 9. INFO LINE FIELDS — PLANNED (from Radium 2 field system)

Adopt Radium 2's full field ID system for TallBoy:

```c
#define FIELD_NONE        0
#define FIELD_DAY_LONG    1   // "SATURDAY"
#define FIELD_DATE        2   // "MAR 21"
#define FIELD_DAY_DATE    3   // "SAT MAR 21"
#define FIELD_STEPS       4   // step count
#define FIELD_TEMP_F      5   // weather °F
#define FIELD_TEMP_C      6   // weather °C
#define FIELD_BATTERY     7   // battery %
#define FIELD_DISTANCE    8   // mi or km
#define FIELD_CALORIES    9   // active kcal
#define FIELD_BT          10  // BT disconnect indicator
#define FIELD_HEART_RATE  11  // BPM (emery/diorite/flint only)
#define FIELD_SUNRISE     12  // "6:23am"
#define FIELD_SUNSET      13  // "7:41pm"
#define FIELD_DAYLIGHT    14  // "13h18m"
#define FIELD_STEPS_AVG   15  // avg steps at this time of day (historical)
#define FIELD_STEPS_DELTA 16  // +/- vs average ("ahead" / "behind")
#define FIELD_STEPS_PROJ  17  // projected daily total at current pace
```

`FIELD_STEPS_AVG/DELTA/PROJ` are TallBoy additions beyond Radium 2's set — see Section 12.

---

## 10. ICONS (to be ported from Radium 2)

Radium 2 draws all icons in C at two sizes (11px small / 14px large).
TallBoy will adopt the same icon functions:

| Icon | Function | Used for |
|------|----------|----------|
| Footprint pair | `draw_steps_icon` | Steps + distance |
| Battery | `draw_battery_icon` | Battery % + charging bolt |
| BT rune | `draw_bt_icon` | BT disconnect (hidden when connected) |
| Heart | `draw_heart_icon` | Heart rate |
| Flame | `draw_calories_icon` | Active calories |
| Sun | `draw_sun_icon` | Solar fields |
| Weather | `draw_weather_icon` | Dispatches by weather code |

**BT icon note:** Exclamation dot must use explicit pixel draws, NOT fill_rect — causes diagonal artifact on e-paper.

---

## 11. FUTURE VISION — FINAL FORM

### Idle state (primary)
Wide layout only: tall digits filling screen, nothing else.
Color of background/digits provides **ambient step pace indicator** — no numbers, no labels.
Pure sculptural time. See Design Tenet below.

### Shake interaction
- Time squishes (already implemented)
- Configured info lines slide in from top and bottom
- Same mechanic as Radium 2's overlay — holds for ~1 minute then auto-dismisses
- Second shake dismisses early

### Info lines (shake mode, wide layout only)
- User-configurable, 2–4 lines max
- Stacked layout: info column always visible (time already small)
- No shake-to-reveal needed for stacked mode

### Color ambient layer — Step Pace Spectrum
Background (or digit) color shifts along a user-selected palette based on step pace vs. historical average.

**Two comparison modes:**
1. **Cumulative daily pace** — today's total vs. historical average total at same time of day. Smooth, motivational. Best for persistent displays and Radium's arc dot.
2. **Hourly pace** — current hour's steps vs. historical average for this specific hour. Gamified, resets each hour. Color can snap dramatically at :00 — fun visual clock chime.

**Palette options (named schemes):**
- *Thermal* — blue → teal → green → yellow → red
- *Forest* — earth/brown → green (behind → thriving)
- *Ocean* — deep blue → cyan → white
- *Monochrome* — dim gray → bright white (works on B&W)

Setting to toggle between daily and hourly comparison mode.

**Neutral zone:** "On pace" = center of spectrum. Color only deviates visibly when meaningfully off-track. Prevents constant flickering from normal variance.

### Radium 2 — Step pace dot
On the existing step arc: a small marker dot showing "expected steps by now" based on historical average. Actual arc fill vs. dot position tells the story at a glance. Zero extra real estate, fits existing visual language, works on B&W (notch or gap).

---

## 12. STEP PACE DATA SYSTEM (planned)

Uses `health_service_get_minute_history()` — real per-minute granularity, 7-day lookback.

```c
// Returns average steps accumulated to current minute-of-day, over past 7 days.
// Uses real per-minute historical data — no linear interpolation needed
// except as fallback when history is sparse (day 1, or off-wrist gaps).
int prv_get_average_steps_to_now(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int elapsed_minutes = t->tm_hour * 60 + t->tm_min;

  int total = 0, day_count = 0;
  for (int day = 1; day <= 7; day++) {
    time_t day_start = time_start_of_today() - (time_t)day * SECONDS_PER_DAY;
    HealthMinuteData buf[elapsed_minutes]; // stack: max ~1440 × 3B = ~4KB
    uint32_t n = health_service_get_minute_history(buf, elapsed_minutes, &day_start, NULL);
    int day_steps = 0;
    for (uint32_t i = 0; i < n; i++)
      if (!buf[i].is_invalid) day_steps += buf[i].steps;
    if (day_steps > 0) { total += day_steps; day_count++; }
  }
  return day_count > 0 ? total / day_count : -1; // -1 = no history yet
}
```

**Notes:**
- Recalculate once per minute in tick handler — ~10K int additions, negligible cost
- `#ifdef PBL_HEALTH` guard required
- 1-minute grace period at hour boundary before showing hourly comparison color
- Fallback to `(hour_avg / 60) * minutes_elapsed` if history is sparse
- Memory: `HealthMinuteData` = 3 bytes each. Full day = ~4.3KB. Keep window to `elapsed_minutes` only.

---

## 13. WEATHER + SOLAR (planned)

Adopt Radium 2's weather/solar system:
- JS fetches from Open-Meteo: temp, weather code, sunrise, sunset, sunrise_tomorrow
- Solar timestamps cached to persistent storage (`SOLAR_KEY`)
- Stale data handled by `while (now > sunrise_tomorrow)` roll-forward loop
- Info lines: `FIELD_SUNRISE`, `FIELD_SUNSET`, `FIELD_DAYLIGHT`

---

## 14. DESIGN TENET — AMBIENT INFORMATION

*Watchfaces display data as atmosphere, not content. Artfully vague > precisely readable.*

A color shift, arc fill, or dot position communicates mood and direction in a 2-second glance — no numbers required. The slight cognitive friction of "why is it blue today?" creates more engagement than a stat that gets ignored. Every face should do one or two things that reward attention without demanding it.

**The watch is the barometer. The phone is the app.**

Info lines are the opposite mode — pure data, deliberate readout. Both are valid. The craft is knowing when to use each, and combining them for a multi-modal picture when appropriate.

---

## 15. CLOUDPEBBLE / BUILD RULES

1. Remove `resources/media` block from appinfo.json
2. Menu icons via CloudPebble UI only (not via appinfo)
3. No tilde (`~`) in resource filenames
4. Alloy SDK (JS/TS, Moddable XS, ES2025) available for new development
5. Always give full files for copy-paste — never partial diffs
6. `src/main.c` flat path (not `src/c/main.c`)

---

## 16. GITHUB MCP NOTES

- `create_or_update_file` for all pushes — requires current file SHA
- `push_files` tool sends empty content — do NOT use
- TallBoy repo uses `main` branch
- `get_file_contents` times out on files above ~15KB

---

## 17. CURRENT STATUS / NEXT STEPS

**Working:** 0, 4, 8 vector digits. Animation system. All 5 layouts. Raster digit system.

**In progress (v3.6):** Vector digits 1, 2, 3, 5, 6, 7, 9 — geometry largely correct, minor anchor/tail tuning ongoing. v3.4 was a regression; v3.6 is recovering.

**Next code tasks:**
1. Finish vector digit tuning (1, 2, 3, 5, 6, 9 — 7 acceptable)
2. Port full info line field system from Radium 2
3. Port icon drawing functions from Radium 2
4. Implement shake-to-reveal info lines in wide mode (auto-dismiss ~1min)
5. Weather/solar JS fetch (port from Radium 2 index.js)
6. Step pace history calculation + ambient color layer
7. Config page

---

*End of context seed.*
