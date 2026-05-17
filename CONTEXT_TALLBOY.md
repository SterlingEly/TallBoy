# TALLBOY — CONTEXT SEED FOR NEW THREAD
*Everything a fresh Claude session needs to resume TallBoy development*

---

## 1. WHAT IS THIS PROJECT?

**TallBoy** is a Pebble watchface built around oversized vector-drawn digits that scale dynamically — "tall boys" that stretch and squish as an animation mechanic and fill the screen with pure time. Design by Sterling Ely (2026), implemented by Sterling Ely + Claude.

**Sterling:** Design/concept lead, CloudPebble builds, device testing (Pebble Time 2 / emery).
**Claude:** Code, GitHub commits, documentation.

---

## 2. REPO & BUILD

```
SterlingEly/TallBoy (branch: main)
└── src/main.c       ← entire watchface, currently v3.30a
```

**CloudPebble build rule:** The file Claude commits to GitHub is `src/main.c`. CloudPebble builds its own internal copy — after Claude pushes, Sterling manually syncs or edits in CloudPebble. Never use `push_files` (sends empty content). Always use `create_or_update_file` with current SHA.

---

## 3. PLATFORM TARGETS

| Platform | Watch | Screen | UNIT | Notes |
|----------|-------|--------|------|-------|
| emery | Pebble Time 2 | 200×228 rect | 8px | Color, HR, touch — **primary dev device** |
| flint | Pebble 2 | 144×168 rect | 6px | B&W, HR |
| basalt | Pebble Time | 144×168 rect | 6px | Color |
| diorite | Pebble 2 SE | 144×168 rect | 6px | B&W, HR |
| aplite | Pebble Classic | 144×168 rect | 6px | B&W |

Emery = 25u wide (200px / 8px). All others = 24u (144px / 6px).

---

## 4. DIGIT GEOMETRY — THE UNIT SYSTEM

### Horizontal
Every digit is **4u wide** (GLYPH_W) within a **5u slot** (SLOT_W = 5u). Half-unit padding each side creates 1u visual gap between digits.

```
[margin] [½u|4u digit|½u] ×4 + [½u|1u colon|½u] + [margin] = 23u content
Emery: 25u total → ~1u margin each side
Flint: 24u total → ½u margin each side
```

### Vertical — the h-based system (v3.27+)
Digits are parameterized by **pixel height `h`** directly — no discrete size index.

```c
// Immutable — NEVER change with h:
const int ro = UNIT * 2;   // arc outer radius
const int ri = UNIT * 1;   // arc inner radius  
const int sw = UNIT;       // stroke width

// Derived from h:
int bar  = (h - 7*UNIT) / 2;         // ring gap between inner cap centers
int tail = (h > H_MIN) ? (h - H_MIN) / 4 : 0;  // tail stub extension

// H_MIN = 11*UNIT = 88px (emery) / 66px (flint) — minimum legible
// At H_MIN: bar=0, tail=0, arcs just touching
```

**Formulas verified:** At h=196px (emery resting): bar=70px, tail=27px. At h=88px (min): bar=0, tail=0. ✓

### Digit layer taxonomy
1. **Arc cap** (top, fixed size): arc cap (0,2,3,6,8,9), hbar cap (5,7), diagonal cap (1), none (4)
2. **Spans** (resize with h): full span (body), bridge span (`bar`), diagonal span (2,7), tail span (`tail`)
3. **Center elements** (anchored to cy, never move): crossbar (4), ring pair (8), center arc (6,9)
4. **Arc base** (bottom, fixed size): arc base (0,3,5,6,8,9), flat base (2), none (1,4,7)

---

## 5. MARGIN MODEL

```
Full-screen vector:  target_h = ub_h - 4*UNIT  (2u top + 2u bottom)
Stacked layout:      row_h = (ub_h - 5*UNIT) / 2  (2u top + 1u gap + 2u bottom)
Quick look:          same margins maintained as ub_h shrinks (unobstructed_change fires)
H_MIN = 11*UNIT      floor for all layouts
ANIM_OVERSHOOT = UNIT  1u above target for snap effect
```

On emery unobstructed (228px): `target_h = 228 - 32 = 196px`. Stacked row: `(228-40)/2 = 94px`.

---

## 6. ANIMATION SYSTEM

```c
ANIM_STEP_PX = 8     // pixels per timer step
ANIM_STEP_MS = 16    // ~60fps cadence
ANIM_SNAP_MS = 120   // hold at overshoot peak before snap back
ANIM_OVERSHOOT = UNIT  // 1u above target

COUNTDOWN_HOLD_MS = 400  // pause at full height between countdown digits
BLINK_REPS = 2
```

### Sizing variables (no discrete size index — all pixel heights)
```c
s_h        // current animated digit height (pixels)
s_target_h // resting height = prv_compute_target_h(ub_h)
H_MIN      // = 11*UNIT, squish floor
```

### Animation phases
- **PHASE_COUNTDOWN:** 9→0 wink at startup; all-vector, smooth pixel steps
- **PHASE_BLINK:** 2× squish blinks after countdown before settling
- **PHASE_SQUISH:** minute-change animation; swaps time digits at H_MIN
- **PHASE_SHAKE_CYCLE:** accel-tap; SHAKE_OFFSETS_PX relative to s_target_h, 2 reps
- All expand phases overshoot to `s_target_h + ANIM_OVERSHOOT`, snap back after ANIM_SNAP_MS

---

## 7. LAYOUT SYSTEM

Current layouts (shake to cycle):
| # | Name | Description |
|---|------|-------------|
| 0 | `LAYOUT_VECTOR` | Full-screen vector digits, squish on minute change |
| 1 | `LAYOUT_RIGHT` | Stacked vector right + info column left |

**Planned v3.31 layout cycle (5 layouts):**
| Shake | Layout | Description |
|-------|--------|-------------|
| 0 | Full time | Large digits, no info |
| 1 | Info 1+1 | Time shrunk + 1 info line above + 1 below |
| 2 | Info 2+2 | Time shrunk more + 2 above + 2 below |
| 3 | Stacked right | Stacked digits right + info left |
| 4 | Stacked left | Stacked digits left + info right |

---

## 8. STEP PACE BACKGROUND COLOR (emery / PBL_COLOR only)

Compares today's steps to `s_steps_expected` (expected cumulative at current minute, from 7-day minute-level watch history — fully offline).

| % of expected | Color | fg |
|---------------|-------|----|
| 0 / no data | Black | White |
| 1–30% | Red | White |
| 31–60% | Orange | White |
| 61–90% | Yellow | **Black** |
| 91–200% | Green | White |
| 201–400% | Blue | White |
| 401–700% | Indigo | White |
| 701–1000% | VividViolet | White |
| 1001%+ | Black | White (easter egg) |

Note: `GColorViolet` does NOT exist in Pebble SDK. Use `GColorVividViolet`.

---

## 9. HEALTH DATA (v3.30+)

All health reads happen **once per minute** inside `tick_handler` via `prv_update_health()`. No `health_service_events_subscribe` — removed entirely. All reads are synchronous from on-device cache.

```c
s_steps          // HealthMetricStepCount (sum today)
s_distance_m     // HealthMetricWalkedDistanceMeters (sum today)
s_calories       // HealthMetricActiveKCalories (sum today)
s_heart_rate     // HealthMetricHeartRateBPM (peek current)
s_steps_expected // prv_calc_steps_expected() — 7-day minute-level avg at this time of day
```

`prv_calc_steps_expected()`: fetches up to 120 minutes of history from each of 7 prior days, sums steps, averages across days. Window capped at 120min; scales up proportionally if elapsed > window. Returns -1 if < 2 minutes elapsed.

---

## 10. INFO LINES (stacked layout, up to 8)

Built by `build_info_lines()`, order:
1. Full day name (Sunday, Monday…)
2. Date (MAY 16)
3. Step count (1,234 steps)
4. Step pace % (87% pace) — only if s_steps_expected > 0
5. Distance (mi or km) — only if > 0
6. Calories (cal) — only if > 0
7. Heart rate (bpm) or "no phone" if BT disconnected
8. Battery (bat 84% or bat 84% +)

---

## 11. PIXEL VERIFICATION STATUS

- ✅ Emery vector digits pixel-verified at all sizes (Photoshop)
- ✅ Low-res (flint) vector digits verified (subpixel anomalies acceptable)
- ✅ Raster system fully retired (v3.25) — all bitmap code removed
- ✅ Dynamic vertical sizing (v3.27+) — h in pixels, no discrete size index
- ✅ Margins: 2u top/bottom full-screen, 2u+1u+2u stacked (v3.28)

---

## 12. PENDING FEATURES

### Next: v3.31 — multi-layout shake cycle
Time shrinks dynamically as info lines are added above/below. Needs `draw_info_overlay()` system. Shake cycles through 5 layouts (see §7).

### Soon
- Weather/solar JS fetch (port from Radium2 `src/pkjs/`)
- Config page (Clay vs custom HTML — decide when closer)
- Hourly step pace comparison mode

---

## 13. DESIGN TENET

*Watchfaces display data as atmosphere, not content. Artfully vague > precisely readable.*

Color = mood. Position = direction. The watch is the barometer; the phone is the app.

---

## 14. RADIUM2 REFERENCE

Repo: `SterlingEly/Radium2` (branch: master)
- `src/c/main.c` — field system reference, icon drawing, weather/solar logic
- `src/pkjs/` — weather/solar JS fetch to port

Field IDs from Radium2 (for future config system):
```
FIELD_NONE=0, FIELD_DAY_LONG=1, FIELD_DATE=2, FIELD_DAY_DATE=3,
FIELD_STEPS=4, FIELD_TEMP_F=5, FIELD_TEMP_C=6, FIELD_BATTERY=7,
FIELD_DISTANCE=8, FIELD_CALORIES=9, FIELD_BT=10, FIELD_HEART_RATE=11,
FIELD_SUNRISE=12, FIELD_SUNSET=13, FIELD_DAYLIGHT=14
```

---

*End of context seed. Last updated: v3.30a, May 2026.*
