# TallBoy — Project Context

> **START HERE.** Read this file completely before making any code changes.
> This is the authoritative technical continuity document for this repository.
> It is written for technical contributors and AI collaborators alike.

---

## Purpose

TallBoy is a Pebble watchface built around oversized vector-drawn digits that dynamically fill the available screen height. The digits squish and expand as an animation mechanic on every minute change.

**Design tenet:** *Watchfaces display data as atmosphere, not content. Artfully vague > precisely readable. Color = mood. Position = direction. The watch is the barometer; the phone is the app.*

The watchface supports all seven Pebble hardware platforms, three display layouts, health and weather data integration, and a pace-based background color system. It is under active development as of mid-2026.

---

## Current Status

**Version: v3.61** — active development, not yet formally released.

### What is working
- Builds clean on all 7 target platforms
- Full-mode animated digits: anticipate → squish → expand on every minute tick
- Wide info mode: up to 3 info slots above + 3 below; digits fill remaining space; empty slots collapse instantly
- Stacked info mode (STACK_L / STACK_R): stacked hour/minute digits on one side, up to 8 info lines in a column on the other; minute-change fast-blink animation
- Pace-based background color spectrum (color + health platforms)
- Weather, UV index, and solar time fetch via PebbleKit JS (Open-Meteo API, no key required)
- All 28 info slot types working, including sleep, calories (active + resting), and typical steps
- Round platform support (chalk + gabbro): per-column digit heights follow circular bezel geometry — outer columns 108px, inner 144px; center point aligned at unobstructed midpoint
- Background: black base → colored rounded-rect (radius UNIT×2 = 16px on emery); plain fill on round (OS clips to circle automatically)

### Known active bugs
None as of v3.61.

### Unresolved questions
- STACK_L animation (symmetric squish) vs STACK_R animation (fold toward center) — preference pending device comparison
- Whether to add info line support to round platforms in a future version

---

## Human / AI Role Split

| Responsibility | Owner |
|---|---|
| Design direction and visual decisions | Sterling Ely |
| CloudPebble builds and device testing | Sterling Ely |
| Code writing and GitHub commits | AI collaborator (Claude) |
| Documentation | AI collaborator, reviewed by Sterling |
| Product decisions: slot content, animation feel | Sterling Ely |
| Geometry math, slot text formatting | AI collaborator |

---

## Repository Structure

```
SterlingEly/TallBoy  (branch: main)
├── src/
│   ├── main.c              ← entire watchface (~88KB); single-file architecture
│   └── pkjs/
│       └── index.js        ← PebbleKit JS: weather/UV/solar fetch + config page
├── appinfo.json            ← app metadata, message keys, target platforms
├── PROJECT_CONTEXT.md      ← this file
├── README.md               ← human-facing overview
└── wscript                 ← Waf build script (do not edit)
```

**Dead files still in repo — safe to delete via GitHub web UI:**
- `src/digit.c`, `src/digit.h` — old raster digit library, fully superseded by vector drawing
- `src/case8_patch.txt`, `src/main_digit1_fix.txt` — old patch notes
- `CLAUDE_CONTEXT.md`, `CONTEXT_TALLBOY.md` — retired legacy context docs (stubbed)
- `_cycle_fix.txt`, `_draw_info_line_snippet.txt`, `_smooth_fix.txt` — old scratch files (stubbed)

---

## Build / Deployment Rules

### Human build workflow
1. Import `SterlingEly/TallBoy` (branch: `main`) into [CloudPebble](https://cloudpebble.net)
2. Build and install via CloudPebble's device push
3. Sterling manually re-imports or pastes updated files after each AI push

### CloudPebble constraints
- `src/main.c` must be at the flat path — CloudPebble does not support `src/c/main.c`
- Menu icons must be added via CloudPebble UI (Resources → + → Image), not via GitHub
- Do not include a `resources/media` block in `appinfo.json` — causes "Unsupported published resource type" errors
- Font resources must be added via the CloudPebble UI "Another Font" button, not via `appinfo.json`
- Tilde (`~`) in resource filenames breaks CloudPebble GitHub import
- CloudPebble import fails if duplicate source files exist at different paths — delete before re-importing

### SDK
- SDK version 3, targeting Pebble OS 3.x
- Primary documentation: [developer.repebble.com](https://developer.repebble.com) (Pebble is officially back — prefer over Rebble docs)

### AI collaborator rules (CRITICAL)
- **ALWAYS use `create_or_update_file` with the current file SHA** — NEVER use `push_files` (confirmed bug: sends empty content, silently zeroing the file)
- Always commit **full file content** — never partial diffs or patches
- **Always fetch the current SHA** immediately before any update — SHAs from prior sessions are stale after any intervening commit
- `get_file_contents` times out on `main.c` (~88KB) — use `create_or_update_file` directly; retry if timeout occurs
- File "deletion" via MCP produces a stub file, not a true delete — actual deletion requires the GitHub web UI trash icon

---

## Architecture

### Platform constants

| Platform | Watch | Screen | UNIT | HALF_UNIT | Notes |
|---|---|---|---|---|---|
| emery | Pebble Time 2 | 200×228 rect | 8 | 4 | Color, HR sensor — primary dev device |
| basalt | Pebble Time | 144×168 rect | 6 | 3 | Color |
| chalk | Pebble Time Round | 180×180 round | 6 | 3 | Color, round |
| diorite | Pebble 2 SE | 144×168 rect | 6 | 3 | B&W, HR sensor |
| flint | Pebble 2 | 144×168 rect | 6 | 3 | B&W |
| aplite | Pebble Classic | 144×168 rect | 6 | 3 | B&W |
| gabbro | Pebble 2 HR | 180×180 round | 6 | 3 | Color, round, HR sensor |

- HR sensors: **emery, diorite, and gabbro only** — use "on supported models" in store copy, never list platforms
- Round platforms (chalk, gabbro): share a single `PBL_ROUND` code block; info layouts not supported on round
- Touch input: emery only — non-functional at firmware level on shipping hardware; all `PBL_TOUCH` code removed

### Digit geometry (stable)

```
UNIT = 8 (emery) / 6 (all others)
SLOT_W       = UNIT * 5     — digit slot width (includes padding)
GLYPH_W      = UNIT * 4     — actual drawn glyph width
HALF_SLOT_PAD = UNIT / 2    — padding each side of glyph

Arc caps — invariant, never change with height h:
  ro = UNIT * 2             — outer arc radius
  ri = UNIT * 1             — inner arc radius
  sw = UNIT                 — stroke width

H_MIN        = UNIT * 11    — minimum legible height (arcs just touching, bars gone)
H_ABSOLUTE_MIN = UNIT * 5   — animation floor for stacked blink

Derived from h:
  bar  = (h - 7*UNIT) / 2                         — gap between inner cap centers
  tail = (h > H_MIN) ? (h - H_MIN) / 4 : 0        — tail stub length (some digits)
```

Digits 1, 2, and 7 use `gpath_create` for diagonal strokes — these allocate/free on every draw call. Null checks are in place; not a memory leak.

### Slot positions — emery (UNIT=8, SLOT_W=40)

```
SLOT_H_TENS=12, SLOT_H_ONES=52, COLON_SLOT_X=80, SLOT_M_TENS=108, SLOT_M_ONES=148
```

### Slot positions — round (chalk/gabbro, UNIT=6, SLOT_W=30, centered on 180px)

```
SLOT_H_TENS=15, SLOT_H_ONES=45, COLON_SLOT_X=75, SLOT_M_TENS=105, SLOT_M_ONES=135
Colon center = x=90 = exact screen center ✓
ROUND_OUTER_H = UNIT*18 = 108px  — H_TENS and M_ONES columns (dx=60 from center)
ROUND_INNER_H = UNIT*24 = 144px  — H_ONES and M_TENS columns (dx=30 from center)
cy on round   = ub_top + ub_h/2  — NOT MARGIN_OUTER + s_target_h/2 (6px off-center)
```

### Slot positions — other rect (basalt/diorite/flint/aplite, UNIT=6, SLOT_W=30)

```
SLOT_H_TENS=6, SLOT_H_ONES=36, COLON_SLOT_X=57, SLOT_M_TENS=78, SLOT_M_ONES=108
```

### Margin model

```
MARGIN_CANVAS = UNIT        — screen edge → bg rounded-rect edge
MARGIN_DIGIT  = UNIT        — bg edge → digit content
MARGIN_OUTER  = UNIT * 2    — convenience alias

QUICK_LOOK_ACTIVE(ub_h) = (ub_h < SCREEN_H - UNIT)
BOTTOM_MARGIN(ub_h)     = QUICK_LOOK_ACTIVE ? MARGIN_DIGIT : MARGIN_OUTER
```

### Layout system

```c
LAYOUT_FULL    = 0  // full-screen time; anticipate → squish → expand each minute
LAYOUT_INFO    = 1  // wide mode; up to 3 info slots above + 3 below time
LAYOUT_STACK_R = 2  // digits right, info column left (right-aligned text)
LAYOUT_STACK_L = 3  // digits left, info column right (left-aligned text)
```

Transitions to stacked: `PHASE_SPLIT_V` → `PHASE_SPLIT_H`
Transitions to full: `PHASE_MERGE_H` → `PHASE_MERGE_V`

Wide blink: fast linear drop/rise, no overshoot, single rep, digit swap at minimum.
Stacked blink: STACK_L = symmetric squish; STACK_R = fold toward center (hours top pinned, minutes bottom pinned).

### Animation phases

```
Full mode:      PHASE_DONE → PHASE_ANTICIPATE → PHASE_SQUISH → PHASE_EXPAND → PHASE_DONE
Wide/stacked:   PHASE_DONE → PHASE_BLINK (down, swap at min, ease up) → PHASE_DONE
To stacked:     PHASE_SPLIT_V → PHASE_SPLIT_H
To full:        PHASE_MERGE_H → PHASE_MERGE_V
```

### Key state variables

```c
static int s_h = 0;               // current digit height (animated)
static int s_target_h = 0;        // target height for current layout
static int s_h_min = 0;           // animation floor (H_MIN or H_ABSOLUTE_MIN)
static int s_stk_h_start = 0;     // height at start of stacked blink (Option B drift)
static int s_steps_expected = -1; // typical steps at current time (same-day-of-week avg)
static int s_steps_typical_day = -1; // typical full-day total (same-day-of-week avg)
static int s_calories_resting = 0;   // BMR calories today
static int s_sleep_seconds = -1;     // sleep since 6pm yesterday; -1 = unavailable
static int s_uv_index = -1;          // -1 = never received; 0+ = valid (0 is real UV 0)
static time_t s_weather_ts = 0;      // unix time of last weather receipt; 0 = never
static int s_weather_temp_f = -999;  // sentinel for no data
```

### Data caching philosophy

**Hide > mislead.** If data is unavailable or stale, the slot returns `false` and disappears. Never show zeros or stale defaults.

- UV index 0 is a valid reading — use `-1` as the "never received" sentinel
- Weather/UV ages out after 3 hours without a phone update (`WEATHER_MAX_AGE = 3 * 3600`)
- Sunrise/sunset never ages out (astronomical; valid all day)
- Heart rate displays `"--"` when the sensor exists but has no reading — the only exception to the hide rule
- Wide mode empty slots collapse immediately on the next redraw after data arrives or expires

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

### Persist keys

```
PERSIST_CFG_VERSION=0, PERSIST_CFG_FLAGS=1, PERSIST_CFG_WIDE=2
PERSIST_CFG_STACK=3, PERSIST_CFG_STACK_HI=4, PERSIST_CFG_COLORS=5 (and 6)
CFG_VERSION=5  ← bump this if the persist layout changes
```

### Slot type IDs (complete)

```
0  = empty
1  = day                        3  = day+date
2  = date                       4  = temp
5  = weather                    6  = steps
7  = distance                   8  = exp_steps (typical at current time)
9  = pace (% of typical)        10 = calories (active)
11 = heart_rate                 12 = sunrise
13 = sunset                     14 = daylight duration
15 = battery                    16 = bluetooth (shows only when disconnected)
17 = sunrise+sunset             18 = steps+exp_steps (wide only)
19 = exp_steps+pace (wide only) 20 = battery+bluetooth (wide only)
21 = debug                      22 = UV index
23 = light_remaining            24 = uv+light (wide only)
25 = temp+UV (wide only)        26 = typical_day (full-day step total)
27 = calories_total (active+resting)   28 = sleep duration
```

---

## Known Bugs / Known Traps

### Active bugs
None as of v3.61.

### Known traps — do not repeat

| Trap | Notes |
|---|---|
| `push_files` sends empty content | Always use `create_or_update_file` with the current SHA |
| Stale SHA | Fetch SHA immediately before every update — any intervening commit invalidates it |
| `get_file_contents` times out on `main.c` | Skip the read; write directly using `create_or_update_file` |
| Degree symbol (0xB0) breaks weather text | Use plain ASCII — `"72 F"` not `"72°F"` — degree symbol silently prevents rendering |
| Gabbro is round, not rectangular | Never group gabbro with aplite/basalt/diorite/flint; use `PBL_ROUND` |
| Round `cy` must be `ub_top + ub_h/2` | `MARGIN_OUTER + s_target_h/2` is 6px off-center on 180px screens |
| Wide mode draw height | Use `actual_h` (live-computed from rendered slot count), not `s_target_h` |
| Stacked blink floor | Uses `H_ABSOLUTE_MIN`, not `H_MIN` |
| Stacked `s_h` at rest | Not meaningful at rest — use `prv_compute_stacked_h(ub_h)` directly |
| CloudPebble import with duplicate sources | Delete conflicting files before re-importing |
| Tilde in resource filenames | Breaks CloudPebble GitHub import |
| MCP file deletion produces stub | Actual deletion requires GitHub web UI trash icon |

---

## Current TODO

### Immediate
- Test v3.61 round geometry on chalk or gabbro — confirm per-column heights look correct and nothing clips
- Decide STACK_L vs STACK_R animation preference after device comparison

### Near-term
- Delete dead files via GitHub web UI: `src/digit.c`, `src/digit.h`, `src/case8_patch.txt`, `src/main_digit1_fix.txt`, and the root scratch/stub files
- Lower-resolution platform tuning (basalt/aplite/diorite/flint) — layout math scales with UNIT; needs device testing
- Config page (JavaScript/Clay) — not yet built

### Backlog
- Info line support for round platforms — no design decision yet
- Icon redesign pass — Sterling sketches at 4× in Photoshop; AI translates to C vector drawing calls
- Step browser companion app — discussed, not started

---

## Verification Plan

Before committing any version:

1. Code compiles on emery (primary target)
2. No geometry regressions — digit height, colon position, info line spacing
3. Wide mode slot collapse works correctly when data is absent vs. present
4. Stacked mode minute-change blink animation fires correctly
5. For round: outer digits (H_TENS, M_ONES) are visibly shorter than inner (H_ONES, M_TENS); no bezel clipping
6. SHA fetched fresh immediately before `create_or_update_file`

---

## Source of Truth / External Links

| Resource | URL |
|---|---|
| Pebble SDK docs (primary) | https://developer.repebble.com |
| TallBoy repo | https://github.com/SterlingEly/TallBoy |
| Open-Meteo API | https://open-meteo.com/ |
| WMO weather code reference | https://open-meteo.com/en/docs |

---

## Related Projects

Other Pebble watchface and tool repositories by Sterling Ely:

| Repository | URL | Relationship |
|---|---|---|
| Radium2 | https://github.com/SterlingEly/Radium2 | Sibling watchface; radial time display with weather, health presets, and config; the most mature released project in the portfolio |
| BarGraph2 | https://github.com/SterlingEly/BarGraph2 | Sibling watchface; time as vertical bar graphs; rebuild of the original 2013 Pebble concept |
| Monogram | https://github.com/SterlingEly/Monogram | Sibling watchface; custom monogram-style digit assets; early development, targets gabbro (Pebble Round 2) |
| PixelSampler | https://github.com/SterlingEly/PixelSampler | Developer reference tool; samples fonts, colors, and hardware capabilities across Pebble platforms |

TallBoy shares platform constants, build rules, and CloudPebble conventions with all of the above. Radium2 is the most complete reference for PebbleKit JS weather/solar fetch patterns.

---

## Last Updated

v3.61 — July 2026
Updated by: AI collaborator (Claude Sonnet)
Changes in this pass: added Related Projects section, reorganized Build Rules to separate human vs. AI collaborator rules, minor clarifications throughout, updated status.
