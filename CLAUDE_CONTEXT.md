# TallBoy — Claude AI Session Context

This file is the **authoritative handoff document** for AI-assisted development sessions.
**Read this file completely before making any changes.** It reflects the current state of the codebase.

---

## PROJECT OVERVIEW

**TallBoy** is a Pebble watchface by Sterling Ely, inspired by Radium2.
Design tenet: *Watchfaces display data as atmosphere, not content. Artfully vague > precisely readable. Color = mood. Position = direction.*

- **Repo:** `SterlingEly/TallBoy`, branch `main`
- **Primary device:** Pebble Time 2 (platform: `emery`, UNIT=8)
- **All target platforms:** aplite, basalt, diorite, emery, flint, gabbro
- **SDK/Docs:** developer.repebble.com is the PRIMARY source of truth (Pebble is officially back). Rebble docs are secondary.
- **CloudPebble** is used for building + pushing to device. Code lives in GitHub; each new version = fresh CloudPebble import.

## ROLE SPLIT

- **Sterling** = design decisions, CloudPebble builds, device testing
- **Claude** = code writing + GitHub commits

## CRITICAL GITHUB RULES

- **ALWAYS** use `create_or_update_file` with the current file SHA — NEVER `push_files` (push_files sends empty content — confirmed bug)
- `src/main.c` is the **flat** path (CloudPebble convention — no `src/c/` subdirectory in the repo)
- Always commit **full files**, never partial diffs
- Get the current SHA before every update (it changes with each commit)

---

## CURRENT STATUS: v3.47d (HEAD)

### What's Working
- Builds clean on all 6 platforms (gabbro, flint, emery, diorite, basalt, aplite)
- App launches and runs on device (no "not responding" crash — fixed in v3.47d by moving InfoLine arrays off stack)
- Animated digit rendering (anticipate → squeeze → expand)
- Layout cycling on shake: FULL → INFO → STACK_L → STACK_R → FULL
- Pace-based background color spectrum (emery/color/health only)
- Info lines render in wide mode (center-aligned above/below time)
- Info lines render in stacked mode — **BUG: all align left regardless of layout** (see known issues)
- Config page opens (settings gear), arrow buttons work for reordering
- Weather + solar fetch via open-meteo API (no key required)

### Known Bugs (as of v3.47d)

**1. Stacked layout info alignment is wrong**
Both STACK_L and STACK_R show info lines aligned left. They should:
- Span from 1–2 UNITs from the screen edge to 1–2 UNITs from the digit column
- Text aligned toward the digit side (left-align for STACK_R info-left-column, right-align for STACK_L info-right-column)
- The `draw_info_line` text path uses `GRect(0, ...)` for the left edge — should use `info_x` as the left origin

**2. Touch not firing on emery/gabbro**
50ms haptic vibe diagnostic has never fired despite all known API fixes. pebble-calculator works on same device. Cause unknown. All fixes applied:
- `touch_service_is_enabled()` guard in window_load
- `TouchEvent_Touchdown` filter in handler
- Unconditional `touch_service_unsubscribe()` in window_unload
- Real no-op button bindings in `prv_click_config_provider`
- `window_set_click_config_provider` called in `init()` before `window_stack_push`

**3. Config page up/down arrows swap visually but select element state is broken**
The `moveRow()` JS function swaps `sel.value` between two `<select>` elements, but the browser may not update the visual selection when you just set `.value`. The DOM `selected` attribute on `<option>` children is what needs updating. See fix approach in NEXT SESSION TODO.

### Design Changes Planned (Full Spec — NOT Yet Implemented)

Sterling's full redesign request for config and display logic:

#### A. Info Display Mode (new setting)
1. Always on
2. Always off
3. Toggle on shake (current behavior — default for now)
4. Shake shows info for 1 minute then auto-hides (future default)

#### B. Info Layout Mode (new setting, replaces shake-cycling through layouts)
1. Wide mode (info lines centered above/below time)
2. Stacked Left (digits left, info column right)
3. Stacked Right (digits right, info column left)

#### C. Info Content — Wide Mode (separate from stacked)
7 predefined combo lines — user picks which to show (checkboxes, no auto-combining):
1. Day & Date
2. Temp & Weather
3. Steps & Distance
4. Expected Steps & Pace %
5. Calories & Heart Rate
6. Sunrise & Sunset
7. Battery & Bluetooth
(possibly more)

#### D. Info Content — Stacked Mode (separate from wide)
Up to 8 individual slot selects from this list:
1. Day (short: "Mon")
2. Date ("May 28")
3. Temperature (F or C)
4. Weather (icon + code description)
5. Steps
6. Distance
7. Expected Steps at current time
8. Pace (% of expected steps)
9. Active Calories
10. Heart Rate
11. Sunrise
12. Sunset
13. Daylight duration
14. Battery %
15. Bluetooth status
(possibly more)

Wide and stacked each retain their own settings independently.

#### E. Color Mode (new setting)
1. Pace-based (current: background color reflects step pace %)
2. Custom solid color (user picks a color from a palette)
(Future: custom per-step-range color mapping)

#### F. Remove auto-combo logic
`prv_slot_text()` currently auto-combines adjacent slots in wide mode (e.g. steps+distance on one line). This is too clever — confusing for users. Wide mode combos should be predefined slot types, not auto-detected.

---

## ARCHITECTURE

### Slot Types (SlotType enum, main.c)
```c
SLOT_EMPTY=0, SLOT_DAY=1, SLOT_DATE=2, SLOT_DAY_DATE=3,
SLOT_TEMP=4, SLOT_WEATHER=5, SLOT_STEPS=6, SLOT_DISTANCE=7,
SLOT_EXP_STEPS=8, SLOT_PACE=9, SLOT_CALORIES=10, SLOT_HEART=11,
SLOT_SUNRISE=12, SLOT_SUNSET=13, SLOT_DAYLIGHT=14,
SLOT_BATTERY=15, SLOT_BLUETOOTH=16
TIME_MARKER=17  // special: marks time position in cfg_order array
```

### Layout Constants
```c
LAYOUT_FULL=0    // full-screen digits, no info lines
LAYOUT_INFO=1    // wide: info centered above + below time
LAYOUT_STACK_R=2 // digits right, info column left (GTextAlignmentLeft)
LAYOUT_STACK_L=3 // digits left, info column right (GTextAlignmentRight)
```
Shake currently cycles: FULL → INFO → STACK_L → STACK_R → FULL.
Touch cycles corner radius (emery only, diagnostic — not working yet).

### Config Order Array
```c
s_cfg_order[7]  // 7 items: 6 data slots + TIME_MARKER
```
Default: `{ 1, 2, 3, 17, 4, 5, 6 }` = day / date / day+date | TIME | steps / pace / battery
TIME_MARKER (17) marks where time sits in the layout.
Packed into persist key 1 as 5-bit fields (7×5=35 bits fits in int32).

### Platform Constants
```
EMERY (Pebble Time 2):  SCREEN=200×228, UNIT=8, ICON_W=14, INFO_FONT=GOTHIC_24_BOLD
ALL OTHERS:             SCREEN=144×168, UNIT=6, ICON_W=11, INFO_FONT=GOTHIC_18_BOLD
```

### Key Derived Constants
```c
SLOT_W       = 30 (non-emery) or 40 (emery)  // pixel width of one digit slot
SIDE_MARGIN  = 6 (non-emery) or 12 (emery)
GLYPH_W      = UNIT * 4
H_MIN        = UNIT * 11
H_ABSOLUTE_MIN = UNIT * 5
INFO_LINE_BUF = 48
INFO_LINES_MAX = 8
```

### Persist Keys
```c
PERSIST_CFG_ORDER = 1   // int32, 7 items packed 5-bit each
PERSIST_CFG_FLAGS = 2   // bit0=temp_f, bit1=dist_mi, bit2=24h
```

### Animation Phases
```
PHASE_DONE → tick → PHASE_ANTICIPATE → PHASE_SQUISH → PHASE_EXPAND → PHASE_DONE
PHASE_BLINK   (end of expand: double blink at new time)
PHASE_SHAKE_CYCLE (shake feedback animation)
PHASE_COUNTDOWN   (unused but preserved in code)
```
EASE[10] = {4,6,8,10,12,12,10,8,6,4} applied to expand/shrink pixel steps.

### Stacked Layout Geometry (CORRECT intent — current impl has bug)
```
LAYOUT_STACK_R (digits RIGHT, info LEFT):
  ones_x    = SCREEN_W - SIDE_MARGIN - SLOT_W
  tens_x    = ones_x - SLOT_W
  info_x    = SIDE_MARGIN          // left edge of info column
  info_w    = tens_x - SIDE_MARGIN - UNIT  // 1 UNIT gap before digits
  info_align = GTextAlignmentLeft  // text left-aligned (away from digits)

LAYOUT_STACK_L (digits LEFT, info RIGHT):
  tens_x    = SIDE_MARGIN
  ones_x    = SIDE_MARGIN + SLOT_W
  info_x    = ones_x + SLOT_W + UNIT  // 1 UNIT gap after digits
  info_w    = SCREEN_W - info_x - SIDE_MARGIN
  info_align = GTextAlignmentRight  // text right-aligned (toward digits)
```
**BUG in v3.47d:** `draw_info_line` uses `GRect(0, y - INFO_TOP_PAD, width, INFO_LINE_H)` —
the `0` should be `info_x` (or `col_x` parameter) and width should be `info_w`.
The `draw_icon_text` path also uses `icon_x = 0` instead of `info_x` for left-aligned case.
Fix: pass `col_x` and `col_w` through `draw_info_line` all the way to the GRect origin.

### InfoLine Static Globals (CRITICAL — must stay static)
```c
// Declared at file scope (NOT inside any function):
static InfoLine s_above_lines[INFO_LINES_MAX];  // wide mode above time
static InfoLine s_below_lines[INFO_LINES_MAX];  // wide mode below time
static InfoLine s_col_lines[INFO_LINES_MAX];    // stacked mode column
```
These MUST be static globals. Stack allocation of ~1.3KB inside draw_layer = crash ("not responding").
Watchface stack is ~2KB total.

### InfoLine Struct
```c
typedef void (*IconFn)(GContext*,int,int,GColor,bool);
typedef struct {
  char   text[INFO_LINE_BUF];  // 48 bytes
  bool   has_icon;
  IconFn icon_fn;
  bool   is_battery;
  bool   is_weather;
  int    icon_extra;           // battery pct or weather code
} InfoLine;                    // ~56 bytes each, 8 per array = 448 bytes
```

### Icons (pixel-drawn, ported from Radium2)
Two sizes: small=11px (GOTHIC_18_BOLD cap height), large=14px (GOTHIC_24_BOLD).
Functions: `icon_steps`, `icon_battery`, `icon_bt`, `icon_heart`, `icon_calories`,
`icon_sun`, `icon_cloud`, `icon_partly_cloudy`, `icon_rain`, `icon_snow`, `icon_storm`.
`icon_weather(ctx, ox, oy, col, code, large)` maps WMO codes to icon functions.
`icon_footprint` is a helper (not called directly).

---

## KNOWN BUG HISTORY (accumulated — never repeat these mistakes)

1. `"108%"` in snprintf format = compiler error. Must be `"108%%"` to emit a literal `%`
2. Health state vars (`s_steps`, `s_distance_m`, `s_calories`, `s_heart_rate`, `s_steps_expected`) must be `#if defined(PBL_HEALTH)` guarded — aplite has no health
3. `prv_fmt_dist` / `prv_fmt_steps` must be `#if defined(PBL_HEALTH)` guarded
4. `UnobstructedAreaHandlers ua` — scope in `{ ... }` block to suppress unused-var warning on frozen SDK platforms (aplite/basalt/diorite have `-DPBL_SDK_FROZEN`)
5. `-Werror=misleading-indentation` — all `else` must be braced, even single-statement bodies
6. `push_files` tool sends **empty** content — always use `create_or_update_file` with correct SHA
7. InfoLine arrays on stack inside draw_layer = stack overflow = "not responding" crash. Must be static globals.
8. `draw_info_line` text GRect uses `x=0` — should use `col_x` parameter so stacked info renders in the correct column, not always at x=0

---

## MESSAGE KEYS (appinfo.json ↔ index.js ↔ main.c — must all agree)

```
WeatherTempF:   0
WeatherTempC:   1
WeatherCode:    2
SunriseTime:    3
SunsetTime:     4
CfgSlot1:       5    (ordered slot position 0, value is SlotType)
CfgSlot2:       6
CfgSlot3:       7
CfgSlot4:       8
CfgSlot5:       9
CfgSlot6:       10
CfgSlot7:       11
CfgTempUnit:    12   (0=°F, 1=°C)
CfgDistUnit:    13   (0=mi, 1=km)
CfgClockFormat: 14   (0=12h, 1=24h)
```
main.c reads: `MESSAGE_KEY_CfgSlot1 + i` for i in 0..6.

---

## WEATHER DATA (Open-Meteo, no API key)

URL: `https://api.open-meteo.com/v1/forecast?latitude=X&longitude=Y&current=temperature_2m,weather_code&daily=sunrise,sunset&temperature_unit=celsius&timezone=auto&forecast_days=2`
- Updated every 30 min from JS companion
- Sunrise/sunset sent as Unix timestamps (uint32)

### WMO Code → Icon
```
0          → sun
1-3        → partly_cloudy
4-48       → cloud (overcast/fog)
51-69,80-82 → rain
71-77,85-86 → snow
95+        → storm
```

---

## PACE COLOR SPECTRUM (emery + PBL_COLOR + PBL_HEALTH only)

```
0 / no data  → GColorBlack
1-30%        → GColorRed
31-60%       → GColorOrange
61-90%       → GColorYellow  ← needs dark (black) fg
91-200%      → GColorGreen   ← needs dark (black) fg
201-400%     → GColorBlue
401-700%     → GColorIndigo
701-1000%    → GColorVividViolet
1001%+       → GColorBlack
```
`prv_bg_needs_dark_fg()` returns true for Yellow + Green.
Non-health / non-color platforms: `bg = s_bg` (black), `s_fg = GColorWhite`.

---

## STEP PACE CALCULATION

Uses `health_service_get_minute_history` to get 7-day historical minute data.
Window = min(elapsed_minutes_today, 120). Returns -1 if < 2 min elapsed today.
`s_steps_expected` = average steps at this time-of-day over past 7 days.
`pace_pct = (s_steps * 100) / s_steps_expected`.

---

## TOUCH API (unresolved)

**Platform guard:** `#if defined(PBL_TOUCH)` — fires on emery and gabbro only.

Current implementation in v3.47d:
```c
// In window_load:
if (touch_service_is_enabled()) {
  touch_service_subscribe(touch_handler, NULL);
}
// In window_unload:
touch_service_unsubscribe();  // unconditional

// touch_handler:
if (event->type != TouchEvent_Touchdown) return;
vibes_enqueue_custom_pattern(TOUCH_VIBE);  // 50ms diagnostic
#if defined(PBL_PLATFORM_EMERY)
s_radius_idx = (s_radius_idx + 1) % RADIUS_COUNT;
layer_mark_dirty(s_canvas_layer);
#endif

// In init() before window_stack_push:
window_set_click_config_provider(s_window, prv_click_config_provider);
// prv_click_config_provider binds real no-op handlers to UP/SELECT/DOWN
```
**Status:** Vibe never fires. All known fixes applied. pebble-calculator works on same device.
Things to try in a future session:
- Subscribe in `init()` instead of `window_load()`
- Remove `touch_service_is_enabled()` guard entirely (try unconditional subscribe)
- Check gabbro (round) — also has PBL_TOUCH

---

## CONFIG PAGE (current v3.47d)

Built as HTML string in `index.js`, opened via `Pebble.openURL('data:text/html,...')`.
Closes via `location.href = 'pebblejs://close#' + encodeURIComponent(JSON.stringify(settings))`.

**v3.47d has:**
- 7 rows, each with ▲▼ buttons + `<select>` for slot type (including Time=17)
- Temp unit (F/C), Distance unit (mi/km), Clock format (12h/24h) radio toggles
- Save button
- BUG: `moveRow()` swaps `sel.value` but doesn't update `<option selected>` attributes — visual display is wrong after swap

**moveRow fix needed:**
```javascript
function moveRow(idx, dir) {
  var newIdx = idx + dir;
  if (newIdx < 0 || newIdx >= 7) return;
  var s1 = document.getElementById('sel' + idx);
  var s2 = document.getElementById('sel' + newIdx);
  var tmp = s1.value;
  s1.value = s2.value;  // this sets value but may not update visual
  s2.value = tmp;
  // Force visual update by manually setting selectedIndex:
  for (var i = 0; i < s1.options.length; i++) s1.options[i].selected = (s1.options[i].value == s1.value);
  for (var i = 0; i < s2.options.length; i++) s2.options[i].selected = (s2.options[i].value == s2.value);
}
```

---

## FILES IN REPO

```
src/main.c          — main watchface C code (v3.47d)
src/pkjs/index.js   — JS companion: weather + config page (v3.47d)
appinfo.json        — app metadata, capabilities, message keys, resources
resources/images/   — digit PNG files (legacy; not used by vector renderer
                      but must stay in appinfo.json or build fails)
CLAUDE_CONTEXT.md   — THIS FILE (AI session handoff)
```

---

## APPINFO.JSON NOTES

- Must have `"configurable"` in `capabilities` array for settings gear to appear
- Resources section lists all the digit/colon PNG files — they are legacy artifacts
  (current code draws all digits as vectors) but removing them breaks the build
- Message keys in `appKeys` must exactly match the table above

---

## NEXT SESSION TODO (priority order)

### 1. Fix stacked layout info line positioning (quick fix)
In `draw_layer`, the stacked branch already computes `info_x` and `info_w` correctly.
The problem is `draw_info_line` ignores them — it uses `GRect(0, ...)` as left edge.

Fix: add `col_x` and `col_w` parameters to `draw_info_line` and pass through:
```c
static void draw_info_line(GContext *ctx, InfoLine *line, int y,
                            int col_x, int col_w, int cx, GTextAlignment align)
```
In the text path: `GRect(col_x, y - INFO_TOP_PAD, col_w, INFO_LINE_H)`
In `draw_icon_text`: `icon_x` for `GTextAlignmentLeft` = `col_x`, for right = `col_x + col_w - unit_w`
All callers in LAYOUT_INFO pass `col_x=SIDE_MARGIN, col_w=line_w`.
Stacked callers pass computed `info_x, info_w`.

### 2. Fix config page moveRow() visual update bug
See fix in CONFIG PAGE section above. `.value =` assignment alone doesn't always force re-render in Pebble webview; need to explicitly set `option.selected`.

### 3. Touch — try alternative approaches
- Subscribe unconditionally (remove `touch_service_is_enabled()` guard)
- Try subscribing in `init()` rather than `window_load()`

### 4. Config page full redesign (see Design Changes Planned above)
This is a large task. New settings:
- Info display mode (always on / off / shake-toggle / shake-1min)
- Info layout mode (wide / stacked-L / stacked-R)  
- Wide content: 7 combo checkboxes
- Stacked content: 8 slot selects  
- Color mode: pace-based / custom solid

### 5. Remove auto-combo logic from prv_slot_text()
Wide combos (Steps+Distance on one line, etc.) should be predefined SlotType IDs,
not auto-detected from adjacent slots. Simplifies the code and gives users control.

### 6. appinfo.json — consider adding new persist/message keys
When config redesign adds more settings (display mode, layout mode, color mode, wide combos, stacked slots), new message keys and possibly new persist keys will be needed. Plan carefully to avoid key collisions.

---

## DESIGN NOTES

- Digits drawn as vectors: `graphics_fill_radial` for rounded caps, `graphics_fill_rect` for stems
- "Squeeze" animation: digits compress then expand on each minute tick
- Corner radius (emery only): `s_radius_opts[] = {UNIT*2, UNIT*3, UNIT*4}` = 16/24/32px
- Background: black base → colored rect with corner radius on top
- Stacked mode: hours on top half, minutes on bottom half, each = 2 side-by-side digits (no colon)
- Wide info mode: colon shown, time centered, info above + below
- `SLOT_DAY_DATE` (id=3) = combined "Mon, May 28" — distinct from separate SLOT_DAY + SLOT_DATE slots
- `DOT` = `" \xc2\xb7 "` = Unicode middle dot separator used in combo wide-mode text
