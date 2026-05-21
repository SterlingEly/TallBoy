# TALLBOY — CONTEXT SEED FOR NEW THREAD
*Everything a fresh Claude session needs to resume TallBoy development.*  
*Last updated: v3.40, May 2026.*

---

## 1. WHAT IS THIS PROJECT?

**TallBoy** is a Pebble watchface built around oversized vector-drawn digits that scale dynamically — "tall boys" that stretch to fill the screen and squish as an animation mechanic on every minute change. Design by Sterling Ely (2026), implemented by Sterling Ely + Claude.

**Sterling:** Design/concept lead, CloudPebble builds, device testing.  
**Claude:** Code, GitHub commits, documentation.

---

## 2. REPO & BUILD

```
SterlingEly/TallBoy (branch: main)
└── src/main.c        ← entire watchface, currently v3.40
└── appinfo.json      ← app metadata and message keys
└── README.md         ← user-facing documentation
└── CONTEXT_TALLBOY.md ← this file
```

**CloudPebble build rules:**
- File Claude commits to GitHub is `src/main.c` (flat path)
- CloudPebble builds its own internal copy — Sterling manually syncs after Claude pushes
- **Always use `create_or_update_file` with current SHA** — never `push_files` (sends empty content)
- Full files only — never partial diffs

**Dead files to delete (still in repo, causing `hstroke` warning on every build):**
- `src/digit.c` — old bitmap/raster digit library, fully superseded
- `src/digit.h` — header for above
- `src/case8_patch.txt` — old patch notes
- `src/main_digit1_fix.txt` — old patch notes

---

## 3. PLATFORM TARGETS

| Platform | Watch | Screen | UNIT | Notes |
|----------|-------|--------|------|-------|
| emery | Pebble Time 2 | 200×228 | 8px | Color, HR, touch, smartstrap — **primary dev device** |
| flint | Pebble 2 | 144×168 | 6px | B&W, HR |
| basalt | Pebble Time | 144×168 | 6px | Color |
| diorite | Pebble 2 SE | 144×168 | 6px | B&W, HR |
| aplite | Pebble Classic | 144×168 | 6px | B&W |

---

## 4. UNIT SYSTEM & DIGIT GEOMETRY

**UNIT = 8px (emery) / 6px (flint).** All layout is expressed in multiples of UNIT.

Each digit occupies a **5u slot** (SLOT_W). Content is **4u wide** (GLYPH_W = UNIT×4) with HALF_UNIT (4px) padding each side.

### Fixed constants (NEVER change with height `h`):
```c
const int ro = UNIT * 2;   // arc outer radius (16px emery)
const int ri = UNIT * 1;   // arc inner radius (8px emery)
const int sw = UNIT;       // stroke width (8px emery)
```

### Derived from `h`:
```c
int bar  = (h - 7 * UNIT) / 2;         // gap between inner cap centers (0 at H_MIN)
int tail = (h > H_MIN) ? (h - H_MIN) / 4 : 0;  // tail stub length
```

**H_MIN = 11u = 88px (emery) / 66px (flint)** — minimum legible height. At H_MIN: bar=0, tail=0, arcs just touching.

**Digits 1, 2, 7** use `gpath_create` for diagonal strokes. These allocate/free heap every frame. NULL checks are in place. Not a leak — destroy is always called.

---

## 5. MARGIN MODEL

```
MARGIN_CANVAS = 1u  — screen edge → bg rounded-rect boundary
MARGIN_DIGIT  = 1u  — canvas boundary → digit/content area
MARGIN_OUTER  = 2u  — MARGIN_CANVAS + MARGIN_DIGIT (convenience alias)
```

**Quick-look detection:**
```c
#define QUICK_LOOK_ACTIVE(ub_h)  ((ub_h) < (SCREEN_H - UNIT))
#define BOTTOM_MARGIN(ub_h)      (QUICK_LOOK_ACTIVE(ub_h) ? MARGIN_DIGIT : MARGIN_OUTER)
```
When the quick-look bar is active, `ub_h` is reduced from the bottom. `BOTTOM_MARGIN` returns `MARGIN_DIGIT` (1u) so digits sit 1u from the bar, `MARGIN_OUTER` (2u) otherwise.

---

## 6. LAYOUT SYSTEM

```c
#define LAYOUT_FULL      0  // full-screen time, squish on minute change
#define LAYOUT_INFO_1    1  // 1 info line above + 1 below, time fills middle
#define LAYOUT_INFO_2    2  // 2 above + 2 below
#define LAYOUT_INFO_3    3  // 3 above + 3 below
#define LAYOUT_STACK_R   4  // stacked digits right, info column left
#define LAYOUT_STACK_L   5  // stacked digits left, info column right
#define LAYOUT_COUNT     6
```

**Shake (accel tap):** cycles forward through layouts. Returns to FULL with shake animation.  
**Touch (emery, PBL_TOUCH):** cycles bg corner radius — `{2u, 3u, 4u}`.

### Wide layout geometry (INFO_1/2/3):
- Above block: first glyph at `ub_top + UNIT` (1u from top), lines step down by `INFO_LINE_STEP`
- Below block: last glyph bottom at `ub_bot - BOTTOM_MARGIN`, lines step up by `INFO_LINE_STEP`
- Time: fills between the two blocks with 1u gap each side; height auto-calculated, clamped to H_MIN

### Stacked layout geometry:
```c
dh    = (ub_h - 2*MARGIN_OUTER - UNIT) / 2   // digit height (both rows equal)
h_cy  = ub_top + MARGIN_OUTER + dh/2          // top digit center
m_cy  = ub_bot - BOTTOM_MARGIN(ub_h) - dh/2  // bottom digit center
```

---

## 7. ANIMATION SYSTEM

```c
#define ANIM_STEP_PX    8    // px per timer tick
#define ANIM_STEP_MS   16    // ~60fps
#define ANIM_SNAP_MS  120    // hold at overshoot peak before snap
#define ANIM_OVERSHOOT UNIT  // 1u above target
#define COUNTDOWN_HOLD_MS 150  // pause between countdown digits
#define BLINK_REPS        2    // squish blinks after countdown
#define SHAKE_LEN         9    // steps in shake pulse wave
```

### Phases:
- `PHASE_COUNTDOWN`: 9→0 launch sequence; digits squish and re-expand per digit
- `PHASE_BLINK`: 2× squish blinks after countdown completes
- `PHASE_SQUISH`: minute-change animation; swaps time at H_MIN; used for layout transitions too
- `PHASE_SHAKE_CYCLE`: 9-point pulse wave `{0,-1u,-2u,-3u,-2u,-1u,0,+1u,0}` × 2 reps
- `PHASE_DONE`: idle, waiting for events

All expand phases overshoot to `s_target_h + ANIM_OVERSHOOT`, hold `ANIM_SNAP_MS`, snap back.

Time digits swap at `H_MIN` during `PHASE_SQUISH`. If a tick arrives during squish, time is queued in `s_pending_hour`/`s_pending_minute` and applied at H_MIN.

---

## 8. STEP PACE BACKGROUND COLOR

Emery + PBL_COLOR + PBL_HEALTH only. Compares today's cumulative steps to `s_steps_expected` (7-day minute-level average at current time of day, fully offline from watch health history).

| % of expected | BG Color | FG Color |
|---------------|----------|----------|
| 0 / no data | Black | White |
| 1–30% | Red | White |
| 31–60% | Orange | White |
| 61–90% | Yellow | **Black** |
| 91–200% | Green | **Black** |
| 201–400% | Blue | White |
| 401–700% | Indigo | White |
| 701–1000% | VividViolet | White |
| 1001%+ | Black | White (easter egg) |

Note: Use `GColorVividViolet` — `GColorViolet` does not exist in the Pebble SDK.

---

## 9. HEALTH DATA

All health reads happen **once per minute** inside `tick_handler` via `prv_update_health()`. No `health_service_events_subscribe` — removed. All reads are synchronous from on-device cache.

```c
s_steps          // HealthMetricStepCount (sum today)
s_distance_m     // HealthMetricWalkedDistanceMeters (sum today)
s_calories       // HealthMetricActiveKCalories (sum today)
s_heart_rate     // HealthMetricHeartRateBPM (peek current)
s_steps_expected // prv_calc_steps_expected() — 7-day min-level avg at this time
```

`prv_calc_steps_expected()`: fetches up to 120 minutes of history from each of 7 prior days, sums steps, averages across days with data. Window capped at 120min; scales proportionally if elapsed > window. Returns -1 if < 2 minutes elapsed today.

Health minute buffer: `s_minute_buf[STEPS_AVG_MAX_MIN]` = 120 × `HealthMinuteData` = 480 bytes, statically allocated.

---

## 10. INFO LINES

### Wide layout (build_info_lines_wide) — up to 6 lines:
1. Day & date: `"Thursday, May 21"`
2. Weather: `"72 F & sunny"` (fallback if no data)
3. Sunrise · sunset: `"6:02am · 8:14pm"` (fallback if no data)
4. Steps · distance: `"3,500 steps · 2.1 mi"`
5. Expected · %: `"exp 3,500 · 87%"`
6. HR · cal: `"72 bpm · 212 cal"` (fallback `"-- bpm · -- cal"` if unavailable)

All slots always rendered with fallback strings for layout debugging.

### Stacked layout (build_info_lines_stacked) — up to 8 lines:
1. Weather temp
2. Day name
3. Date
4. Steps (short format)
5. Expected steps
6. Pace %
7. +/- steps vs expected (or "on pace")
8. Distance or battery

Battery always fills last available slot in both layouts.

---

## 11. FONT METRICS (emery, Gothic 24 Bold — measured on device)

```
INFO_LINE_H     = 28px  full font cell height
INFO_TOP_PAD    = 10px  dead space above glyph top inside cell
INFO_GLYPH_H    = 18px  cap-top to descender-bottom (full glyph space)
INFO_DESCENDER  =  4px  descenders below baseline (g, y, comma)
INFO_CAP_OFFSET =  3px  caps above x-height (approximately)
INFO_BOT_PAD    =  0px  glyph touches bottom of cell
INFO_LINE_STEP  = 26px  rect-top to rect-top spacing for 1u gap between glyphs
                         = INFO_GLYPH_H + UNIT = 18 + 8
INFO_BOT_ADJUST =  2px  half-descender — documented, not yet applied
```

All text uses `draw_text_at_glyph_y(ctx, text, x, glyph_y, width)` which shifts the rect up by `INFO_TOP_PAD` so the visual glyph appears at `glyph_y`.

---

## 12. TOUCH API (PBL_TOUCH, emery + gabbro only)

Confirmed from `pebble.h` line 913:
```c
void touch_service_subscribe(TouchServiceHandler handler, void *context);
typedef void (*TouchServiceHandler)(const TouchEvent *, void *);
```

Current use: any touch → cycle `s_radius_idx` → redraw with new corner radius.

**TODO:** Filter to touch-down only once event type enum is confirmed (compilation showed `TOUCH_EVENT_TYPE_DOWN` is wrong name). Also want `event->touch.x` / `event->touch.y` for per-digit squish easter egg — slot x positions are: H_TENS=12, H_ONES=52, COLON=80, M_TENS=108, M_ONES=148 (emery).

---

## 13. WEATHER / SOLAR DATA

Received via `app_message` inbox from JS pebble-js-app (not yet written — see Radium2 reference).

```c
MESSAGE_KEY_WeatherTempF  // integer °F
MESSAGE_KEY_WeatherTempC  // integer °C (fallback)
MESSAGE_KEY_WeatherCode   // 0=clear, 1-3=cloudy, 4-49=fog, 50-69=rain, 70-79=snow, 80-99=storm
// Stub (not yet handled):
// MESSAGE_KEY_SunriseMin   // minutes since midnight
// MESSAGE_KEY_SunsetMin    // minutes since midnight
```

Inbox size: 256 bytes (sufficient). Outbox: 64 bytes (nothing sent currently).

---

## 14. PENDING FEATURES

### Near-term
- Weather/solar JS fetch — port from `SterlingEly/Radium2` `src/pkjs/`
- `MESSAGE_KEY_SunriseMin` / `MESSAGE_KEY_SunsetMin` inbox handling (stub in `inbox_received`)
- Touch easter egg: per-digit squish using x/y hit testing
- Delete dead files: `src/digit.c`, `src/digit.h`, `.txt` patch files
- Prune unused legacy images from `resources/`

### Design TBD
- Config page (Clay vs custom HTML — prefer custom given Radium2 precedent)
- `INFO_BOT_ADJUST = 2px` bottom margin tweak (documented, not applied)
- Orange foreground: decide if luminance requires dark fg (borderline)
- Corner radius final choice (currently touch-cycled for evaluation)
- Canvas outline / border: likely removed in final; `draw_border()` deleted in v3.38b
- Light vs dark color spectrum variant

---

## 15. DESIGN TENET

*Watchfaces display data as atmosphere, not content. Artfully vague > precisely readable.*

Color = mood. Position = direction. The watch is the barometer; the phone is the app.

---

## 16. RADIUM2 REFERENCE

Repo: `SterlingEly/Radium2` (branch: master)
- `src/c/main.c` — field system reference, weather/solar parsing
- `src/pkjs/` — weather/solar JS fetch to port to TallBoy
