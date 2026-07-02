# TallBoy — Project Context

> **START HERE.** Read this file completely before making any code changes.
> This is the authoritative handoff document for AI-assisted development sessions.

---

## Purpose

TallBoy is a Pebble watchface built around oversized vector-drawn digits that dynamically fill the screen height. The digits "squish" and expand as an animation mechanic on every minute change. Designed and implemented by Sterling Ely with AI (Claude) as implementation partner.

**Design tenet:** *Watchfaces display data as atmosphere, not content. Artfully vague > precisely readable. Color = mood. Position = direction. The watch is the barometer; the phone is the app.*

---

## Current Status

**Version: v3.61** (HEAD as of late June 2026)

What's working:
- Builds clean on all 7 target platforms
- Full-mode animated digits with anticipate → squish → expand on every minute tick
- Wide info mode: up to 3 info slots above + 3 below, digits fill remaining space; empty slots collapse instantly (no reserved space)
- Stacked info mode (STACK_L / STACK_R): stacked hour/minute digits one side, up to 8 info lines other side; minute-change fast-blink animation
- Pace-based background color spectrum (color + health platforms)
- Weather, UV, solar time fetch via PebbleKit JS (Open-Meteo API, no key required)
- All 28 info slot types working including sleep, calories (active + resting), typical steps
- Round platform support (chalk + gabbro): per-column digit heights follow circle geometry — outer columns 108px, inner 144px; cy centered at unobstructed midpoint
- Background: black base → colored rounded-rect (UNIT*2 radius = 16px on emery); plain fill on round (OS clips to circle)
- Wide mode blink: BLINK_STEP=8 (was 6 — ~25% faster)

---

## Human / AI Role Split

| Responsibility | Owner |
|---|---|
| Design direction, visual decisions | Sterling |
| CloudPebble builds, device testing | Sterling |
| Code writing, GitHub commits | AI (Claude) |
| Documentation | AI (Claude), reviewed by Sterling |
| Product decisions (slot content, animation feel) | Sterling |
| Slot text formatting, geometry math | AI (Claude) |

---

## Repository Structure

```
SterlingEly/TallBoy  (branch: main)
├── src/
│   ├── main.c          ← entire watchface (~88KB); single-file architecture
│   └── pkjs/
│       └── index.js    ← PebbleKit JS: weather/UV/solar fetch + config page
├── appinfo.json        ← app metadata, message keys, target platforms
├── PROJECT_CONTEXT.md  ← this file (handoff doc for AI sessions)
├── README.md           ← user-facing overview; points here for dev context
└── wscript             ← Waf build script (do not edit)
```

**Dead files still in repo (safe to delete when convenient):**
- `src/digit.c`, `src/digit.h` — old raster digit library, fully superseded by vector drawing
- `src/case8_patch.txt`, `src/main_digit1_fix.txt` — old patch notes

---

## Build / Deployment Rules

### GitHub (CRITICAL)
- **ALWAYS use `create_or_update_file` with the current file SHA** — NEVER `push_files` (confirmed bug: push_files sends empty content, silently zeroing the file)
- Always commit **full file content** — never partial diffs or patches
- **Always fetch the current SHA** before any update — prior session SHAs are stale
- `get_file_contents` times out on files above ~15KB; for main.c (~88KB), use `create_or_update_file` directly and retry on timeout

### CloudPebble
- Sterling manually syncs from GitHub after Claude pushes (re-import or manual paste)
- `src/main.c` must be at the **flat path** (`src/main.c`) — CloudPebble does not support `src/c/main.c`
- Menu icons: add via CloudPebble UI directly, not via GitHub
- Do not put a `resources/media` block in appinfo.json — causes "Unsupported published resource type" errors
- Tilde (`~`) in resource filenames breaks CloudPebble import

### SDK
- SDK version 3, targeting Pebble OS 3.x
- Primary docs: developer.repebble.com (Pebble is officially back — use these over Rebble docs)

---

## Architecture

### Platform Constants

| Platform | Watch | Screen | UNIT | HALF_UNIT | Notes |
|---|---|---|---|---|---|
| emery | Pebble Time 2 | 200×228 rect | 8 | 4 | Color, HR sensor, primary dev device |
| basalt | Pebble Time | 144×168 rect | 6 | 3 | Color |
| chalk | Pebble Time Round | 180×180 round | 6 | 3 | Color, round |
| diorite | Pebble 2 SE | 144×168 rect | 6 | 3 | B&W, HR sensor |
| flint | Pebble 2 | 144×168 rect | 6 | 3 | B&W |
| aplite | Pebble Classic | 144×168 rect | 6 | 3 | B&W |
| gabbro | Pebble 2 HR | 180×180 round | 6 | 3 | Color, round, HR sensor |

HR sensors: **only emery, diorite, and gabbro**. In store copy use "on supported models" — never list platforms explicitly.
Round platforms (chalk, gabbro): both use `PBL_ROUND` block. Info layouts not supported on round.
Touch: **only emery** — but non-functional at firmware level on shipping hardware. All PBL_TOUCH code removed.

### Digit Geometry (stable canon)

```
UNIT = 8 (emery) / 6 (others)
SLOT_W = UNIT * 5          — digit slot width (includes padding)
GLYPH_W = UNIT * 4         — actual drawn glyph width
HALF_SLOT_PAD = UNIT / 2   — padding each side of glyph

Arc caps (NEVER change with height h):
  ro = UNIT * 2             — outer arc radius
  ri = UNIT * 1             — inner arc radius
  sw = UNIT                 — stroke width

H_MIN = UNIT * 11           — minimum legible height (arcs just touching, bars gone)
H_ABSOLUTE_MIN = UNIT * 5   — animation floor for stacked blink

Derived from h:
  bar  = (h - 7*UNIT) / 2  — gap between inner cap centers (0 at H_MIN)
  tail = (h > H_MIN) ? (h - H_MIN)/4 : 0  — tail stub length for some digits
```

Digits 1, 2, 7 use `gpath_create` for diagonal strokes. These allocate/free on every draw call. Null checks are in place; not a memory leak.

### Slot Positions (emery, UNIT=8, SLOT_W=40)

```
SLOT_H_TENS=12, SLOT_H_ONES=52, COLON_SLOT_X=80
SLOT_M_TENS=108, SLOT_M_ONES=148
```

### Slot Positions — round (chalk/gabbro, UNIT=6, SLOT_W=30, centered on 180px)

```
SLOT_H_TENS=15, SLOT_H_ONES=45, COLON_SLOT_X=75
SLOT_M_TENS=105, SLOT_M_ONES=135
Colon center = 90 = exact screen center ✓
ROUND_OUTER_H = UNIT*18 = 108px  (H_TENS, M_ONES; dx=60 from center)
ROUND_INNER_H = UNIT*24 = 144px  (H_ONES, M_TENS; dx=30 from center)
cy on round = ub_top + ub_h/2  (NOT MARGIN_OUTER + s_target_h/2 — off by 6px)
```

### Slot Positions — other rect (basalt/diorite/flint/aplite, UNIT=6, SLOT_W=30)

```
SLOT_H_TENS=6, SLOT_H_ONES=36, COLON_SLOT_X=57
SLOT_M_TENS=78, SLOT_M_ONES=108
```

### Margin Model

```
MARGIN_CANVAS = UNIT        — screen edge → bg rounded-rect edge
MARGIN_DIGIT  = UNIT        — bg edge → digit content
MARGIN_OUTER  = UNIT * 2    — convenience alias (MARGIN_CANVAS + MARGIN_DIGIT)

QUICK_LOOK_ACTIVE(ub_h) = (ub_h < SCREEN_H - UNIT)
BOTTOM_MARGIN(ub_h) = QUICK_LOOK_ACTIVE ? MARGIN_DIGIT : MARGIN_OUTER
```

### Layout System

```c
LAYOUT_FULL    = 0  // full-screen time; anticipate → squish → expand each minute
LAYOUT_INFO    = 1  // wide mode; up to 3 info slots above + 3 below time
LAYOUT_STACK_R = 2  // digits right, info column left (right-aligned text)
LAYOUT_STACK_L = 3  // digits left, info column right (left-aligned text)
```

Layout transitions: SPLIT_V then SPLIT_H (to stacked); MERGE_H then MERGE_V (to full).
Wide blink: fast linear drop/rise, no overshoot, single rep, digit swap at blink minimum.
Stacked blink: STACK_L = symmetric squish (Option A); STACK_R = fold toward center (Option B, hours top pinned, minutes bottom pinned).

### Animation Phases

```
PHASE_DONE → PHASE_ANTICIPATE → PHASE_SQUISH → PHASE_EXPAND → PHASE_DONE  (full mode)
PHASE_DONE → PHASE_BLINK (fast: down, swap digits at min, ease up) → PHASE_DONE  (wide/stacked)
PHASE_SPLIT_V → PHASE_SPLIT_H  (transition to stacked)
PHASE_MERGE_H → PHASE_MERGE_V  (transition back to full)
```

### Key State Variables

```c
static int s_h = 0;            // current digit height (animated)
static int s_target_h = 0;     // target height for current layout
static int s_h_min = 0;        // animation floor (H_MIN or H_ABSOLUTE_MIN)
static int s_stk_h_start = 0;  // height at start of stacked blink (Option B drift)
static int s_steps_expected = -1;    // typical steps at current time (same-day-of-week avg)
static int s_steps_typical_day = -1; // typical full-day total (same-day-of-week avg)
static int s_calories_resting = 0;   // BMR calories today
static int s_sleep_seconds = -1;     // sleep since 6pm yesterday; -1 = unavailable
static int s_uv_index = -1;          // -1 = never received; 0+ = valid
static time_t s_weather_ts = 0;      // unix time of last weather receipt; 0 = never
static int s_weather_temp_f = -999;  // sentinel for no data
```

### Data Caching Philosophy

**Hide > mislead.** If data is unavailable or stale, the slot returns `false` and the line disappears entirely. Never show zeros or stale defaults.
- UV index 0 is valid; use `-1` sentinel for "never received"
- Weather/UV ages out after 3 hours without a phone update (`WEATHER_MAX_AGE = 3*3600`)
- Sunrise/sunset never ages out (astronomical, valid all day)
- Heart rate shows `"--"` (sensor exists but no reading) — only exception to hide rule
- Wide mode: empty slots collapse immediately on next redraw after data arrives or expires

---

## Critical Constants / Message Keys

### appinfo.json appKeys

```
WeatherTempF=0, WeatherTempC=1, WeatherCode=2
SunriseTime=3, SunsetTime=4
CfgInfoMode=5, CfgInfoLayout=6, CfgTempUnit=7, CfgDistUnit=8, CfgClockFormat=9
CfgWide1..6 = 10..15
CfgStack1..8 = 16..23
CfgInvert=24, CfgColorMode=25
CfgColorBg=26, CfgColorDigH=27, CfgColorDigM=28, CfgColorShadow=29, CfgColorInfo=30
UvIndex=31, SunriseTomorrow=32
```

### Persist Keys

```
PERSIST_CFG_VERSION=0, PERSIST_CFG_FLAGS=1, PERSIST_CFG_WIDE=2
PERSIST_CFG_STACK=3, PERSIST_CFG_STACK_HI=4, PERSIST_CFG_COLORS=5 (and 6)
CFG_VERSION=5  ← bump this if persist layout changes
```

### Slot Type IDs (complete)

```
0=empty, 1=day, 2=date, 3=day+date, 4=temp, 5=weather, 6=steps, 7=distance
8=exp_steps (typ X), 9=pace, 10=calories, 11=heart_rate, 12=sunrise, 13=sunset
14=daylight, 15=battery, 16=bluetooth, 17=sunrise+sunset
18=steps+exp_steps (wide only), 19=exp_steps+pace (wide only), 20=battery+bluetooth (wide only)
21=debug, 22=UV, 23=light_remaining, 24=uv_light (wide only), 25=temp+UV (wide only)
26=typical_day (full-day step total), 27=calories_total (active+resting), 28=sleep
```

---

## Known Bugs / Known Traps

### Active bugs

None currently known. (As of v3.61.)

### Known traps (do not repeat)

- **`push_files` sends empty content** — always use `create_or_update_file` with the correct SHA
- **Degree symbol (0xB0) silently breaks weather text rendering** — use plain ASCII `"72 F"` not `"72°F"`
- **CloudPebble font resources** must be added via the UI "Another Font" button, not via appinfo.json
- **Stale SHA** — always fetch current SHA before any update; prior session SHAs go stale after any commit
- **`get_file_contents` times out on main.c** (~88KB) — use `create_or_update_file` directly
- **Gabbro is round** — never group gabbro with rectangular platforms. It uses `PBL_ROUND`, not `PBL_PLATFORM_EMERY`
- **Round cy must be `ub_top + ub_h / 2`** (unobstructed midpoint), not `MARGIN_OUTER + s_target_h/2` — the latter is 6px off-center on 180px screens
- **Wide mode target height** is computed from configured slot count at startup, then refined live per-frame using actual rendered count (`an`/`bn`). Don't use `s_target_h` as draw height in wide mode — use `actual_h`
- **Stacked blink** uses `H_ABSOLUTE_MIN` (not `H_MIN`) as the floor
- **Stacked s_h at rest** is not meaningful — always use `prv_compute_stacked_h(ub_h)` directly for at-rest drawing
- **CloudPebble import fails** if duplicate source files exist at different paths — delete before re-import
- **Tilde in resource filenames** breaks CloudPebble GitHub import
- **File "deletion" via MCP** produces a zero-byte file — actual deletion requires GitHub web UI (trash icon)

### Unresolved questions

- STACK_L (symmetric squish) vs STACK_R (fold toward center) stacked animation preference — pending Sterling's device comparison
- Whether to add info line support to round platforms, and what form (arc text? abbreviated column?)

---

## Current TODO

### Immediate (next session)

- Test v3.61 round geometry on chalk or gabbro — confirm per-column heights and no clipping
- Decide STACK_L vs STACK_R animation preference after device comparison
- Delete dead files: `src/digit.c`, `src/digit.h`, `src/case8_patch.txt`, `src/main_digit1_fix.txt`

### Near-term

- Lower-resolution platform tuning (basalt/aplite/diorite/flint) — layout math scales via UNIT, needs device testing
- Config page (JavaScript/Clay) — not yet built

### Backlog / ideas

- Info line support for round platforms (no design decision yet)
- Icon redesign pass (Sterling sketches at 4× in Photoshop; Claude translates to C vector calls)
- Step browser companion app (discussed, not started)

---

## Verification Plan

Before pushing any version:

1. Code compiles on emery (primary target)
2. No geometry regressions — digit height, colon position, info line spacing
3. Wide mode slot collapse works (configure a weather slot; observe before/after weather data arrives)
4. Stacked mode animation fires on minute change
5. For round: confirm outer digits (H_TENS, M_ONES) are shorter than inner (H_ONES, M_TENS) and none clip the bezel
6. SHA fetched fresh immediately before `create_or_update_file`

---

## Source of Truth / External Links

| Resource | URL |
|---|---|
| Pebble SDK docs (primary) | https://developer.repebble.com |
| TallBoy repo | https://github.com/SterlingEly/TallBoy |
| Radium2 repo (reference) | https://github.com/SterlingEly/Radium2 |
| Open-Meteo API | https://open-meteo.com/ |
| WMO weather codes | https://open-meteo.com/en/docs |

---

## Last Updated

v3.61 — June 2026
Updated by: AI collaborator (Claude Sonnet)
Reflects: round platform support (chalk+gabbro), per-column heights, blink speed fix, chalk added to targets, all info slot types through slot 28, documentation standardization.
