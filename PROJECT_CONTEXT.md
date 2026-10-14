# TallBoy — Project Context

> **START HERE.** Read this file completely before making any code changes.
> This is the authoritative technical continuity document for this repository.
> It is written for technical contributors and AI collaborators alike.

---

## Purpose

TallBoy is a Pebble watchface built around oversized vector-drawn digits that dynamically fill the available screen height. The digits squish and expand as an animation mechanic on every minute change.

**Design tenet:** *Watchfaces display data as atmosphere, not content. Artfully vague > precisely readable. Color = mood. Position = direction. The watch is the barometer; the phone is the app.*

The watchface supports all seven Pebble hardware platforms, three display layouts, health and weather data integration, and a pace-based background color system. It is under active development as of autumn 2026.

---

## Current Status

**Internal version: v3.65** — active development, not yet formally released. (Internal numbers are development checkpoints, not public releases.)

### What is working
- Builds clean on all 7 target platforms as of v3.63; v3.64 ran on a real gabbro (Sterling, 2026-10-14: "looks pretty good" apart from the colon spacing fixed in v3.65); v3.65 is not yet compiled or device-tested
- Full-mode animated digits: anticipate → squish → expand on every minute tick
- Wide info mode: up to 3 info slots above + 3 below; digits fill remaining space; empty slots collapse instantly
- Stacked info mode (STACK_L / STACK_R): stacked hour/minute digits on one side, up to 8 info lines in a column on the other; minute-change fast-blink animation
- Pace-based background color spectrum (color + health platforms), with three palettes selectable in the config page — dark (#AA max channel, white fg), medium (default, white fg), light (#55 min channel, black fg) — plus a static-color mode (v3.62)
- Separately colorable colon (`CfgColorColon`, v3.62); default custom background dark red #AA0000
- Weather, UV index, and solar time fetch via PebbleKit JS (Open-Meteo API, no key required)
- Air quality (v3.63): US AQI from the Open-Meteo air-quality API (`current=us_aqi`), fetched on connect and every 30 min; hidden after 3 h without an update like weather/UV; stacked slot reads "AQI ##"; uses `icon_haze`. **Device-confirmed working by Sterling, 2026-10-13.**
- All info slot types (IDs 0–30) working, including sleep, calories (active + resting), typical steps, AQI (29) and the wide-only Temp + UV + AQI combo (30)
- Sun icon redrawn (v3.62: filled disc + thin rays; v3.63: longer rays with correct diagonal directions)
- Round platform support: per-column digit heights follow circular bezel geometry; center point aligned at unobstructed midpoint. **chalk (180×180) and gabbro (260×260) now have separate geometry blocks (v3.64).**
- Background: black base → colored rounded-rect (radius UNIT×2 = 16px on emery); plain fill on round (OS clips to circle automatically)

### v3.65 changes (2026-10-14) — NOT YET COMPILED OR DEVICE-TESTED
**Gabbro colon spacing.** Sterling saw v3.64 on a gabbro and reported the digit-to-colon space looked about twice the usual design. Cause: v3.64 laid gabbro's five slots out at a uniform `SLOT_W` (40px) pitch, so the colon slot was a full slot wide on each side. Emery's design uses a 28px (3.5 UNIT) pitch from the inner digit slot to the colon slot, which puts the dot 8px (1 UNIT) from each neighboring glyph. v3.64 put it 20px away (2.5 UNIT), and the glyph-to-glyph gap across the colon was 6 UNIT instead of 3.
Fix: gabbro slots moved to `H_TENS 42 | H_ONES 82 | colon 110 | M_TENS 138 | M_ONES 178`, `SIDE_MARGIN 42`. Colon center is still x=130. Column heights unchanged (outer 160, inner 208); the outer columns now sit at dx=68 instead of 80, so there is geometric headroom to raise `ROUND_OUTER_H` if it looks short.
Note chalk's block still uses a uniform 30px pitch (UNIT 6) — Sterling has not reported its spacing, so it was left alone.

### v3.64 changes (2026-10-13)
1. **Gabbro split from chalk.** Gabbro (Pebble Round 2) is a 260×260 round screen; it had been sharing chalk's 180×180 geometry block (and was mislabeled "Pebble 2 HR"). It now has its own block: `UNIT=8` (same glyph scale as emery), `SLOT_W=40`, slots centered on x=130 (positions superseded by v3.65 — see Architecture), `ROUND_OUTER_H = UNIT*20 = 160`, `ROUND_INNER_H = UNIT*26 = 208`. **The two round heights are scaled from chalk's proportions (outer ≈ 0.6, inner ≈ 0.8 of screen height), not measured** — tune on the real device.
2. **Heart rate fixed.** `prv_update_health()` used `health_service_metric_accessible()` for `HealthMetricHeartRateBPM`, which always reports HR unavailable. It now uses `health_service_metric_aggregate_averaged_accessible(metric, now, now, HealthAggregationAvg, HealthServiceTimeScopeOnce)` — the same fix that made HR work in Radium 2.

### Known active bugs
None logged. (v3.65 change above is unverified.)

### Unresolved questions
- STACK_L animation (symmetric squish) vs STACK_R animation (fold toward center) — preference pending device comparison
- Whether to add info line support to round platforms in a future version
- Gabbro digit heights (see v3.64 note) — scaled, not measured
- Whether chalk's uniform-pitch spacing has the same too-wide colon gap (awaiting a look on a Time Round)
- Whether the HR slot should hide (rather than show "--") on platforms with no HR sensor (basalt, chalk, gabbro, flint, aplite) — currently shows "--"

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
│   ├── main.c              ← entire watchface (~98KB); single-file architecture
│   └── pkjs/
│       └── index.js        ← PebbleKit JS: weather/UV/solar/AQI fetch + config page
├── appinfo.json            ← app metadata, message keys, target platforms
├── PROJECT_CONTEXT.md      ← this file
├── README.md               ← human-facing overview
└── wscript                 ← Waf build script (do not edit)
```

The repo contains only the files above (plus `resources/`). The old dead/scratch files were deleted on 2026-09-28.

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
- `get_file_contents` on `main.c` (~98KB) has timed out in some sessions and worked in others — retry before assuming it is unavailable
- After pushing a large file, verify it: compute the git blob hash locally (`printf 'blob %s\0' <bytes> | cat - file | sha1sum`) and compare with the SHA the API returns — they match only if the upload is byte-identical
- File "deletion" via MCP produces a stub file, not a true delete — actual deletion requires the GitHub web UI trash icon

---

## Architecture

### Platform constants

| Platform | Watch | Screen | UNIT | HALF_UNIT | Notes |
|---|---|---|---|---|---|
| emery | Pebble Time 2 | 200×228 rect | 8 | 4 | Color, HR sensor — primary dev device |
| basalt | Pebble Time | 144×168 rect | 6 | 3 | Color, no HR |
| chalk | Pebble Time Round | 180×180 round | 6 | 3 | Color, round, no HR |
| diorite | Pebble 2 (SE and HR models) | 144×168 rect | 6 | 3 | B&W; HR sensor on the HR model only |
| flint | Pebble 2 Duo | 144×168 rect | 6 | 3 | B&W, no HR |
| aplite | Pebble Classic / Steel | 144×168 rect | 6 | 3 | B&W, no HR, no health service |
| gabbro | Pebble Round 2 (Core Devices, 2026) | **260×260 round** | **8** | 4 | Color, round, **no HR sensor** |

- HR sensors: **emery and diorite (HR model) only** — use "on supported models" in store copy, never list platforms
- Round platforms (chalk, gabbro): share the `PBL_ROUND` code paths but have **separate geometry blocks** (chalk 180×180 UNIT 6; gabbro 260×260 UNIT 8); info layouts not supported on round
- Touch input: emery only — non-functional at firmware level on shipping hardware; all `PBL_TOUCH` code removed

### Digit geometry (stable)

```
UNIT = 8 (emery, gabbro) / 6 (all others)
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
Digit slot → colon slot pitch = 28px (3.5 UNIT)  — the reference spacing
Glyphs: H_ONES 56..88, colon dot centered at x=100 (r=4), M_TENS 112..144  → dot is 8px from each glyph
```

Slot geometry rule: a glyph is drawn at `slot_x + HALF_SLOT_PAD` and is `GLYPH_W` wide; the colon dot is centered at `COLON_SLOT_X + SLOT_W/2`. When defining a new platform, set the digit-to-colon pitch to 3.5 UNIT (not a full `SLOT_W`) to match the emery design.

### Slot positions — chalk (UNIT=6, SLOT_W=30, centered on 180px)

```
SLOT_H_TENS=15, SLOT_H_ONES=45, COLON_SLOT_X=75, SLOT_M_TENS=105, SLOT_M_ONES=135
Colon center = x=90 = exact screen center ✓
ROUND_OUTER_H = UNIT*18 = 108px  — H_TENS and M_ONES columns (dx=60 from center)
ROUND_INNER_H = UNIT*24 = 144px  — H_ONES and M_TENS columns (dx=30 from center)
cy on round   = ub_top + ub_h/2  — NOT MARGIN_OUTER + s_target_h/2 (6px off-center)
Uniform 30px pitch (10 units... i.e. 5 UNIT) around the colon, wider than emery's 3.5 UNIT — not yet reviewed on device
```

### Slot positions — gabbro (UNIT=8, SLOT_W=40, centered on 260px) — v3.65, unverified on device

```
SLOT_H_TENS=42, SLOT_H_ONES=82, COLON_SLOT_X=110, SLOT_M_TENS=138, SLOT_M_ONES=178
SIDE_MARGIN=42 (unused in round full mode)
Colon center = 110 + 20 = 130 = exact screen center ✓  (42px margin each side)
Digit slot → colon slot pitch = 28px (3.5 UNIT), same as emery. Glyphs: H_ONES 86..118, dot at 126..134, M_TENS 142..174
ROUND_OUTER_H = UNIT*20 = 160px  — H_TENS and M_ONES columns (dx=68 from center)
ROUND_INNER_H = UNIT*26 = 208px  — H_ONES and M_TENS columns (dx=28 from center)
Heights are scaled from chalk's proportions (outer ~0.6, inner ~0.8 of screen), NOT measured.
Superseded v3.64 slots (too airy): 30 / 70 / colon 110 / 150 / 190, SIDE_MARGIN 30
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
static int s_aqi = -1;               // -1 = never received; 0+ = valid
static time_t s_weather_ts = 0;      // unix time of last weather receipt; 0 = never
static time_t s_aqi_ts = 0;          // unix time of last AQI receipt; 0 = never
static int s_weather_temp_f = -999;  // sentinel for no data
```

### Data caching philosophy

**Hide > mislead.** If data is unavailable or stale, the slot returns `false` and disappears. Never show zeros or stale defaults.

- UV index 0 is a valid reading — use `-1` as the "never received" sentinel
- Weather/UV/AQI age out after 3 hours without a phone update (`WEATHER_MAX_AGE = 3 * 3600`)
- Sunrise/sunset never ages out (astronomical; valid all day)
- Heart rate displays `"--"` when the slot is shown but there is no reading — the only exception to the hide rule
- Wide mode empty slots collapse immediately on the next redraw after data arrives or expires

### Heart rate (SDK quirk)

`health_service_metric_accessible()` **always** reports `HealthMetricHeartRateBPM` as unavailable. Use `health_service_metric_aggregate_averaged_accessible(HealthMetricHeartRateBPM, now, now, HealthAggregationAvg, HealthServiceTimeScopeOnce)` as the availability check, then `health_service_peek_current_value()` to read it. Only emery and diorite (HR model) have the sensor.

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
CfgColorColon=33 (v3.62), AqiIndex=34 (v3.63)
```

### Persist keys

```
PERSIST_CFG_VERSION=0, PERSIST_CFG_FLAGS=1, PERSIST_CFG_WIDE=2
PERSIST_CFG_STACK=3, PERSIST_CFG_STACK_HI=4, PERSIST_CFG_COLORS=5 (and 6)
CFG_VERSION=6  ← bump this if the persist layout changes (bumped to 6 in v3.62 for colon color)
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
29 = AQI (stacked: "AQI ##")        30 = temp+UV+AQI (wide only; degrades gracefully if any value is missing)
```

---

## Known Bugs / Known Traps

### Active bugs
None logged.

### Known traps — do not repeat

| Trap | Notes |
|---|---|
| `push_files` sends empty content | Always use `create_or_update_file` with the current SHA |
| Stale SHA | Fetch SHA immediately before every update — any intervening commit invalidates it |
| `health_service_metric_accessible` for heart rate | Always reports HR unavailable. Use `aggregate_averaged_accessible` with a point-in-time range (see Architecture → Heart rate) |
| Degree symbol (0xB0) breaks weather text | Use plain ASCII — `"72 F"` not `"72°F"` — degree symbol silently prevents rendering |
| **Gabbro is 260×260, not 180×180** | gabbro = Pebble Round 2 (large round, no HR). It was once wrongly grouped with chalk (180×180) and labeled "Pebble 2 HR". Never share chalk's geometry block with gabbro; never group gabbro with rect platforms |
| **Uniform slot pitch makes the colon gap too wide** | Spacing slots at a full `SLOT_W` on both sides of the colon doubles the digit-to-colon gap versus emery (gabbro v3.64). Use 3.5 UNIT between the inner digit slots and the colon slot. Positions must be checked on device, not just derived from "centered and evenly spaced" |
| Round `cy` must be `ub_top + ub_h/2` | `MARGIN_OUTER + s_target_h/2` is off-center on round screens |
| Wide mode draw height | Use `actual_h` (live-computed from rendered slot count), not `s_target_h` |
| Stacked blink floor | Uses `H_ABSOLUTE_MIN`, not `H_MIN` |
| Stacked `s_h` at rest | Not meaningful at rest — use `prv_compute_stacked_h(ub_h)` directly |
| CloudPebble import with duplicate sources | Delete conflicting files before re-importing |
| Tilde in resource filenames | Breaks CloudPebble GitHub import |
| MCP file deletion produces stub | Actual deletion requires GitHub web UI trash icon |

---

## Current TODO

### Immediate
- **Compile and device-test v3.65** on gabbro: colon spacing now matches emery; check whether the outer digit columns (160px) look short now that they sit closer in (dx=68) — `ROUND_OUTER_H` can likely go up (a circle-inscribed estimate allows about 190px), and check nothing clips at the bezel
- Confirm heart rate shows a real BPM on emery (v3.64 fix)
- Device-test the v3.62 changes still unconfirmed: pace palettes, colon color, sun icon, Temp+UV+AQI combo (AQI itself is confirmed working)
- Decide the default pace palette (code default: medium)
- Decide STACK_L vs STACK_R animation preference after device comparison
- Look at chalk (Time Round) colon spacing — it still uses a uniform 30px pitch

### Near-term
- Lower-resolution platform tuning (basalt/aplite/diorite/flint) — layout math scales with UNIT; needs device testing
- Config page — exists in `src/pkjs/index.js`; further polish as needed
- Consider hiding the HR slot on platforms with no HR sensor instead of showing "--"

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
5. For round: outer digits (H_TENS, M_ONES) are visibly shorter than inner (H_ONES, M_TENS); no bezel clipping — check on **both** chalk and gabbro, they have separate geometry
6. Heart rate shows a live BPM (not `--`) on emery
7. Digit-to-colon gap on any new or changed platform matches emery's (dot 1 UNIT from each glyph)
8. SHA fetched fresh immediately before `create_or_update_file`; large pushes verified by blob hash

---

## Source of Truth / External Links

| Resource | URL |
|---|---|
| Pebble SDK docs (primary) | https://developer.repebble.com |
| TallBoy repo | https://github.com/SterlingEly/TallBoy |
| Open-Meteo API | https://open-meteo.com/ |
| Open-Meteo air-quality API (US AQI 0–500) | https://open-meteo.com/en/docs/air-quality-api |
| WMO weather code reference | https://open-meteo.com/en/docs |

Note: air-quality data is CAMS ENSEMBLE via Open-Meteo, whose terms require attribution to the CAMS data provider as well as Open-Meteo. Add that acknowledgement to store/README copy before any public release.

---

## Related Projects

Other Pebble watchface and tool repositories by Sterling Ely:

| Repository | URL | Relationship |
|---|---|---|
| Radium2 | https://github.com/SterlingEly/Radium2 | Sibling watchface; radial time display with weather, health, UV/AQI and pace features; the most mature released project in the portfolio. Radium 2's pace color and info-line set were ported from TallBoy |
| BarGraph2 | https://github.com/SterlingEly/BarGraph2 | Sibling watchface; time as vertical bar graphs; rebuild of the original 2013 Pebble concept |
| Monogram | https://github.com/SterlingEly/Monogram | Sibling watchface; custom monogram-style digit assets; early development, targets gabbro (Pebble Round 2) |
| PixelSampler | https://github.com/SterlingEly/PixelSampler | Developer reference tool; samples fonts, colors, and hardware capabilities across Pebble platforms |

TallBoy shares platform constants, build rules, and CloudPebble conventions with all of the above. Radium2 is the most complete reference for PebbleKit JS weather/solar fetch patterns and for the heart-rate API quirk.

---

## Last Updated

v3.65 — 2026-10-14
Updated by: AI collaborator (Claude, Anthropic) with direction from Sterling Ely
Changes in this pass: gabbro colon spacing corrected to emery's 3.5 UNIT pitch (slots 42/82/110/138/178); new trap and verification item; chalk spacing flagged for review.

Previous: v3.64 2026-10-13 (gabbro split from chalk, HR fix, AQI device-confirmed, platform table naming); v3.63 doc sync 2026-09-28; v3.61 — July 2026: added Related Projects section, reorganized Build Rules, updated status.
