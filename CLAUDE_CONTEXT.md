# TallBoy — Claude AI Session Context

This file is the authoritative handoff document for AI-assisted development sessions.
Read this file completely before making any changes. It reflects the current state of the codebase.

---

## PROJECT OVERVIEW

**TallBoy** is a Pebble watchface by Sterling Ely, inspired by Radium2.
Design tenet: *Watchfaces display data as atmosphere, not content. Artfully vague > precisely readable. Color = mood. Position = direction.*

- **Repo:** `SterlingEly/TallBoy`, branch `main`
- **Primary device:** Pebble Time 2 (platform: `emery`)
- **All target platforms:** aplite, basalt, diorite, emery, flint, gabbro
- **SDK/Docs:** developer.repebble.com is the PRIMARY source of truth (Pebble is officially back). Rebble docs are secondary.
- **CloudPebble** is used for building + pushing to device. Code lives in GitHub and is imported fresh into CloudPebble each version.

## ROLE SPLIT

- **Sterling** = design decisions, CloudPebble builds, device testing
- **Claude** = code writing + GitHub commits

## CRITICAL GITHUB RULES

- ALWAYS use `create_or_update_file` with the current file SHA, NOT `push_files` (push_files sends empty content — known CloudPebble bug)
- `src/main.c` is the FLAT path (CloudPebble convention — no subdirectory)
- Always commit FULL files, never partial diffs
- Each new version = new full CloudPebble import as new project, sent to watch via BT/WiFi

---

## CURRENT STATUS: v3.47e (HEAD)

### What's Working
- Builds clean on all 6 platforms (gabbro, flint, emery, diorite, basalt, aplite)
- App launches and runs on device (no "not responding" crash)
- Animated digit rendering (squeeze/expand)
- Layout cycling on shake: FULL → INFO → STACK_L → STACK_R → FULL
- Pace-based background color spectrum (emery/color/health only)
- Info lines render (wide mode center-aligned, stacked mode side-aligned)
- Config page opens (settings gear shows)
- Weather + solar fetch via open-meteo API

### Known Issues / In Progress
- **Touch** still not firing on emery/gabbro despite all known API fixes applied
  - 50ms haptic vibe is the diagnostic (should vibrate on screen touch)
  - All known fixes applied: touch_service_is_enabled() guard, TouchEvent_Touchdown filter, unconditional unsubscribe, real no-op button bindings in click_config_provider
  - pebble-calculator confirmed working on same device — our implementation mirrors it exactly
  - Mystery remains: possibly a CloudPebble SDK version difference or device-specific quirk
- **Config page drag-to-reorder** — drag gestures don't work in Pebble's embedded webview (drag events don't fire). Replaced with up/down arrow buttons in v3.47e.
- **Wide mode auto-combos** — currently implemented but flagged for removal/simplification in next design pass per Sterling's feedback (too clever, confusing for users)

### Design Changes Planned (Not Yet Implemented)
- Info display mode setting: always on / always off / toggle on shake / shake-shows-for-1min
- Info layout setting: wide / stacked-left / stacked-right (instead of cycling through all)
- Separate slot content configs for wide vs stacked
- Wide mode: 6-7 predefined combo lines (no auto-combining)
- Stacked mode: up to 8 individual data slots
- Color mode setting: pace-based vs custom solid color
- Remove auto-combo logic from wide mode rendering

---

## ARCHITECTURE

### Slot Types (SlotType enum, main.c)
```c
SLOT_EMPTY=0, SLOT_DAY=1, SLOT_DATE=2, SLOT_DAY_DATE=3,
SLOT_TEMP=4, SLOT_WEATHER=5, SLOT_STEPS=6, SLOT_DISTANCE=7,
SLOT_EXP_STEPS=8, SLOT_PACE=9, SLOT_CALORIES=10, SLOT_HEART=11,
SLOT_SUNRISE=12, SLOT_SUNSET=13, SLOT_DAYLIGHT=14,
SLOT_BATTERY=15, SLOT_BLUETOOTH=16
TIME_MARKER=17  // special: marks time position in order array
```

### Layout Constants
```c
LAYOUT_FULL=0    // full-screen digits, no info
LAYOUT_INFO=1    // wide info: lines above+below centered time
LAYOUT_STACK_R=2 // 2-digit stacked right, info column left-aligned
LAYOUT_STACK_L=3 // 2-digit stacked left, info column right-aligned
```
Shake cycles through layouts. Touch cycles corner radius (emery only, diagnostic).

### Config Order Array
`s_cfg_order[7]` — ordered list of 7 items (6 data slots + TIME_MARKER at position indicating where time sits).
Default: `{ 1, 2, 3, 17, 4, 5, 6 }` = day, date, day+date | TIME | steps, pace, battery
Packed into persist key 1 as 5-bit fields (7×5=35 bits, fits in int32).

### Platform Constants (emery vs all others)
```
EMERY:  SCREEN=200×228, UNIT=8, ICON_W=14, INFO_FONT=GOTHIC_24_BOLD
OTHERS: SCREEN=144×168, UNIT=6, ICON_W=11, INFO_FONT=GOTHIC_18_BOLD
```

### Persist Keys
```c
PERSIST_CFG_ORDER = 1   // int32, 7 items packed 5-bit each
PERSIST_CFG_FLAGS = 2   // bit0=temp_f, bit1=dist_mi, bit2=24h
```

### Animation Phases
```
PHASE_DONE → PHASE_ANTICIPATE → PHASE_SQUISH → PHASE_EXPAND → PHASE_DONE
PHASE_COUNTDOWN (unused but preserved)
PHASE_BLINK (end of minute animation)
PHASE_SHAKE_CYCLE (shake feedback)
```
EASE[10] = {4,6,8,10,12,12,10,8,6,4} applied to expand/shrink steps.
ANIM_STEP_MS=16, ANIM_SNAP_MS=80 (overshoot hold), ANIM_OVERSHOOT=UNIT.

### Stacked Layout Geometry
```
LAYOUT_STACK_R: digits on RIGHT side
  ones_x = SCREEN_W - SIDE_MARGIN - SLOT_W
  tens_x = ones_x - SLOT_W
  info spans: SIDE_MARGIN to (tens_x - SIDE_MARGIN)
  info alignment: GTextAlignmentLeft (left-aligned in left column)

LAYOUT_STACK_L: digits on LEFT side  
  tens_x = SIDE_MARGIN
  ones_x = SIDE_MARGIN + SLOT_W
  info spans: (ones_x + SLOT_W + SIDE_MARGIN) to (SCREEN_W - SIDE_MARGIN)
  info alignment: GTextAlignmentRight (right-aligned butting against digits)
```
Info lines span the full available width, text aligned toward digits.

### InfoLine Static Globals (CRITICAL — must stay static)
```c
static InfoLine s_above_lines[INFO_LINES_MAX];  // wide mode above time
static InfoLine s_below_lines[INFO_LINES_MAX];  // wide mode below time
static InfoLine s_col_lines[INFO_LINES_MAX];    // stacked mode column
```
These MUST be static globals (not local). Local allocation = ~1.3KB stack = crash.
Watchface stack is only ~2KB.

### Icons (pixel-drawn, from Radium2)
Two sizes: small=11px (GOTHIC_18_BOLD cap height), large=14px (GOTHIC_24_BOLD).
Set: footprint/steps, battery (charging bolt), BT rune+dot, heart, calories/flame,
sun, cloud, partly-cloudy, rain, snow, storm.
`icon_weather()` maps Open-Meteo WMO codes to icon functions.

---

## KNOWN BUG HISTORY (for reference)

1. `"108%"` → must be `"108%%"` in non-health snprintf fallbacks
2. Health state vars (`s_steps`, etc.) must be `#if defined(PBL_HEALTH)` guarded
3. `prv_fmt_dist` / `prv_fmt_steps` must be `#if defined(PBL_HEALTH)` guarded
4. `UnobstructedAreaHandlers ua` — scope in block `{ ... }` to suppress unused-var on frozen SDK
5. `-Werror=misleading-indentation` — all `else` must be braced, even single-statement
6. `push_files` sends empty content — always use `create_or_update_file` with SHA
7. InfoLine arrays on stack = stack overflow crash = "not responding"

---

## MESSAGE KEYS (appinfo.json ↔ index.js ↔ main.c)

```json
"WeatherTempF":   0,
"WeatherTempC":   1,
"WeatherCode":    2,
"SunriseTime":    3,
"SunsetTime":     4,
"CfgSlot1":       5,   // was 6; CfgTimePos (key 5) removed, slot1 shifted
"CfgSlot2":       6,
"CfgSlot3":       7,
"CfgSlot4":       8,
"CfgSlot5":       9,
"CfgSlot6":       10,
"CfgSlot7":       11,
"CfgTempUnit":    12,
"CfgDistUnit":    13,
"CfgClockFormat": 14
```
**NOTE:** appinfo.json currently has CfgSlot1=6 (old numbering with CfgTimePos=5).
index.js sends CfgSlot1..CfgSlot7. main.c reads MESSAGE_KEY_CfgSlot1 + i for i in 0..6.
All three must agree. Next appinfo.json update should remove CfgTimePos and renumber.

---

## WEATHER DATA SOURCE

Open-Meteo (free, no API key): `https://api.open-meteo.com/v1/forecast`
- `current=temperature_2m,weather_code`
- `daily=sunrise,sunset`
- `temperature_unit=celsius`, `timezone=auto`
Updated every 30 minutes from JS side. Sunrise/sunset sent as Unix timestamps.

### WMO Weather Code → Icon Mapping
```
0          → sun (clear)
1-3        → partly_cloudy
4-48       → cloud (overcast/fog)
51-69,80-82 → rain
71-77,85-86 → snow
95+        → storm
```

---

## PACE COLOR SPECTRUM (emery + color + health)

```
0 / no data → GColorBlack (invisible = bg)
1-30%       → GColorRed
31-60%      → GColorOrange
61-90%      → GColorYellow (dark fg needed)
91-200%     → GColorGreen  (dark fg needed)
201-400%    → GColorBlue
401-700%    → GColorIndigo
701-1000%   → GColorVividViolet
1001%+      → GColorBlack (invisible again — original touch diagnostic problem)
```
`prv_bg_needs_dark_fg()` returns true for Yellow and Green.

---

## STEP PACE CALCULATION

7-day historical average of steps at current time-of-day, using `health_service_get_minute_history`.
Window: min(elapsed_minutes_today, 120 minutes). Returns -1 if < 2 minutes elapsed.

---

## TOUCH API STATUS

**Platform:** `PBL_TOUCH` fires on emery and gabbro.
**Current implementation** (mirrors pebble-calculator exactly):
```c
#if defined(PBL_TOUCH)
static void touch_handler(const TouchEvent *event, void *context) {
  if (event->type != TouchEvent_Touchdown) return;
  vibes_enqueue_custom_pattern(TOUCH_VIBE);  // 50ms diagnostic vibe
  s_radius_idx = (s_radius_idx + 1) % RADIUS_COUNT;  // emery only
}
#endif
// In window_load:
if (touch_service_is_enabled()) { touch_service_subscribe(touch_handler, NULL); }
// In window_unload:
touch_service_unsubscribe();  // unconditional
// In init() before window_stack_push:
window_set_click_config_provider(s_window, prv_click_config_provider);
// prv_click_config_provider binds real no-op handlers to UP/SELECT/DOWN
```
**Still not working.** All known API fixes applied. pebble-calculator works on same device.
Diagnostic vibe has never fired. Cause unknown.

---

## CONFIG PAGE

Opens via Pebble settings gear (requires `"configurable"` in capabilities).
Built as a `data:text/html,...` URL opened via `Pebble.openURL()`.
Closes via `location.href = 'pebblejs://close#' + encodeURIComponent(JSON.stringify(settings))`.

**v3.47e config page has:**
- 7 rows with up/down arrow buttons for reordering (drag API doesn't work in Pebble webview)
- Each row has a `<select>` for slot type (includes Time as an option)
- Temp unit (F/C), Distance unit (mi/km), Clock format (12h/24h) radio toggles
- Save button

**v3.47e sends:** `CfgSlot1..CfgSlot7` (full ordered list), `CfgTempUnit`, `CfgDistUnit`, `CfgClockFormat`

---

## FILES IN REPO

```
src/main.c          — main watchface code (v3.47e)
src/pkjs/index.js   — JS companion: weather fetch + config page
appinfo.json        — app metadata, capabilities, resources, message keys
resources/images/   — digit PNGs (legacy, not used by current vector rendering)
CLAUDE_CONTEXT.md   — THIS FILE (AI session handoff)
```

**Note:** Resources in appinfo.json (the PNG digit files) are legacy artifacts from when
TallBoy used PNG-based digit rendering. The current code draws all digits as vectors.
The PNGs still need to be declared in appinfo.json or the build fails.

---

## NEXT SESSION TODO (priority order)

1. **Config page redesign** — implement new settings structure:
   - Info display mode: always-on / always-off / shake-toggle / shake-show-1min
   - Info layout mode: wide / stacked-left / stacked-right (replaces layout cycling)
   - Separate wide vs stacked slot configs
   - Wide: 7 predefined combo lines (user picks which to show, with checkboxes)
   - Stacked: 8 individual slot selects
   - Color mode: pace-based vs custom

2. **Remove auto-combo logic** from `prv_slot_text()` — wide combos should be
   predefined slot types, not auto-detected from adjacent slots

3. **Touch** — still unresolved. May need to try different approach:
   - Try subscribing in `init()` instead of `window_load()`
   - Try without the `touch_service_is_enabled()` guard
   - Check if gabbro (round) has the same issue (gabbro also has PBL_TOUCH)

4. **appinfo.json cleanup** — remove `CfgTimePos` key (vestigial), renumber CfgSlot1..7
   starting from key 5 to pack tighter

5. **Stacked layout polish** — verify info line spacing looks right on device,
   tune step size for various slot counts

---

## DESIGN NOTES

- Digits are drawn as vectors using `graphics_fill_radial` for caps and `graphics_fill_rect` for stems
- The "squeeze" animation: digits compress vertically on each minute tick, then expand
- Corner radius (emery only): `s_radius_opts[] = {16, 24, 32}px`, cycles on touch
- Background is always black underneath, colored rect drawn on top with corner radius
- Stacked mode: hours on top half, minutes on bottom half, each pair is 2 digits
- Wide info mode: time centered, info lines above and below
- `SLOT_DAY_DATE` (id=3) is a combined "Mon, May 28" slot — distinct from separate day+date slots
