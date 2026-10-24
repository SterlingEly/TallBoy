# TallBoy — Project Context

> **START HERE.** Read this file completely before making any code changes.
> This is the authoritative technical continuity document for this repository.
> It is written for technical contributors and AI collaborators alike.

---

## Purpose

TallBoy is a Pebble watchface built around oversized vector-drawn digits that dynamically fill the available screen height. The digits squish and expand as an animation mechanic on every minute change.

**Design tenet:** *Watchfaces display data as atmosphere, not content. Artfully vague > precisely readable. Color = mood. Position = direction. The watch is the barometer; the phone is the app.*

The watchface supports all seven Pebble hardware platforms, three display layouts (on every platform, round included since v3.67), health and weather data integration, and a pace-based background color system. It is under active development as of autumn 2026.

**Provenance:** TallBoy is an original Sterling Ely design (unlike Radium 2 and Bar Graph 2, which are rebuilds of older faces). On round watches it delivers about 90% of what the shelved Monogram concept was meant to be, which is why Monogram was shelved on 2026-10-24 (see Related Projects).

---

## Current Status

**Internal version: v3.70** — active development, not yet formally released. (Internal numbers are development checkpoints, not public releases.) A DRAFT `STORE_LISTING.md` exists (added 2026-10-24); there is no store link.

### What is working
- Builds clean on all 7 target platforms as of v3.63; v3.64 ran on a real gabbro (Sterling, 2026-10-14: "looks pretty good" apart from the colon spacing fixed in v3.65); v3.65 through v3.70 are not yet compiled against the real SDK or device-tested (v3.67 and v3.68 were syntax-checked against a stub header, and the v3.68 and v3.70 geometry was checked with the glyph simulator in `tb/`)
- Full-mode animated digits: anticipate → squish → expand on every minute tick
- Wide info mode: up to 3 info slots above + 3 below; digits fill remaining space; empty slots collapse instantly (round: short-format lines only, 2 per side on gabbro, 1 per side on chalk — see v3.67)
- Stacked info mode (STACK_L / STACK_R): stacked hour/minute digits on one side, up to 8 info lines in a column on the other (round: up to 6 on gabbro, 5 on chalk); minute-change fast-blink animation
- Pace-based background color spectrum (color + health platforms), with three palettes selectable in the config page — dark (#AA max channel, white fg), medium (default, white fg), light (#55 min channel, black fg) — plus a static-color mode (v3.62)
- Separately colorable colon (`CfgColorColon`, v3.62); default custom background dark red #AA0000
- Weather, UV index, and solar time fetch via PebbleKit JS (Open-Meteo API, no key required)
- Air quality (v3.63): US AQI from the Open-Meteo air-quality API (`current=us_aqi`), fetched on connect and every 30 min; hidden after 3 h without an update like weather/UV; stacked slot reads "AQI ##"; uses `icon_haze`. **Device-confirmed working by Sterling, 2026-10-13.**
- All info slot types (IDs 0–30) working, including sleep, calories (active + resting), typical steps, AQI (29) and the wide-only Temp + UV + AQI combo (30)
- Sun icon redrawn (v3.62: filled disc + thin rays; v3.63: longer rays with correct diagonal directions)
- Round platform support: per-column digit heights follow circular bezel geometry; center point aligned at unobstructed midpoint. **chalk (180×180) and gabbro (260×260) have separate geometry blocks (v3.64).** Wide and stacked L/R layouts work on round since v3.67.
- Background: black base → colored rounded-rect (radius UNIT×2 = 16px on emery); plain fill on round (OS clips to circle automatically)

### v3.70 changes (2026-10-16) — NOT YET COMPILED AGAINST THE REAL SDK OR DEVICE-TESTED

Sterling (2026-10-16 21:23 ET): "reduce the gabbro digit spacing from 2 units to 1.5 units. the spread looked good, but it crunched the digits too much when stacked" and "pin the top and just shift up the bottoms of the digits when in full time mode, but just have nothing move for quick look in both wide and stacked info modes". Pushed as commit 1aeecd8, `main.c` blob 835a0f5ee53b50284b1f2ade0b6c50c01a0a6e48 (114,694 bytes, hash-verified).

**1. Gabbro spacing 2U → 1.5U.** `SLOT_W` 44 (glyph 32 + 12 gap). Slots `H_TENS 34 | H_ONES 78 | colon 108 | M_TENS 142 | M_ONES 186`; glyph span 38..222; colon dot still centered at x=130. Every gap is 12px (1.5 UNIT): glyph | 12 | glyph | 12 | dot 126..134 | 12 | glyph | 12 | glyph. Outer glyph edge dx=92 from center, inner dx=48.

**2. Heights recovered.** `ROUND_OUTER_H` 136 → **158**, `ROUND_INNER_H` 216 → **224** (`UNIT * 28`). Stacked bounds still equal the full-view ones; `ROUND_STACK_INFO_H` stays 176. Stacked rows (via `prv_round_row_h(bound) = (bound/2 - HALF_UNIT) & ~1`): outer **74**, inner **108** (were 64 / 104 at 2U).

**3. Stacked info column.** Stacked layouts keep the v3.69 scheme: STACK_L on `SLOT_H_TENS` / `SLOT_H_ONES`, STACK_R on `SLOT_M_TENS` / `SLOT_M_ONES`; the info column's x is `ones_x + 48` (1.5U gap to the glyph edge).

**4. Quick Look on round (new behavior).** `prv_view_bounds(root)` returns the full layer bounds on round (the unobstructed bounds on rect), so wide, stacked and the split/merge transitions ignore the peek and nothing moves. Full-time mode alone reads the real unobstructed bottom (`obs_bot`) and calls `prv_round_fit(h_col, cy, obs_bot, &h, &cy)`, which keeps each column's top where it was (centre stays at the full-screen midpoint) and only shortens the bottom when it would drop below `obs_bot - HALF_UNIT`; heights are forced even and floored at `H_MIN`. Rect behavior is unchanged (rect full mode already pinned the top). Chalk uses the same code path; its spacing is unchanged.

**Known trade-offs / caveats (v3.70):**
- Wide "1+1 lines" clearance on gabbro is 0.95U, slightly under the 1U rule of thumb.
- Starting a split/merge while a peek is active in full mode causes a small jump in the bottoms (transitions use the full-screen layout).
- Stacked rows are taller than at 2U but shorter than the v3.67 84-px outer rows.
- Not verified: real SDK build, on-device rendering, quick-look behavior on hardware.

**Verified (geometry only):** `tb/verify370.py` — corner clearance for digits 0–9 at gabbro heights 158 / 224 and the stacked rows; `tb/tbsim.py` for the quick-look fit.

### v3.69 changes (2026-10-16) — superseded in part by v3.70 (spacing and heights); stacked scheme still in force

Sterling (2026-10-16): "the spacing looks good: could we get the stacked modes to keep the same spacing and relative placement?" Gabbro only; chalk's stacked slots already coincide with its full-view slots, so chalk is unchanged.

- **Stacked pair on the full-view slots.** `STACK_SLOT_W = SLOT_W` on gabbro. `prv_stacked_geom` puts STACK_L on `SLOT_H_TENS` / `SLOT_H_ONES` and STACK_R on `SLOT_M_TENS` / `SLOT_M_ONES`, so each digit keeps its full-view x. `SIDE_MARGIN` is unused on gabbro.
- **Full-view bounds.** `ROUND_STACK_OUTER_H` / `ROUND_STACK_INNER_H` = `ROUND_OUTER_H` / `ROUND_INNER_H`. `prv_round_stack_bound` treats `SLOT_H_TENS` and `SLOT_M_ONES` as the outer columns.
- **Info column.** Keeps its own height, `ROUND_STACK_INFO_H` (UNIT*22 = 176, the v3.67 extent; chalk: = `ROUND_OUTER_H`). The column x keeps a 1.5U gap to the glyphs.
- **Trade-off at the time (2U).** Row heights dropped to outer 64 / inner 104; v3.70's 1.5U spacing recovers them to 74 / 108.

### v3.68 changes (2026-10-14) — superseded for gabbro by v3.70 (2U → 1.5U), kept for the measurements
Requested by Sterling: widen the digit and colon spacing on gabbro to 2 UNIT to use the screen width better, and consider the same for chalk ("same units with a bit more spacing may be the better call" versus raising the unit px).

**1. Gabbro spacing 1U → 2U.** Every gap in the time row was 16px (`SLOT_W` 48; slots 26 / 74 / 106 / 146 / 194). **Relaxed to 1.5U in v3.70 after the stacked digits looked crunched.**

**2. The cost: round height.** Wider spacing pushes the outer glyphs toward the bezel, so the full-view and wide heights drop to keep ≥1U corner clearance, measured with `tb/gapscan.py` against the real glyph shapes: gabbro at 2U was `ROUND_OUTER_H` 136 / `ROUND_INNER_H` 216 (from 176 / 224 at 1U). At 1.5U they are 158 / 224.

**3. Stacked constants.** `ROUND_STACK_OUTER_H` / `ROUND_STACK_INNER_H` / `STACK_SLOT_W` introduced here (gabbro 176 / 224 on a 1U pitch at the time); `prv_round_stack_bound(slot_x)` chooses the bound. v3.69 pointed them at the full-view values.

**4. Chalk unchanged — decision pending (Sterling).** Measured trade-off for widening chalk (UNIT 6, current gaps 1U):

| Chalk spacing | Outer height (now 108) | Inner height (now 150) |
|---|---|---|
| 1U (current) | 108 | 150 |
| 1.5U | ≈94 | 152 |
| 2U | 72 | not computed |

2U would cut chalk's outer columns to 72px (about 12 UNIT, near `H_MIN` 66), which is not practical; 1.5U is feasible but shortens the outer digits by about 14px. Raising the UNIT px instead has a similar height cost on the same bezel. Chalk stays at 1U until Sterling decides.

**5. Reference facts gathered during this pass** (apply to TallBoy's Gothic info lines): the next Gothic Bold above 24 is `GOTHIC_28_BOLD`, then 36 (it would need ≈20px icons); the system bold Lecos are 20, 26_AM_PM, 32, 36, 38 and 60_BOLD_NUMBERS_AM_PM. TallBoy's icon set and icon-to-text spacing are the reference that Radium v2.4 was aligned to on 2026-10-14.

### v3.67 changes (2026-10-14) — NOT YET COMPILED AGAINST THE REAL SDK OR DEVICE-TESTED
Requested by Sterling: "what I would like to see is the wide & left/right stacked views on round", plus taller round digits.

**1. Round heights raised (measured, not scaled).** Sterling asked to measure instead of guess. Method: for every digit 0–9, take the worst lit pixel of the actual glyph geometry against the bezel circle, and report clearance in UNITs (U) both at the column centre-x and at the worst glyph corner.

| | Outer col (H_TENS, M_ONES) | Inner col (H_ONES, M_TENS) |
|---|---|---|
| chalk, old (108/144) | 108: 1.2U corner / 2.6U centre | 144: 1.8U / 2.5U |
| gabbro, old (160/208) | 160: 1.75U / 3.1U | 208: 2.15U / 2.8U |
| **chalk, new (108/150)** | 108 (114 would give a 0.83U corner, so outer stays) | **150: 1.37U / 2.0U** |
| **gabbro, new (176/224)** | **176: 1.05U / 2.35U** | **224: 1.23U / 1.82U** |

Chalk: `ROUND_OUTER_H 108`, `ROUND_INNER_H 150` (UNIT×25). Gabbro (v3.67 values at 1U spacing; later superseded: v3.68 136 / 216 at 2U, v3.70 158 / 224 at 1.5U): `ROUND_OUTER_H 176` (UNIT×22), `ROUND_INNER_H 224` (UNIT×28). Sterling chose the "slightly taller" gabbro option (inner 224, not 216). Rule of thumb used: ≥1U clearance at the glyph corners.

**2. Wide on round.** Short-format lines only (`build_lines(..., wide=false)`), like Radium. Capped at `ROUND_WIDE_LINES` per side (**gabbro 2, chalk 1** — Sterling: "def 2 lines max, maybe even 1 for chalk"). Lines are inset `ROUND_INFO_INSET` (chalk 11, gabbro 13) from the unobstructed top/bottom and each is centred and clamped to the bezel chord (`prv_round_chord_half(y, y+INFO_GLYPH_H) - UNIT`). Digit height comes from `prv_round_wide_h(ub_h, an, bn)` (capped at `ROUND_INNER_H`, floor `H_MIN`); columns still cap themselves (`draw_digits_full_round`). Slots that return false when `!wide` simply disappear on round (EXP_PACE, BAT_BT, UV_LIGHT, WEATHER_UV, TEMP_UV_AQI, STEPS_EXP).

**3. Stacked L/R on round.** Per-column rows, drawn by `prv_round_stacked_draw`. Sterling's spec: "tops of top digits and bottoms of bottom digits stretch to unstacked bounds while the other ends meet in the middle as usual." So each column's top row top / bottom row bottom sits at that column's full-view bound; the inner ends sit `HALF_UNIT` either side of the vertical centre (UNIT gap). Row height = `(bound/2 - HALF_UNIT) & ~1` (forced even because `draw_digit_vec` uses `h/2` on both sides, so a column whose bound leaves an odd row height starts 1px inside its bound). The info column uses `ROUND_STACK_LINES` (chalk 5, gabbro 6) spread over the outer-column extent, each line clamped to the chord with a 1U margin; it draws at `lx + (col_x - col_x_final)` so it still slides in during the split. STACK_R blink keeps the fold (drift applied per row). Chalk stacked rows: outer 50 / inner 72.

**4. Transitions.** `prv_round_split_draw` lerps each digit individually (`prv_lerp_v` / `prv_lerp_h`). V phase: full-view x kept, height and centre move to the digit's own stacked row for its full-view column. H phase: x slides to the stacked column and height re-fits to that column. Merge reverses both. Continuous at the phase edges.

**5. Gating removed.** The `PBL_ROUND` early return in `accel_tap_handler` and the `#if !defined(PBL_ROUND)` guard around the INFO_MODE_ALWAYS switch in `init` are gone.

**6. New helpers** (inside `#if defined(PBL_ROUND)`): `prv_isqrt` (Newton; the Pebble SDK has no `sqrt`), `prv_round_chord_half`, `prv_round_bound`, `prv_round_row_h`, `prv_round_row_cy`, `prv_round_wide_h`, `prv_round_draw_digit4`, `prv_round_stacked_draw`, `prv_round_split_draw`. v3.70 adds `prv_round_fit` and `prv_view_bounds`. `prv_draw_stacked_info` gained a `col_x_final` parameter (non-round code path unchanged).

**Verified (geometry only, in a glyph-port simulation):** worst clearance over digits 0–9 incl. split/merge frames — chalk full 1.15U, wide 0.98U, stacked L 1.26U / R 1.31U. Chalk wide is 0.02U under the 1U rule of thumb. **Not verified:** the real SDK build, any on-device rendering, and how Gothic text sits against the chord clamps.

### v3.66 changes (2026-10-14) — NOT YET COMPILED OR DEVICE-TESTED
**Chalk colon spacing**, the same fix as gabbro's v3.65. Sterling asked for chalk to follow the same rule after the gabbro fix. Chalk's slots were a uniform 30px pitch (5 UNIT at UNIT 6), which put the colon dot 15px (2.5 UNIT) from each neighboring glyph instead of 6px (1 UNIT).
Fix: chalk slots moved to `H_TENS 24 | H_ONES 54 | colon 75 | M_TENS 96 | M_ONES 126`, `SIDE_MARGIN 24`. Digit-slot → colon-slot pitch is 21px (3.5 UNIT). Glyphs: H_TENS 27..51, H_ONES 57..81, dot 87..93, M_TENS 99..123, M_ONES 129..153. Colon center is still x=90, and the glyph span 27..153 is centered too.

### v3.65 changes (2026-10-14) — NOT YET COMPILED OR DEVICE-TESTED
**Gabbro colon spacing.** Sterling saw v3.64 on a gabbro and reported the digit-to-colon space looked about twice the usual design. Cause: v3.64 laid gabbro's five slots out at a uniform `SLOT_W` (40px) pitch, so the colon slot was a full slot wide on each side. Emery's design uses a 28px (3.5 UNIT) pitch from the inner digit slot to the colon slot, which puts the dot 8px (1 UNIT) from each neighboring glyph. v3.64 put it 20px away (2.5 UNIT), and the glyph-to-glyph gap across the colon was 6 UNIT instead of 3.
Fix: gabbro slots moved to `H_TENS 42 | H_ONES 82 | colon 110 | M_TENS 138 | M_ONES 178`, `SIDE_MARGIN 42`. Colon center is still x=130. (Superseded by v3.68's 2U layout, then v3.70's 1.5U layout.)

### v3.64 changes (2026-10-13)
1. **Gabbro split from chalk.** Gabbro (Pebble Round 2) is a 260×260 round screen; it had been sharing chalk's 180×180 geometry block (and was mislabeled "Pebble 2 HR"). It now has its own block: `UNIT=8` (same glyph scale as emery), slots centered on x=130 (positions superseded by v3.65, v3.68, v3.70 — see Architecture). Its round heights were scaled from chalk's proportions at the time; **v3.67 replaced them with measured values.**
2. **Heart rate fixed.** `prv_update_health()` used `health_service_metric_accessible()` for `HealthMetricHeartRateBPM`, which always reports HR unavailable. It now uses `health_service_metric_aggregate_averaged_accessible(metric, now, now, HealthAggregationAvg, HealthServiceTimeScopeOnce)` — the same fix that made HR work in Radium 2.

### Known active bugs
None logged. (v3.65 through v3.70 changes above are unverified on device.)

### Unresolved questions
- STACK_L animation (symmetric squish) vs STACK_R animation (fold toward center) — preference pending device comparison
- Round stacked: does the chord-clamped info column look right against the bezel on both round watches, and are 5 (chalk) / 6 (gabbro) lines the right cap?
- Round wide: is 1 line per side enough on chalk, and does Gothic text sit well inside the chord clamps?
- Chalk wide clearance is 0.98U (just under the 1U rule of thumb); gabbro wide "1+1" is 0.95U at 1.5U spacing — confirm both on device
- Chalk digit spacing: stay at 1U, or widen to 1.5U (outer digits ≈94px instead of 108) to match gabbro? 2U is not feasible (outer 72). Sterling to decide (earlier, 2026-10-14: "prolly best left alone for now")
- Gabbro 1.5U (v3.70): do the 158 / 224 full-view digits and the 74 / 108 stacked rows look right on device?
- Round Quick Look (v3.70): is "tops pinned, bottoms shortened" in full mode the right behavior on hardware, and is the small jump when a split/merge starts during a peek acceptable? Extend the same pinning to rect wide/stacked if wanted
- Whether the HR slot should hide (rather than show "--") on platforms with no HR sensor (basalt, chalk, gabbro, flint, aplite) — currently shows "--"
- Pebble Plugins preview (announced 2026-10-23): will the existing Open-Meteo/location calls in `index.js` trigger the new per-face permission prompts, and does the built-in Weather plugin expose UV/AQI/sunrise? Not tested; see the Codex plugins note

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
│   ├── main.c              ← entire watchface (~115KB); single-file architecture
│   └── pkjs/
│       └── index.js        ← PebbleKit JS: weather/UV/solar/AQI fetch + config page
├── appinfo.json            ← app metadata, message keys, target platforms
├── PROJECT_CONTEXT.md      ← this file
├── README.md               ← human-facing overview
├── STORE_LISTING.md        ← DRAFT store listing (unreleased; added 2026-10-24)
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
- The Pebble SDK has **no `sqrt`** (use an integer Newton routine like `prv_isqrt`)

### AI collaborator rules (CRITICAL)
- **ALWAYS use `create_or_update_file` with the current file SHA** — NEVER use `push_files` (confirmed bug: sends empty content, silently zeroing the file)
- Always pass the **full 40-character SHA**; a shortened SHA is rejected with a "does not match" conflict
- Always commit **full file content** — never partial diffs or patches
- **Always fetch the current SHA** immediately before any update — SHAs from prior sessions are stale after any intervening commit
- `get_file_contents` on `main.c` (~115KB) overflows the tool-result size limit and is saved to a file instead; the SHA can be read from that file with grep
- After pushing a large file, verify it: compute the git blob hash locally (`git hash-object file`, or `printf 'blob %s\0' <bytes> | cat - file | sha1sum`) and compare with the SHA the API returns — they match only if the upload is byte-identical (v3.67's 109,988-byte and v3.70's 114,694-byte `main.c` matched exactly, though one dropped blank line has happened once; if hashes differ, fetch the remote and diff)
- File "deletion" via MCP produces a stub file, not a true delete — actual deletion requires the GitHub web UI trash icon
- Syntax-check without the SDK: compile with `gcc -fsyntax-only` against a stub `pebble.h` per platform define (gabbro, chalk, emery, basalt). Errors about `MESSAGE_KEY_*`, `UnobstructedAreaHandlers` and "invalid initializer" are artifacts of the stub, not real bugs

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
- Round platforms (chalk, gabbro): share the `PBL_ROUND` code paths but have **separate geometry blocks** (chalk 180×180 UNIT 6; gabbro 260×260 UNIT 8); all three layouts (full, wide, stacked L/R) are supported on round since v3.67; gabbro full-view uses 1.5 UNIT digit spacing since v3.70 (2 UNIT in v3.68–v3.69)
- Touch input: **gabbro has a touchscreen (Sterling, 2026-10-14: "def has touch")**, as does emery. TallBoy does not use it: an earlier finding that touch was non-functional at firmware level on shipping hardware led to all `PBL_TOUCH` code being removed. That finding predates the confirmation for gabbro, so re-check it before assuming touch is unusable on either watch

### Digit geometry (stable)

```
UNIT = 8 (emery, gabbro) / 6 (all others)
SLOT_W       = UNIT * 5     — digit slot width (includes padding); gabbro: 44 since v3.70
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

`draw_digit_vec` positions the top and bottom edges at `cy ∓ h/2`, so **heights should be even** (an odd height loses 1px of symmetry). A glyph is **4 UNIT wide**: a "0" is a 1-UNIT wall, a 2-UNIT opening, and a 1-UNIT wall (stroke `sw` = 1 UNIT, inner arc radius `ri` = 1 UNIT, outer `ro` = 2 UNIT). Neighboring glyphs are 1 UNIT apart (slot pitch 5 UNIT = 4 glyph + 1 gap) on every platform except gabbro full-view, which uses a 1.5 UNIT gap (slot pitch 5.5 UNIT = 44px) since v3.70. Sterling described digits as "3 units wide" on 2026-10-14 (wall, space, wall); that does not match the code, and nothing was changed for it — if he wants 3-UNIT digits it is a redesign of the arc caps, not a spacing tweak.

Digits 1, 2, and 7 use `gpath_create` for diagonal strokes — these allocate/free on every draw call. Null checks are in place; not a memory leak.

### Slot geometry rule (all platforms)

A glyph is drawn at `slot_x + HALF_SLOT_PAD` (gabbro v3.70: `slot_x + 4`) and is `GLYPH_W` wide; the colon dot (radius `UNIT/2`, 1 UNIT wide) is centered at `COLON_SLOT_X + SLOT_W/2`. Digit-to-digit pitch is `SLOT_W` (5 UNIT). **The pitch from the inner digit slot to the colon slot is 3.5 UNIT** (not a full `SLOT_W`), which puts the dot 1 UNIT from each neighboring glyph and the glyph-to-glyph gap across the colon at 3 UNIT. Apply this to any new platform. **Exception: gabbro full-view uses 1.5 UNIT gaps everywhere since v3.70** (slot pitch 5.5 UNIT, digit-to-colon pitch 30px — see below); chalk and emery still follow the 3.5 UNIT rule.

### Slot positions — emery (UNIT=8, SLOT_W=40) — the reference

```
SLOT_H_TENS=12, SLOT_H_ONES=52, COLON_SLOT_X=80, SLOT_M_TENS=108, SLOT_M_ONES=148
Digit slot → colon slot pitch = 28px (3.5 UNIT)
Glyphs: H_ONES 56..88, colon dot centered at x=100 (r=4), M_TENS 112..144  → dot is 8px from each glyph
```

### Slot positions — chalk (UNIT=6, SLOT_W=30, centered on 180px) — v3.66/v3.67, unverified on device

```
SLOT_H_TENS=24, SLOT_H_ONES=54, COLON_SLOT_X=75, SLOT_M_TENS=96, SLOT_M_ONES=126
SIDE_MARGIN=24 (used by the stacked layouts since v3.67)
Digit slot → colon slot pitch = 21px (3.5 UNIT). Glyphs: H_TENS 27..51, H_ONES 57..81, dot 87..93, M_TENS 99..123, M_ONES 129..153
Colon center = 75 + 15 = 90 = exact screen center ✓; glyph span 27..153 is centered ✓
ROUND_OUTER_H = UNIT*18 = 108px  — H_TENS and M_ONES columns (glyph far edge dx=63 from center)
ROUND_INNER_H = UNIT*25 = 150px  — H_ONES and M_TENS columns (v3.67; was 144)
ROUND_INFO_INSET = 11, ROUND_WIDE_LINES = 1, ROUND_STACK_LINES = 5
Stacked rows: outer column 50px, inner column 72px
cy on round   = ub_top + ub_h/2  — NOT MARGIN_OUTER + s_target_h/2 (6px off-center)
Superseded (pre-v3.66, uniform 30px pitch, dot 2.5 UNIT from glyphs): 15 / 45 / colon 75 / 105 / 135, SIDE_MARGIN 15
Wider-spacing options (1.5U: outer ≈94 / inner 152; 2U: outer 72) are analysed in the v3.68 section; chalk stays at 1U pending Sterling's call
```

### Slot positions — gabbro (UNIT=8, SLOT_W=44, centered on 260px) — v3.70, unverified on device

```
SLOT_H_TENS=34, SLOT_H_ONES=78, COLON_SLOT_X=108, SLOT_M_TENS=142, SLOT_M_ONES=186
SIDE_MARGIN=42 (unused on gabbro since v3.69; stacked layouts use the SLOT_* x positions and STACK_SLOT_W = SLOT_W)
Colon center = 108 + 22 = 130 = exact screen center ✓; glyph span 38..222 is centered ✓
Every gap is 1.5 UNIT (12px): glyph | 12 | glyph | 12 | dot 126..134 | 12 | glyph | 12 | glyph
Glyphs: H_TENS 38..70, H_ONES 82..114, dot 126..134, M_TENS 146..178, M_ONES 190..222
Outer glyph edge dx=92 from center; inner dx=48
ROUND_OUTER_H = 158px  — H_TENS and M_ONES columns, full view and wide   (v3.70; was 136 at 2U, 176 at 1U)
ROUND_INNER_H = UNIT*28 = 224px  — H_ONES and M_TENS columns, full view and wide   (v3.70; was 216 at 2U)
ROUND_STACK_OUTER_H = ROUND_OUTER_H (158), ROUND_STACK_INNER_H = ROUND_INNER_H (224); ROUND_STACK_INFO_H = UNIT*22 = 176 for the info column
ROUND_INFO_INSET = 13, ROUND_WIDE_LINES = 2, ROUND_STACK_LINES = 6
Stacked rows (same bounds as full view): outer column 74px, inner column 108px; info column x = ones_x + 48
Heights are measured against the bezel (tb/gapscan.py, ≥1U corner clearance), not scaled.
Quick Look (v3.70): full mode pins the tops and shortens the bottoms via prv_round_fit; wide, stacked and transitions use the full-screen bounds (prv_view_bounds)
Superseded: v3.68–v3.69 slots 26 / 74 / colon 106 / 146 / 194 (2U gaps, SLOT_W 48, heights 136 / 216); v3.65–v3.67 slots 42 / 82 / colon 110 / 138 / 178 (1U gaps, SLOT_W 40, SIDE_MARGIN 42); v3.64 slots 30 / 70 / colon 110 / 150 / 190 (too airy)
```

### Slot positions — other rect (basalt/diorite/flint/aplite, UNIT=6, SLOT_W=30)

```
SLOT_H_TENS=6, SLOT_H_ONES=36, COLON_SLOT_X=57, SLOT_M_TENS=78, SLOT_M_ONES=108
Digit slot → colon slot pitch = 21px (3.5 UNIT) — already follows the rule
```

### Margin model

```
MARGIN_CANVAS = UNIT        — screen edge → bg rounded-rect edge
MARGIN_DIGIT  = UNIT        — bg edge → digit content
MARGIN_OUTER  = UNIT * 2    — convenience alias

QUICK_LOOK_ACTIVE(ub_h) = (ub_h < SCREEN_H - UNIT)
BOTTOM_MARGIN(ub_h)     = QUICK_LOOK_ACTIVE ? MARGIN_DIGIT : MARGIN_OUTER
```

On round (v3.70) `prv_view_bounds` returns the full screen, so only full-time mode reacts to Quick Look, by shortening column bottoms (`prv_round_fit`). Rect still fits the whole face to the unobstructed area.

### Layout system

```c
LAYOUT_FULL    = 0  // full-screen time; anticipate → squish → expand each minute
LAYOUT_INFO    = 1  // wide mode; up to 3 info slots above + 3 below time (round: 1–2 short lines per side)
LAYOUT_STACK_R = 2  // digits right, info column left (right-aligned text)
LAYOUT_STACK_L = 3  // digits left, info column right (left-aligned text)
```

Transitions to stacked: `PHASE_SPLIT_V` → `PHASE_SPLIT_H`
Transitions to full: `PHASE_MERGE_H` → `PHASE_MERGE_V`

Wide blink: fast linear drop/rise, no overshoot, single rep, digit swap at minimum.
Stacked blink: STACK_L = symmetric squish; STACK_R = fold toward center (hours top pinned, minutes bottom pinned).

On round, the stacked layouts and the split/merge phases do not use the shared `draw_pair` path; they use per-column rows (`prv_round_stacked_draw`, `prv_round_split_draw`) because the outer and inner columns have different heights. The rect path is untouched.

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

On round, wide mode uses the short format (`wide=false`), so the wide-only slots (18, 19, 20, 24, 25, 30) render nothing there.

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
| **Uniform slot pitch makes the colon gap too wide** | Spacing slots at a full `SLOT_W` on both sides of the colon puts the dot 2.5 UNIT from each glyph instead of 1 (gabbro v3.64, chalk until v3.66). Use 3.5 UNIT between the inner digit slots and the colon slot (gabbro now deliberately uses 1.5U gaps). Positions must be checked on device, not just derived from "centered and evenly spaced" |
| **Scaling round heights from another screen** | Chalk's proportions were scaled to gabbro in v3.64 and left it short. Round heights should be measured against the bezel with the real glyph geometry (worst lit pixel over digits 0–9), not eyeballed or scaled |
| **Wider digit spacing costs round height** | On a round screen the outer columns move toward the bezel as spacing grows: gabbro outer height 176 (1U) → 158 (1.5U) → 136 (2U); chalk 1.5U would cost ≈14px and 2U about 36px. Re-measure heights with `tb/gapscan.py` whenever slot positions change; the stacked layouts on gabbro inherit the same bounds (`ROUND_STACK_*_H` = full-view values), so one measurement covers both; `ROUND_STACK_INFO_H` stays separate |
| **Round Quick Look must not move wide/stacked** | On round, `prv_view_bounds` deliberately returns the full screen; only full-time mode shortens column bottoms (`prv_round_fit`). Do not route wide, stacked or split/merge through the unobstructed bounds |
| **Odd digit heights** | `draw_digit_vec` uses `h/2` on both sides; an odd `h` is 1px asymmetric. Stacked round row heights are forced even with `& ~1` |
| **No `sqrt` in the Pebble SDK** | Use `prv_isqrt` (integer Newton) for chord math |
| Round `cy` must be `ub_top + ub_h/2` | `MARGIN_OUTER + s_target_h/2` is off-center on round screens |
| Wide mode draw height | Use `actual_h` (live-computed from rendered slot count), not `s_target_h` |
| Stacked blink floor | Uses `H_ABSOLUTE_MIN`, not `H_MIN` |
| Stacked `s_h` at rest | Not meaningful at rest — use `prv_compute_stacked_h(ub_h)` directly (on round it returns the tallest row, the inner-column row height) |
| CloudPebble import with duplicate sources | Delete conflicting files before re-importing |
| Tilde in resource filenames | Breaks CloudPebble GitHub import |
| MCP file deletion produces stub | Actual deletion requires GitHub web UI trash icon |

---

## Current TODO

### Immediate
- **Compile v3.70 (carries v3.65–v3.69) in CloudPebble and device-test it on chalk and gabbro**: full-view heights (chalk 108/150, gabbro 158/224 at 1.5U spacing; stacked rows on gabbro 74/108), wide (1 line per side on chalk, 2 on gabbro), stacked L and R, the minute blink in each layout, and the split/merge animations. Check for bezel clipping and for Gothic text against the chord clamps
- Test Quick Look on round: full mode should keep the tops fixed and shorten only the bottoms; wide and stacked should not move at all
- The same build also carries v3.65 (gabbro) and v3.66 (chalk) colon spacing: check the gabbro gaps (12px each) and that the stacked layouts sit on the full-view digit positions
- Confirm heart rate shows a real BPM on emery (v3.64 fix)
- Device-test the v3.62 changes still unconfirmed: pace palettes, colon color, sun icon, Temp+UV+AQI combo (AQI itself is confirmed working)
- Decide the default pace palette (code default: medium)
- Decide STACK_L vs STACK_R animation preference after device comparison

### Near-term
- Lower-resolution platform tuning (basalt/aplite/diorite/flint) — layout math scales with UNIT; needs device testing
- Config page — exists in `src/pkjs/index.js`; further polish as needed (consider noting that round wide shows short lines only)
- Consider hiding the HR slot on platforms with no HR sensor instead of showing "--"
- Attribution: CAMS/Open-Meteo acknowledgement in store/README copy before any public release (the draft `STORE_LISTING.md` already carries the line; keep it in sync)
- Review and finish `STORE_LISTING.md` (draft added 2026-10-24: original-work framing; first-release note, screenshots and banner still to do)
- Pebble Plugins preview (announced 2026-10-23): test on the beta app whether the existing location/Open-Meteo calls now trigger permission prompts; do not publish a plugin-using build while the API is in preview

### Backlog
- Wider digit spacing on chalk (1.5U) — see v3.68 trade-off table; gabbro is at 1.5U since v3.70
- Possibly extend round-style Quick Look pinning to rect wide/stacked if wanted
- Icon redesign pass — Sterling sketches at 4× in Photoshop; AI translates to C vector drawing calls
- Step browser companion app — discussed, not started
- Revisit a Monogram-style fit-to-circle only if digit bounds can be pushed dynamically to fit the circle better; per the 2026-10-24 decision, branch Monogram off TallBoy rather than reworking TallBoy

---

## Verification Plan

Before committing any version:

1. Code compiles on emery (primary target)
2. No geometry regressions — digit height, colon position, info line spacing
3. Wide mode slot collapse works correctly when data is absent vs. present
4. Stacked mode minute-change blink animation fires correctly
5. For round: outer digits (H_TENS, M_ONES) are visibly shorter than inner (H_ONES, M_TENS); no bezel clipping — check on **both** chalk and gabbro, they have separate geometry; check full, wide and both stacked layouts plus the split/merge transitions; with Quick Look active, full mode shortens only the bottoms and wide/stacked stay put
6. Heart rate shows a live BPM (not `--`) on emery
7. Digit-to-colon gap on any new or changed platform matches emery's (dot 1 UNIT from each glyph) — except gabbro full-view, which is 1.5 UNIT (12px) on every gap since v3.70
8. SHA fetched fresh immediately before `create_or_update_file`; large pushes verified by blob hash

---

## Source of Truth / External Links

| Resource | URL |
|---|---|
| Pebble SDK docs (primary) | https://developer.repebble.com |
| Pebble plugins guide (preview) | https://developer.repebble.com/guides/plugins/ |
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
| Radium2 | https://github.com/SterlingEly/Radium2 | Sibling watchface; radial time display with weather, health, UV/AQI and pace features; the most mature released project in the portfolio. Radium 2's pace color and info-line set were ported from TallBoy; TallBoy's round wide mode (short lines only, few per side) follows Radium's approach |
| BarGraph2 | https://github.com/SterlingEly/BarGraph2 | Sibling watchface; time as vertical bar graphs; rebuild of the original 2013 Pebble concept |
| Monogram | https://github.com/SterlingEly/Monogram | **Shelved 2026-10-24.** Concept: custom monogram-style digits filling the circle. TallBoy on round delivers about 90% of it, so Monogram was shelved (repo kept). If a fit-to-circle technique is ever pursued, Monogram would be branched off TallBoy |
| PixelSampler | https://github.com/SterlingEly/PixelSampler | Developer reference tool; samples fonts, colors, and hardware capabilities across Pebble platforms |

TallBoy shares platform constants, build rules, and CloudPebble conventions with all of the above. Radium2 is the most complete reference for PebbleKit JS weather/solar fetch patterns and for the heart-rate API quirk.

---

## Last Updated

2026-10-24 — doc pass (no code change; `main.c` is still v3.70)
Updated by: AI collaborator (Claude, Anthropic) with direction from Sterling Ely
Changes in this pass: Monogram recorded as shelved in favor of TallBoy (Purpose provenance note, Related Projects, backlog); DRAFT `STORE_LISTING.md` added and listed in the repo tree; plugins-preview note added to unresolved questions, near-term TODO and external links; AI rule added to pass the full 40-character SHA.

Previous pass: v3.70 — 2026-10-16. v3.70 documented (gabbro spacing 2U → 1.5U with slots 34/78/108/142/186, heights 158/224, stacked rows 74/108; round Quick Look now pins the tops in full mode and ignores the peek in wide and stacked); gabbro slot section, traps, TODO, unresolved questions and verification plan refreshed; v3.68/v3.69 sections marked as superseded where they conflict.

Earlier: v3.69 (2026-10-16) gabbro stacked layouts follow the full-view spacing and slots; v3.68 (gabbro 2U spacing, re-measured heights, chalk 1.5U/2U trade-off table, font reference facts); v3.67 2026-10-14 (measured round heights, wide and stacked L/R on round, per-digit split/merge, new helpers); v3.66 2026-10-14 (chalk colon spacing); v3.65 2026-10-14 (gabbro colon spacing); v3.64 2026-10-13 (gabbro split from chalk, HR fix, AQI device-confirmed, platform table naming); v3.63 doc sync 2026-09-28; v3.61 — July 2026: added Related Projects section, reorganized Build Rules, updated status.
