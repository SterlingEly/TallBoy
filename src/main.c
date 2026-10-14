// ============================================================
// TallBoy — main.c  v3.67
// Design: Sterling Ely. Code: Sterling Ely + Claude. 2026.
//
// v3.59j: data caching philosophy — hide > mislead; UV sentinel -1; 3h weather timeout
// v3.59k: remove PBL_TOUCH code — non-functional on emery firmware (see README)
// v3.59l: icon-text gap HALF_UNIT; sun icon thick+diagonals; steps fill bounds;
//         temp unit spacing; stacked light short format; typical steps via SDK;
//         SLOT_TYPICAL_DAY (26): full-day step goal
// v3.59m: Weekly scope for typical steps (same-day-of-week, most granular);
//         SLOT_CALORIES_TOT (27): resting+active; SLOT_SLEEP (28): sleep duration
// v3.59n: fix wide mode digit freeze — apply pending time at blink bottom (not in SQUISH)
// v3.59o: stacked minute animation — STACK_L: Option A (symmetric), STACK_R: Option B (fold)
// v3.59p: fix stacked full-height bug; fix blink travel (H_ABSOLUTE_MIN floor); naming pass
// v3.59q: wide mode empty slots collapse — digit height computed from actual rendered count
// v3.59r: sun icon rays doubled to stroke_width 2 for better visibility
// v3.59s: remove UP button radius debug feature
// v3.59t: restore rounded rect background (fixed radius UNIT*3)
// v3.59u: background radius corrected to UNIT*2 (matches digit stroke weight)
// v3.60: round platform support (chalk+gabbro, full mode only); faster wide blink (BLINK_STEP 6→8)
// v3.61: round per-column heights — outer digits shorter to follow circle, no clipping;
//        cy centered at ub midpoint on round
// v3.62: sun icon filled disc + thin rays; three pace palettes (dark/medium/light);
//        greys in palette; colon separate color; default custom bg = dark red
// v3.63: sun icon rays longer + correct diagonal directions; AQI slot (29);
//        wide combo slot temp+UV+AQI (30); AQI message key 34; AQI stacked labeled "AQI ##"
// v3.64: gabbro split from chalk -- Pebble Round 2 is 260x260 (UNIT=8, own slot positions and
//        round column heights), it was wrongly sharing chalk's 180x180 block;
//        heart-rate availability check fixed (health_service_metric_accessible always reports
//        HR unavailable -- use aggregate_averaged_accessible with a point-in-time range)
// v3.65: gabbro colon spacing -- slots now use emery's pitch (digit slot to colon slot = 28px =
//        3.5 UNIT, not a full SLOT_W of 40) so the gap across the colon is 3 UNIT, not 6.
// v3.66: chalk colon spacing -- same 3.5 UNIT digit-slot-to-colon pitch (21px at UNIT 6), so the
//        colon dot is 1 UNIT from each neighboring glyph (was a uniform 30px pitch = 2.5 UNIT).
// v3.67: round heights raised (chalk 108/150, gabbro 176/224 -- 2U clearance at column centre,
//        >=1U at glyph corners, measured against the bezel); wide and stacked L/R layouts now
//        work on round. Wide: short-format lines only, 2 above + 2 below (chalk 1 + 1), inset
//        to fit the chord. Stacked: top row tops / bottom row bottoms stretch to each column's
//        full-view bound, the other ends meet at the middle gap; info column clamped to the chord.
// ============================================================

#include <pebble.h>

typedef enum {
  SLOT_EMPTY      =  0,
  SLOT_DAY        =  1,
  SLOT_DATE       =  2,
  SLOT_DAY_DATE   =  3,
  SLOT_TEMP       =  4,
  SLOT_WEATHER    =  5,
  SLOT_STEPS      =  6,
  SLOT_DISTANCE   =  7,
  SLOT_EXP_STEPS  =  8,
  SLOT_PACE       =  9,
  SLOT_CALORIES   = 10,
  SLOT_HEART      = 11,
  SLOT_SUNRISE    = 12,
  SLOT_SUNSET     = 13,
  SLOT_DAYLIGHT   = 14,
  SLOT_BATTERY    = 15,
  SLOT_BLUETOOTH  = 16,
  SLOT_SUN_TIMES  = 17,
  SLOT_STEPS_EXP  = 18,
  SLOT_EXP_PACE   = 19,
  SLOT_BAT_BT     = 20,
  SLOT_DEBUG      = 21,
  SLOT_UV         = 22,
  SLOT_LIGHT_REM  = 23,
  SLOT_UV_LIGHT   = 24,
  SLOT_WEATHER_UV = 25,
  SLOT_TYPICAL_DAY = 26,   // typical full-day step total (midnight-to-midnight, same-day-of-week avg)
  SLOT_CALORIES_TOT = 27,  // active + resting calories (wide: both; stacked: total)
  SLOT_SLEEP        = 28,  // sleep duration last night
  SLOT_AQI          = 29,  // air quality index (stacked: "AQI ##"; wide: combined below)
  SLOT_TEMP_UV_AQI  = 30,  // wide-only: temp · UV index · AQI
} SlotType;

#define LAYOUT_FULL    0
#define LAYOUT_INFO    1
#define LAYOUT_STACK_R 2
#define LAYOUT_STACK_L 3
#define SHAKE_LEN      7

typedef enum {
  INFO_MODE_DEBUG    = 0,
  INFO_MODE_OFF      = 1,
  INFO_MODE_ALWAYS   = 2,
  INFO_MODE_SHAKE    = 3,
  INFO_MODE_SHAKE1   = 4,
} InfoMode;

typedef enum {
  INFO_LAYOUT_WIDE    = 0,
  INFO_LAYOUT_STACK_L = 1,
  INFO_LAYOUT_STACK_R = 2,
} InfoLayout;

#if defined(PBL_PLATFORM_EMERY)
  #define SCREEN_W           200
  #define SCREEN_H           228
  #define SLOT_W              40
  #define COLON_SLOT_X        80
  #define SLOT_H_TENS         12
  #define SLOT_H_ONES         52
  #define SLOT_M_TENS        108
  #define SLOT_M_ONES        148
  #define SIDE_MARGIN         12
  #define HALF_UNIT            4
  #define INFO_LINE_H         28
  #define INFO_GLYPH_H        18
  #define INFO_TOP_PAD        10
  #define INFO_LINE_STEP      26
  #define INFO_LINE_STEP_WIDE 22
  #define INFO_FONT_KEY    FONT_KEY_GOTHIC_24_BOLD
  #define UNIT                 8
  #define ICON_W              16
  #define ICON_V_ADJUST       -3
#elif defined(PBL_PLATFORM_CHALK)
  // chalk (Pebble Time Round): 180x180 round color.
  // UNIT=6. Digit-to-digit pitch is SLOT_W (30px = 5 UNIT: 4 UNIT glyph + 1 UNIT gap); the
  // digit-slot-to-colon pitch is 21px (3.5 UNIT), same rule as emery/gabbro, so the colon dot
  // (1 UNIT wide) sits 1 UNIT from each neighboring glyph:
  //   H_TENS 24 | H_ONES 54 | colon 75 | M_TENS 96 | M_ONES 126   (glyphs 27..51, 57..81, 99..123, 129..153)
  //   Colon center = 75 + 15 = 90 = exact screen center; glyph span 27..153 is centered too.
  // Heights below were set for the old wider columns (outer glyph edge dx=72, now 63), so there
  // is headroom to raise them -- not measured; tune on device.
  // Per-column heights follow the circular bezel (MARGIN_OUTER inset from edge):
  //   Outer cols (H_TENS, M_ONES) dx=60 from center: max_h = ROUND_OUTER_H = 108px
  //   Inner cols (H_ONES, M_TENS) dx=30 from center: max_h = ROUND_INNER_H = 144px
  // No clipping — digits are sized to fit the circle, not clipped by it.
  // Info layouts (wide, stacked L/R) supported on round since v3.67.
  // NOTE: gabbro (Pebble Round 2) is a DIFFERENT, larger screen -- see its own block below.
  #define SCREEN_W           180
  #define SCREEN_H           180
  #define SLOT_W              30
  #define COLON_SLOT_X        75
  #define SLOT_H_TENS         24
  #define SLOT_H_ONES         54
  #define SLOT_M_TENS         96
  #define SLOT_M_ONES        126
  #define SIDE_MARGIN         24
  #define HALF_UNIT            3
  #define INFO_LINE_H         20
  #define INFO_GLYPH_H        14
  #define INFO_TOP_PAD         6
  #define INFO_LINE_STEP      20
  #define INFO_LINE_STEP_WIDE 17
  #define INFO_FONT_KEY    FONT_KEY_GOTHIC_18_BOLD
  #define UNIT                 6
  #define ICON_W              12
  #define ICON_V_ADJUST        0
  // Per-column max digit heights inscribed in the circular bezel with MARGIN_OUTER inset:
  //   Outer cols (H_TENS, M_ONES): dx=60px from center -> max_h = 108px (UNIT*18)
  //   Inner cols (H_ONES, M_TENS): dx=30px from center -> max_h = 150px (UNIT*25, v3.67)
  // v3.67: inner 144 -> 150. Outer stays 108 (114 would leave only 0.83 UNIT at the glyph corners).
  #define ROUND_OUTER_H       (UNIT * 18)   // 108px
  #define ROUND_INNER_H       (UNIT * 25)   // 150px
  // Round wide mode: info lines are inset so each fits the bezel chord; short-format text only.
  #define ROUND_INFO_INSET    11            // px from the top/bottom of the unobstructed area
  #define ROUND_WIDE_LINES    1             // lines above and below the digits
  #define ROUND_STACK_LINES   5             // max lines in the stacked info column
#elif defined(PBL_PLATFORM_GABBRO)
  // gabbro (Pebble Round 2, Core Devices 2026): 260x260 round color. NO heart-rate sensor.
  // Same glyph scale as emery (UNIT=8, SLOT_W=40) so digits keep their weight on the bigger screen.
  // Slots are spaced SLOT_W apart and centered on x=130:
  //   H_TENS 42 | H_ONES 82 | colon 110 | M_TENS 138 | M_ONES 178  (42px margin each side)
  //   Colon center = 110 + 20 = 130 = exact screen center.
  //   Digit-to-colon pitch is 28px (3.5 UNIT), identical to emery (52/80/108), so the glyph gap
  //   across the colon is 3 UNIT. v3.64 used a full 40px pitch there (6 UNIT) -- too airy.
  // Per-column heights keep chalk's proportions (outer ~0.6, inner ~0.8 of screen height):
  //   Outer cols (H_TENS, M_ONES) dx=68 from center -> ROUND_OUTER_H = UNIT*20 = 160px
  //   Inner cols (H_ONES, M_TENS) dx=28 from center -> ROUND_INNER_H = UNIT*26 = 208px
  //   (heights kept from v3.64; columns moved inward so there is headroom to raise them)
  // These two heights are scaled from chalk, not measured -- tune on device if anything clips
  // or looks undersized. Info layouts supported on round since v3.67.
  #define SCREEN_W           260
  #define SCREEN_H           260
  #define SLOT_W              40
  #define COLON_SLOT_X       110
  #define SLOT_H_TENS         42
  #define SLOT_H_ONES         82
  #define SLOT_M_TENS        138
  #define SLOT_M_ONES        178
  #define SIDE_MARGIN         42
  #define HALF_UNIT            4
  #define INFO_LINE_H         28
  #define INFO_GLYPH_H        18
  #define INFO_TOP_PAD        10
  #define INFO_LINE_STEP      26
  #define INFO_LINE_STEP_WIDE 22
  #define INFO_FONT_KEY    FONT_KEY_GOTHIC_24_BOLD
  #define UNIT                 8
  #define ICON_W              16
  #define ICON_V_ADJUST       -3
  // v3.67: 160/208 -> 176/224 (corner clearance 1.05 / 1.23 UNIT, centre clearance 2.35 / 1.82 UNIT).
  #define ROUND_OUTER_H       (UNIT * 22)   // 176px
  #define ROUND_INNER_H       (UNIT * 28)   // 224px
  #define ROUND_INFO_INSET    13
  #define ROUND_WIDE_LINES    2
  #define ROUND_STACK_LINES   6
#else
  #define SCREEN_W           144
  #define SCREEN_H           168
  #define SLOT_W              30
  #define COLON_SLOT_X        57
  #define SLOT_H_TENS          6
  #define SLOT_H_ONES         36
  #define SLOT_M_TENS         78
  #define SLOT_M_ONES        108
  #define SIDE_MARGIN          6
  #define HALF_UNIT            3
  #define INFO_LINE_H         20
  #define INFO_GLYPH_H        14
  #define INFO_TOP_PAD         6
  #define INFO_LINE_STEP      20
  #define INFO_LINE_STEP_WIDE 17
  #define INFO_FONT_KEY    FONT_KEY_GOTHIC_18_BOLD
  #define UNIT                 6
  #define ICON_W              12
  #define ICON_V_ADJUST        0
#endif

#define ICON_LARGE       (ICON_W == 16)
#define ICON_TEXT_GAP    HALF_UNIT
#define MARGIN_CANVAS   UNIT
#define MARGIN_DIGIT    UNIT
#define MARGIN_OUTER    (MARGIN_CANVAS + MARGIN_DIGIT)
#define HALF_SLOT_PAD   (UNIT / 2)
#define GLYPH_W         (UNIT * 4)
#define H_MIN           (UNIT * 11)
#define H_ABSOLUTE_MIN  (UNIT * 5)
#define INFO_LINE_BUF   48
#define INFO_LINES_MAX  8

#define STACK_INFO_TOP_MARGIN   (UNIT * 2)
#define WIDE_BELOW_EXTRA        HALF_UNIT
#define WIDE_SLOTS         6
#define STACK_SLOTS        8

#define SHADOW_DX  HALF_UNIT
#define SHADOW_DY  HALF_UNIT

#define SPLIT_V_FRAMES  24
#define SPLIT_H_FRAMES  14
#define SPLIT_MS        10

#define INFO_SLIDE_FRAMES  12
#define INFO_SLIDE_DIST    (UNIT * 4)

static int prv_lerp_v(int a, int b, int step) {
  if (step <= 0) return a;
  if (step >= SPLIT_V_FRAMES) return b;
  return a + ((b - a) * step) / SPLIT_V_FRAMES;
}
static int prv_lerp_h(int a, int b, int step) {
  if (step <= 0) return a;
  if (step >= SPLIT_H_FRAMES) return b;
  return a + ((b - a) * step) / SPLIT_H_FRAMES;
}

static const int EASE[10] = { 4, 6, 8, 10, 12, 12, 10, 8, 6, 4 };
#define EASE_LEN  10
#define ANIM_STEP_MS      16
#define ANIM_SNAP_MS      80
#define ANIM_OVERSHOOT    UNIT
#define ANTICIPATION_PX   HALF_UNIT
#define ANTICIPATION_MS   16
#define COUNTDOWN_HOLD_MS 120
#define BLINK_REPS        2
#define BLINK_STEP        8
#define DOT               " \xc2\xb7 "
#define SHAKE1_TIMEOUT_MS 60000
#define WEATHER_MAX_AGE   (3 * 3600)   // hide weather/UV/AQI after 3h without update

#define COLON_OFFSCREEN  (SCREEN_H)

#define QUICK_LOOK_ACTIVE(ub_h)  ((ub_h) < (SCREEN_H - UNIT))
#define BOTTOM_MARGIN(ub_h)      (QUICK_LOOK_ACTIVE(ub_h) ? MARGIN_DIGIT : MARGIN_OUTER)

typedef enum { CD_SHRINK, CD_HOLD_MIN, CD_EXPAND, CD_HOLD_MAX } CdSubPhase;
typedef enum {
  PHASE_COUNTDOWN, PHASE_BLINK, PHASE_DONE,
  PHASE_ANTICIPATE, PHASE_SQUISH, PHASE_EXPAND, PHASE_SHAKE_CYCLE,
  PHASE_SPLIT_V, PHASE_SPLIT_H, PHASE_MERGE_H, PHASE_MERGE_V
} Phase;

static bool s_expand_no_overshoot = false;
static bool s_blink_single = false;

static int  s_info_slide      = 0;
static int  s_info_slide_dir  = 0;
static int  s_info_slide_step = 0;

#define COLOR_BLACK  0x00
#define COLOR_WHITE  0x3f

static GColor prv_color_from_idx(uint8_t idx) {
  return GColorFromRGB(((idx >> 4) & 3) * 85, ((idx >> 2) & 3) * 85, (idx & 3) * 85);
}

// Color modes
// DYNAMIC (0)      — pace-based background, medium palette, auto fg (backward compat)
// STATIC (1)       — fully custom colors from picker
// PACE_DARK (2)    — pace palette using dark colors (#AA max channel); always white fg
// PACE_MEDIUM (3)  — pace palette using saturated mid-brightness; always white fg
// PACE_LIGHT (4)   — pace palette using light colors (#55 min channel); always black fg
#define COLOR_MODE_DYNAMIC    0
#define COLOR_MODE_STATIC     1
#define COLOR_MODE_PACE_DARK  2
#define COLOR_MODE_PACE_MED   3
#define COLOR_MODE_PACE_LIGHT 4

// Default custom color scheme: dark red background, black shadow, white digits
#define COLOR_DEFAULT_BG     32   // #AA0000 dark red
#define COLOR_DEFAULT_DIG    COLOR_WHITE
#define COLOR_DEFAULT_SHADOW COLOR_BLACK
#define COLOR_DEFAULT_INFO   COLOR_WHITE
#define COLOR_DEFAULT_COLON  COLOR_WHITE

static int     s_cfg_color_mode   = COLOR_MODE_DYNAMIC;
static uint8_t s_cfg_col_bg       = COLOR_DEFAULT_BG;
static uint8_t s_cfg_col_dig_h    = COLOR_DEFAULT_DIG;
static uint8_t s_cfg_col_dig_m    = COLOR_DEFAULT_DIG;
static uint8_t s_cfg_col_shadow   = COLOR_DEFAULT_SHADOW;
static uint8_t s_cfg_col_info     = COLOR_DEFAULT_INFO;
static uint8_t s_cfg_col_colon    = COLOR_DEFAULT_COLON;

typedef void (*IconFn)(GContext*,int,int,GColor,bool);
typedef struct {
  char text[INFO_LINE_BUF];
  bool has_icon;
  IconFn icon_fn;
  bool is_battery;
  bool is_weather;
  bool is_debug_sq;
  int  icon_extra;
} InfoLine;

static Window    *s_window;
static Layer     *s_canvas_layer;
static int        s_hour = 0, s_minute = 0;
static int        s_h = 0, s_target_h = 0, s_h_min = 0, s_ease_idx = 0;
static GColor     s_fg, s_bg, s_fg_hr, s_fg_mn, s_fg_colon, s_shadow_col;
static Phase      s_phase = PHASE_DONE;
static int        s_anim_step = 0, s_anim_rep = 0;
static bool       s_going_down = true, s_overshot = false;
static int        s_countdown_digit = 9;
static CdSubPhase s_cd_sub = CD_HOLD_MAX;
static AppTimer  *s_timer = NULL;
static AppTimer  *s_shake1_timer = NULL;
static bool       s_digit_pending = false;
static int        s_pending_hour = 0, s_pending_minute = 0;
static int        s_stk_h_start = 0;  // digit height at start of stacked blink (Option B drift)
static int        s_battery_pct = 100;
static bool       s_charging = false, s_bt_connected = true;
#if defined(PBL_HEALTH)
static int        s_steps = 0, s_distance_m = 0, s_calories = 0, s_calories_resting = 0, s_heart_rate = 0;
static bool       s_heart_rate_valid = false;
static int        s_steps_expected = -1;    // typical steps at current time of day (same-day-of-week avg)
static int        s_steps_typical_day = -1; // typical full-day step total (same-day-of-week avg)
static int        s_sleep_seconds = -1;     // sleep since 6pm yesterday; -1 = unavailable
#endif
static int        s_sunrise_min = -1, s_sunset_min = -1;
static int        s_weather_temp_f = -999, s_weather_temp_c = -999;
static int        s_weather_code = 0;
static int        s_uv_index = -1;          // -1 = never received; 0+ = valid (0 is real UV 0)
static int        s_aqi = -1;               // -1 = never received; 0+ = valid AQI
static int        s_sunrise_tom_min = -1;
static time_t     s_weather_ts = 0;         // unix time of last weather receipt; 0 = never
static time_t     s_aqi_ts = 0;             // unix time of last AQI receipt; 0 = never
static int        s_layout = LAYOUT_FULL;

static InfoLine s_above_lines[INFO_LINES_MAX];
static InfoLine s_below_lines[INFO_LINES_MAX];
static InfoLine s_col_lines[INFO_LINES_MAX];

static int  s_cfg_wide[WIDE_SLOTS]   = { 3, 5, 0, 6, 17, 15 };
static int  s_cfg_stack[STACK_SLOTS] = { 1, 3, 6, 9, 11, 5, 15, 16 };
static bool s_cfg_temp_f    = true;
static bool s_cfg_dist_mi   = true;
static bool s_cfg_invert    = false;
static InfoMode   s_cfg_info_mode   = INFO_MODE_DEBUG;
static InfoLayout s_cfg_info_layout = INFO_LAYOUT_STACK_L;

static int s_debug_next = 0;
static const int DEBUG_CYCLE_LAYOUTS[] = { LAYOUT_INFO, LAYOUT_STACK_R, LAYOUT_STACK_L };
#define DEBUG_CYCLE_LEN 3

#define PERSIST_CFG_VERSION  0
#define PERSIST_CFG_FLAGS    1
#define PERSIST_CFG_WIDE     2
#define PERSIST_CFG_STACK    3
#define PERSIST_CFG_STACK_HI 4
#define PERSIST_CFG_COLORS   5
#define CFG_VERSION          6   // bumped v3.62: added colon color to persist

static int s_sv_hr_cy_s, s_sv_hr_cy_e;
static int s_sv_mn_cy_s, s_sv_mn_cy_e;
static int s_sv_h_s,     s_sv_h_e;
static int s_sv_cy;
static int s_sv_top_dot_s, s_sv_top_dot_e;
static int s_sv_bot_dot_s, s_sv_bot_dot_e;

static int s_sh_hr_tx_s, s_sh_hr_tx_e;
static int s_sh_mn_tx_s, s_sh_mn_tx_e;
static int s_sh_hr_cy, s_sh_mn_cy;
static int s_sh_h;
static int s_sh_col_s, s_sh_col_e;
static int s_split_target_layout;

static void prv_save_config(void) {
  persist_write_int(PERSIST_CFG_VERSION, CFG_VERSION);
  int32_t flags = (s_cfg_temp_f  ? 1 : 0)
                | (s_cfg_dist_mi ? 2 : 0)
                | ((int)s_cfg_info_mode   << 2)
                | ((int)s_cfg_info_layout << 5)
                | (s_cfg_invert           ? (1 << 7) : 0);
  persist_write_int(PERSIST_CFG_FLAGS, flags);
  int32_t wp = 0;
  for (int i = 0; i < WIDE_SLOTS; i++) wp |= (s_cfg_wide[i] & 0x1f) << (i * 5);
  persist_write_int(PERSIST_CFG_WIDE, wp);
  int32_t sp_lo = 0, sp_hi = 0;
  for (int i = 0; i < 6; i++) sp_lo |= (s_cfg_stack[i] & 0x1f) << (i * 5);
  for (int i = 0; i < 2; i++) sp_hi |= (s_cfg_stack[6+i] & 0x1f) << (i * 5);
  persist_write_int(PERSIST_CFG_STACK, sp_lo);
  persist_write_int(PERSIST_CFG_STACK_HI, sp_hi);
  int32_t ca = (s_cfg_color_mode & 0xff)
             | ((int32_t)(s_cfg_col_bg    & 0x3f) << 8)
             | ((int32_t)(s_cfg_col_dig_h & 0x3f) << 16)
             | ((int32_t)(s_cfg_col_dig_m & 0x3f) << 24);
  int32_t cb = ((int32_t)(s_cfg_col_shadow & 0x3f))
             | ((int32_t)(s_cfg_col_info   & 0x3f) << 8)
             | ((int32_t)(s_cfg_col_colon  & 0x3f) << 16);
  persist_write_int(PERSIST_CFG_COLORS, ca);
  persist_write_int(PERSIST_CFG_COLORS + 1, cb);
}
static void prv_load_config(void) {
  if (!persist_exists(PERSIST_CFG_VERSION) ||
      persist_read_int(PERSIST_CFG_VERSION) != CFG_VERSION) return;
  if (persist_exists(PERSIST_CFG_FLAGS)) {
    int32_t flags = persist_read_int(PERSIST_CFG_FLAGS);
    s_cfg_temp_f       = (flags & 1) != 0;
    s_cfg_dist_mi      = (flags & 2) != 0;
    int mode   = (flags >> 2) & 0x7;
    int layout = (flags >> 5) & 0x3;
    s_cfg_invert       = (flags & (1 << 7)) != 0;
    s_cfg_info_mode   = (mode   < 5) ? (InfoMode)mode     : INFO_MODE_DEBUG;
    s_cfg_info_layout = (layout < 3) ? (InfoLayout)layout : INFO_LAYOUT_STACK_L;
  }
  if (persist_exists(PERSIST_CFG_WIDE)) {
    int32_t wp = persist_read_int(PERSIST_CFG_WIDE);
    for (int i = 0; i < WIDE_SLOTS; i++) s_cfg_wide[i] = (wp >> (i * 5)) & 0x1f;
  }
  if (persist_exists(PERSIST_CFG_STACK)) {
    int32_t sp_lo = persist_read_int(PERSIST_CFG_STACK);
    int32_t sp_hi = persist_exists(PERSIST_CFG_STACK_HI) ? persist_read_int(PERSIST_CFG_STACK_HI) : 0;
    for (int i = 0; i < 6; i++) s_cfg_stack[i] = (sp_lo >> (i * 5)) & 0x1f;
    for (int i = 0; i < 2; i++) s_cfg_stack[6+i] = (sp_hi >> (i * 5)) & 0x1f;
  }
  if (persist_exists(PERSIST_CFG_COLORS)) {
    int32_t ca = persist_read_int(PERSIST_CFG_COLORS);
    int32_t cb = persist_exists(PERSIST_CFG_COLORS + 1) ? persist_read_int(PERSIST_CFG_COLORS + 1) : 0;
    s_cfg_color_mode  =  ca        & 0xff;
    s_cfg_col_bg      = (ca >>  8) & 0x3f;
    s_cfg_col_dig_h   = (ca >> 16) & 0x3f;
    s_cfg_col_dig_m   = (ca >> 24) & 0x3f;
    s_cfg_col_shadow  =  cb        & 0x3f;
    s_cfg_col_info    = (cb >>  8) & 0x3f;
    s_cfg_col_colon   = (cb >> 16) & 0x3f;
  }
}

static int prv_info_layout_for_shake(void) {
  switch (s_cfg_info_mode) {
    case INFO_MODE_DEBUG: {
      int layout = DEBUG_CYCLE_LAYOUTS[s_debug_next];
      s_debug_next = (s_debug_next + 1) % DEBUG_CYCLE_LEN;
      return layout;
    }
    case INFO_MODE_SHAKE:
    case INFO_MODE_SHAKE1:
      switch (s_cfg_info_layout) {
        case INFO_LAYOUT_WIDE:    return LAYOUT_INFO;
        case INFO_LAYOUT_STACK_L: return LAYOUT_STACK_L;
        case INFO_LAYOUT_STACK_R: return LAYOUT_STACK_R;
        default:                  return LAYOUT_INFO;
      }
    default: return LAYOUT_INFO;
  }
}

#if defined(PBL_COLOR)
// Step pace background color palettes.
// Each maps today's steps vs. typical (same-day-of-week avg) to a background color.
// Palette choice controls brightness tier and fg color:
//   DARK   — #AA max per channel; always white fg
//   MEDIUM — saturated, no fg flip (Chrome Yellow and May Green replace Yellow/Green)
//   LIGHT  — #55 min per channel; always black fg
// DYNAMIC mode uses MEDIUM for backward compatibility.

static GColor prv_pace_color_from_idx(int idx) {
  // Convert 6-bit Pebble palette index to GColor
  return GColorFromRGB(((idx >> 4) & 3) * 85, ((idx >> 2) & 3) * 85, (idx & 3) * 85);
}

static GColor prv_pace_color(int steps_today, int steps_expected, int mode) {
  if (steps_expected <= 0 || steps_today <= 0) return GColorBlack;
  int pct = (steps_today * 100) / steps_expected;
  if (pct <= 0) return GColorBlack;

  // Palette indices for 7 pace steps (thresholds: 30/60/90/200/400/700/1000%)
  // Dark:   idx 32,36,40, 8, 2,18,34  (#AA0000→#AA5500→#AAAA00→#00AA00→#0000AA→#5500AA→#AA00AA)
  // Medium: idx 48,52,56,25, 3,18,34  (#FF0000→#FF5500→#FFAA00→#55AA55→#0000FF→#5500AA→#AA00AA)
  // Light:  idx 53,57,61,45,27,23,55  (#FF5555→#FFAA55→#FFFF55→#AAFF55→#55AAFF→#5555FF→#FF55FF)
  static const int dark_pal[7]   = { 32, 36, 40,  8,  2, 18, 34 };
  static const int medium_pal[7] = { 48, 52, 56, 25,  3, 18, 34 };
  static const int light_pal[7]  = { 53, 57, 61, 45, 27, 23, 55 };

  const int *pal = (mode == COLOR_MODE_PACE_DARK)  ? dark_pal  :
                   (mode == COLOR_MODE_PACE_LIGHT)  ? light_pal : medium_pal;

  int step = (pct <= 30)   ? 0 :
             (pct <= 60)   ? 1 :
             (pct <= 90)   ? 2 :
             (pct <= 200)  ? 3 :
             (pct <= 400)  ? 4 :
             (pct <= 700)  ? 5 : 6;

  return prv_pace_color_from_idx(pal[step]);
}

static bool prv_pace_mode_needs_dark_fg(int mode) {
  // Light tier always has dark (black) fg; dark and medium always white
  return (mode == COLOR_MODE_PACE_LIGHT);
}
#endif

#if defined(PBL_HEALTH)
// Typical steps use HealthServiceTimeScopeWeekly — the most granular scope.
// Weekly = same day-of-week only (e.g. all past Tuesdays if today is Tuesday),
// up to 4 weeks of history. This gives "typical Tuesday" not "typical day",
// which is more accurate since schedules vary by day of week.
// The SDK handles averaging and zero-day exclusion automatically.
static void prv_update_health(void) {
  HealthServiceAccessibilityMask mask;
  time_t today_start = time_start_of_today();
  time_t now_t = time(NULL);

  mask = health_service_metric_accessible(HealthMetricStepCount, today_start, now_t);
  s_steps = (mask & HealthServiceAccessibilityMaskAvailable)
    ? (int)health_service_sum_today(HealthMetricStepCount) : 0;

  mask = health_service_metric_accessible(HealthMetricWalkedDistanceMeters, today_start, now_t);
  s_distance_m = (mask & HealthServiceAccessibilityMaskAvailable)
    ? (int)health_service_sum_today(HealthMetricWalkedDistanceMeters) : 0;

  mask = health_service_metric_accessible(HealthMetricActiveKCalories, today_start, now_t);
  s_calories = (mask & HealthServiceAccessibilityMaskAvailable)
    ? (int)health_service_sum_today(HealthMetricActiveKCalories) : 0;

  mask = health_service_metric_accessible(HealthMetricRestingKCalories, today_start, now_t);
  s_calories_resting = (mask & HealthServiceAccessibilityMaskAvailable)
    ? (int)health_service_sum_today(HealthMetricRestingKCalories) : 0;

  // Heart rate. SDK quirk: health_service_metric_accessible() ALWAYS reports
  // HealthMetricHeartRateBPM as unavailable, so the cumulative-metric check used above can never
  // succeed here. aggregate_averaged_accessible with a point-in-time range is the correct check
  // for an instantaneous metric. Only emery and diorite (HR model) have the sensor; elsewhere this
  // stays unavailable and the slot shows "--".
  mask = health_service_metric_aggregate_averaged_accessible(
           HealthMetricHeartRateBPM, now_t, now_t,
           HealthAggregationAvg, HealthServiceTimeScopeOnce);
  if (mask & HealthServiceAccessibilityMaskAvailable) {
    int bpm = (int)health_service_peek_current_value(HealthMetricHeartRateBPM);
    if (bpm > 0) { s_heart_rate = bpm; s_heart_rate_valid = true; }
  }

  // Typical steps at current time of day (same-day-of-week, up to 4 past weeks)
  mask = health_service_metric_averaged_accessible(HealthMetricStepCount, today_start, now_t,
                                                    HealthServiceTimeScopeWeekly);
  s_steps_expected = (mask & HealthServiceAccessibilityMaskAvailable)
    ? (int)health_service_sum_averaged(HealthMetricStepCount, today_start, now_t,
                                        HealthServiceTimeScopeWeekly)
    : -1;

  // Typical full-day step total (same-day-of-week, midnight to midnight)
  time_t tomorrow_start = today_start + SECONDS_PER_DAY;
  mask = health_service_metric_averaged_accessible(HealthMetricStepCount, today_start, tomorrow_start,
                                                    HealthServiceTimeScopeWeekly);
  s_steps_typical_day = (mask & HealthServiceAccessibilityMaskAvailable)
    ? (int)health_service_sum_averaged(HealthMetricStepCount, today_start, tomorrow_start,
                                        HealthServiceTimeScopeWeekly)
    : -1;

  // Sleep last night: 6pm yesterday to now (wide window catches late sleepers)
  time_t sleep_start = today_start - (6 * SECONDS_PER_HOUR);
  mask = health_service_metric_accessible(HealthMetricSleepSeconds, sleep_start, now_t);
  s_sleep_seconds = (mask & HealthServiceAccessibilityMaskAvailable)
    ? (int)health_service_sum(HealthMetricSleepSeconds, sleep_start, now_t)
    : -1;
}
#endif

// ============================================================
// ICONS
// Each icon draws into a fixed box at (ox,oy):
//   large=true  -> 16x16 px box (emery, ICON_W=16 = 2xUNIT)
//   large=false -> 12x12 px box (other platforms, ICON_W=12 = 2xUNIT)
// ============================================================

static void icon_footprint(GContext *ctx, int fx, int fy, GColor col, bool large) {
  graphics_context_set_fill_color(ctx, col);
  if (large) {
    graphics_fill_rect(ctx, GRect(fx,   fy,   5, 8), 2, GCornersAll);
    graphics_fill_rect(ctx, GRect(fx+1, fy+7, 3, 5), 1, GCornersAll);
  } else {
    graphics_fill_rect(ctx, GRect(fx,   fy,   4, 6), 2, GCornersAll);
    graphics_fill_rect(ctx, GRect(fx+1, fy+5, 2, 4), 1, GCornersAll);
  }
}
static void icon_steps(GContext *ctx, int ox, int oy, GColor col, bool large) {
  if (large) {
    icon_footprint(ctx, ox+10, oy+0, col, true);
    icon_footprint(ctx, ox+0,  oy+3, col, true);
  } else {
    icon_footprint(ctx, ox+7, oy+0, col, false);
    icon_footprint(ctx, ox+0, oy+3, col, false);
  }
}
static void icon_battery(GContext *ctx, int ox, int oy, GColor col, int pct, bool large) {
  graphics_context_set_fill_color(ctx, col);
  if (s_charging) {
    if (large) {
      graphics_fill_rect(ctx, GRect(ox+7,oy+0,5,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+6,oy+1,5,1),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+5,oy+2,5,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+4,oy+3,5,1),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+3,oy+4,5,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+1,oy+5,11,2),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+5,oy+7,5,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+4,oy+8,5,1),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+3,oy+9,5,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+2,oy+10,5,1),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+1,oy+11,5,1),0,GCornerNone);
    } else {
      graphics_fill_rect(ctx, GRect(ox+5,oy+0,4,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+4,oy+1,4,1),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+3,oy+2,4,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+1,oy+3,8,2),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+3,oy+5,4,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+2,oy+6,4,1),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+1,oy+7,4,1),0,GCornerNone);
    }
  } else {
    graphics_context_set_stroke_color(ctx, col); graphics_context_set_stroke_width(ctx, 1);
    if (large) {
      graphics_draw_rect(ctx, GRect(ox+1,oy+3,12,10));
      graphics_fill_rect(ctx, GRect(ox+13,oy+6,2,4),0,GCornerNone);
      int fw = (10*pct)/100; if (fw<1&&pct>0) fw=1;
      if (fw>0) graphics_fill_rect(ctx, GRect(ox+2,oy+4,fw,8),0,GCornerNone);
    } else {
      graphics_draw_rect(ctx, GRect(ox+1,oy+2,9,7));
      graphics_fill_rect(ctx, GRect(ox+10,oy+4,2,3),0,GCornerNone);
      int fw = (7*pct)/100; if (fw<1&&pct>0) fw=1;
      if (fw>0) graphics_fill_rect(ctx, GRect(ox+2,oy+3,fw,5),0,GCornerNone);
    }
  }
}
static void icon_bt(GContext *ctx, int ox, int oy, GColor col, bool large) {
  graphics_context_set_stroke_color(ctx, col); graphics_context_set_stroke_width(ctx, 1);
  if (large) {
    graphics_draw_line(ctx, GPoint(ox+5,oy+1),  GPoint(ox+5,oy+14));
    graphics_draw_pixel(ctx, GPoint(ox+4,oy+5)); graphics_draw_pixel(ctx, GPoint(ox+3,oy+4)); graphics_draw_pixel(ctx, GPoint(ox+2,oy+3));
    graphics_draw_pixel(ctx, GPoint(ox+4,oy+10)); graphics_draw_pixel(ctx, GPoint(ox+3,oy+11)); graphics_draw_pixel(ctx, GPoint(ox+2,oy+12));
    graphics_draw_pixel(ctx, GPoint(ox+6,oy+2)); graphics_draw_pixel(ctx, GPoint(ox+7,oy+3)); graphics_draw_pixel(ctx, GPoint(ox+8,oy+4));
    graphics_draw_pixel(ctx, GPoint(ox+9,oy+5)); graphics_draw_pixel(ctx, GPoint(ox+8,oy+6)); graphics_draw_pixel(ctx, GPoint(ox+7,oy+7)); graphics_draw_pixel(ctx, GPoint(ox+6,oy+8));
    graphics_draw_pixel(ctx, GPoint(ox+6,oy+9)); graphics_draw_pixel(ctx, GPoint(ox+7,oy+10)); graphics_draw_pixel(ctx, GPoint(ox+8,oy+11));
    graphics_draw_pixel(ctx, GPoint(ox+9,oy+12)); graphics_draw_pixel(ctx, GPoint(ox+8,oy+13)); graphics_draw_pixel(ctx, GPoint(ox+7,oy+14));
  } else {
    graphics_draw_line(ctx, GPoint(ox+3,oy+0),  GPoint(ox+3,oy+10));
    graphics_draw_pixel(ctx, GPoint(ox+2,oy+3)); graphics_draw_pixel(ctx, GPoint(ox+1,oy+2));
    graphics_draw_pixel(ctx, GPoint(ox+2,oy+7)); graphics_draw_pixel(ctx, GPoint(ox+1,oy+8));
    graphics_draw_pixel(ctx, GPoint(ox+4,oy+1)); graphics_draw_pixel(ctx, GPoint(ox+5,oy+2)); graphics_draw_pixel(ctx, GPoint(ox+6,oy+3));
    graphics_draw_pixel(ctx, GPoint(ox+7,oy+4)); graphics_draw_pixel(ctx, GPoint(ox+6,oy+5)); graphics_draw_pixel(ctx, GPoint(ox+5,oy+6)); graphics_draw_pixel(ctx, GPoint(ox+4,oy+7));
    graphics_draw_pixel(ctx, GPoint(ox+5,oy+8)); graphics_draw_pixel(ctx, GPoint(ox+6,oy+9));
    graphics_draw_pixel(ctx, GPoint(ox+7,oy+10)); graphics_draw_pixel(ctx, GPoint(ox+6,oy+11));
  }
}
static void icon_heart(GContext *ctx, int ox, int oy, GColor col, bool large) {
  graphics_context_set_fill_color(ctx, col);
  if (large) {
    int y = oy + 3;
    graphics_fill_rect(ctx,GRect(ox+1,y+0,4,2),1,GCornersTop); graphics_fill_rect(ctx,GRect(ox+9,y+0,4,2),1,GCornersTop);
    graphics_fill_rect(ctx,GRect(ox+0,y+1,5,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+9,y+1,4,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+0,y+2,6,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+8,y+2,5,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+0,y+3,13,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+1,y+4,11,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+2,y+5,9,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+3,y+6,7,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+4,y+7,5,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+5,y+8,3,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+6,y+9,1,1),0,GCornerNone);
  } else {
    int y = oy + 2;
    graphics_fill_rect(ctx,GRect(ox+1,y+0,3,2),1,GCornersTop); graphics_fill_rect(ctx,GRect(ox+7,y+0,3,2),1,GCornersTop);
    graphics_fill_rect(ctx,GRect(ox+0,y+1,4,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+7,y+1,3,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+0,y+2,10,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+1,y+3,8,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+2,y+4,6,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+3,y+5,4,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+4,y+6,2,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+5,y+7,1,1),0,GCornerNone);
  }
}
static void icon_calories(GContext *ctx, int ox, int oy, GColor col, bool large) {
  graphics_context_set_fill_color(ctx, col);
  if (large) {
    graphics_fill_rect(ctx,GRect(ox+3,oy+10,10,5),2,GCornersBottom);
    graphics_fill_rect(ctx,GRect(ox+4,oy+6,8,5),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+5,oy+3,4,4),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+6,oy+1,2,3),0,GCornerNone);
  } else {
    graphics_fill_rect(ctx,GRect(ox+2,oy+7,8,4),2,GCornersBottom);
    graphics_fill_rect(ctx,GRect(ox+3,oy+4,6,4),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+4,oy+2,3,3),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+5,oy+1,1,2),0,GCornerNone);
  }
}
static void icon_sun(GContext *ctx, int ox, int oy, GColor col, bool large) {
  int sz  = large ? 16 : 12;
  int icx = ox + sz/2, icy = oy + sz/2;
  int cr  = large ? 3 : 2;
  // Filled disc
  graphics_context_set_fill_color(ctx, col);
  graphics_fill_radial(ctx, GRect(icx-cr, icy-cr, cr*2, cr*2),
                        GOvalScaleModeFitCircle, (uint16_t)cr, 0, DEG_TO_TRIGANGLE(360));
  // Thin rays: stroke_width 1, all pointing directly away from center.
  // Diagonal rays: start near circle edge and extend outward along the true 45 degree diagonal.
  // top-right and bottom-left were previously mirrored -- now corrected.
  graphics_context_set_stroke_color(ctx, col); graphics_context_set_stroke_width(ctx, 1);
  if (large) {
    // Cardinal rays (3px)
    graphics_draw_line(ctx, GPoint(icx, oy+0),   GPoint(icx, oy+2));    // top
    graphics_draw_line(ctx, GPoint(icx, oy+13),  GPoint(icx, oy+15));   // bottom
    graphics_draw_line(ctx, GPoint(ox+0,  icy),  GPoint(ox+2,  icy));   // left
    graphics_draw_line(ctx, GPoint(ox+13, icy),  GPoint(ox+15, icy));   // right
    // Diagonal rays (3px): from near edge outward along each 45 degree axis
    graphics_draw_line(ctx, GPoint(ox+5, oy+5),  GPoint(ox+3, oy+3));   // top-left
    graphics_draw_line(ctx, GPoint(ox+11,oy+5),  GPoint(ox+13,oy+3));   // top-right
    graphics_draw_line(ctx, GPoint(ox+5, oy+11), GPoint(ox+3, oy+13));  // bottom-left
    graphics_draw_line(ctx, GPoint(ox+11,oy+11), GPoint(ox+13,oy+13));  // bottom-right
  } else {
    // Cardinal rays (2px)
    graphics_draw_line(ctx, GPoint(icx, oy+0),  GPoint(icx, oy+1));    // top
    graphics_draw_line(ctx, GPoint(icx, oy+10), GPoint(icx, oy+11));   // bottom
    graphics_draw_line(ctx, GPoint(ox+0, icy),  GPoint(ox+1, icy));    // left
    graphics_draw_line(ctx, GPoint(ox+10,icy),  GPoint(ox+11,icy));    // right
    // Diagonal rays (2px)
    graphics_draw_line(ctx, GPoint(ox+4, oy+4),  GPoint(ox+2, oy+2));  // top-left
    graphics_draw_line(ctx, GPoint(ox+8, oy+4),  GPoint(ox+10,oy+2));  // top-right
    graphics_draw_line(ctx, GPoint(ox+4, oy+8),  GPoint(ox+2, oy+10)); // bottom-left
    graphics_draw_line(ctx, GPoint(ox+8, oy+8),  GPoint(ox+10,oy+10)); // bottom-right
  }
}
static void icon_cloud(GContext *ctx, int ox, int oy, GColor col, bool large) {
  graphics_context_set_fill_color(ctx, col);
  if (large) {
    graphics_fill_rect(ctx,GRect(ox+3,oy+4,5,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+4,oy+2,4,3),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+8,oy+3,5,2),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+1,oy+5,14,7),0,GCornerNone);
  } else {
    graphics_fill_rect(ctx,GRect(ox+2,oy+3,4,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+3,oy+2,3,2),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+6,oy+2,4,2),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+1,oy+4,10,5),0,GCornerNone);
  }
}
static void icon_partly_cloudy(GContext *ctx, int ox, int oy, GColor col, bool large) {
  graphics_context_set_stroke_color(ctx, col); graphics_context_set_stroke_width(ctx, 1);
  if (large) {
    graphics_draw_circle(ctx, GPoint(ox+11,oy+4), 3);
    graphics_draw_pixel(ctx,GPoint(ox+11,oy+0)); graphics_draw_pixel(ctx,GPoint(ox+15,oy+4)); graphics_draw_pixel(ctx,GPoint(ox+11,oy+8));
    graphics_context_set_fill_color(ctx, col);
    graphics_fill_rect(ctx,GRect(ox+2,oy+6,4,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+6,oy+6,3,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+1,oy+7,10,6),0,GCornerNone);
  } else {
    graphics_draw_circle(ctx, GPoint(ox+8,oy+3), 2);
    graphics_draw_pixel(ctx,GPoint(ox+8,oy+0)); graphics_draw_pixel(ctx,GPoint(ox+11,oy+3)); graphics_draw_pixel(ctx,GPoint(ox+8,oy+6));
    graphics_context_set_fill_color(ctx, col);
    graphics_fill_rect(ctx,GRect(ox+1,oy+5,3,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+4,oy+5,2,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+0,oy+6,7,4),0,GCornerNone);
  }
}
static void icon_rain(GContext *ctx, int ox, int oy, GColor col, bool large) {
  icon_cloud(ctx,ox,oy,col,large);
  graphics_context_set_stroke_color(ctx,col); graphics_context_set_stroke_width(ctx,1);
  if (large) {
    graphics_draw_pixel(ctx,GPoint(ox+3,oy+13)); graphics_draw_pixel(ctx,GPoint(ox+3,oy+15));
    graphics_draw_pixel(ctx,GPoint(ox+8,oy+13)); graphics_draw_pixel(ctx,GPoint(ox+8,oy+15));
    graphics_draw_pixel(ctx,GPoint(ox+13,oy+13)); graphics_draw_pixel(ctx,GPoint(ox+13,oy+15));
  } else {
    graphics_draw_pixel(ctx,GPoint(ox+2,oy+10)); graphics_draw_pixel(ctx,GPoint(ox+2,oy+12));
    graphics_draw_pixel(ctx,GPoint(ox+6,oy+10)); graphics_draw_pixel(ctx,GPoint(ox+6,oy+12));
    graphics_draw_pixel(ctx,GPoint(ox+10,oy+10)); graphics_draw_pixel(ctx,GPoint(ox+10,oy+12));
  }
}
static void icon_snow(GContext *ctx, int ox, int oy, GColor col, bool large) {
  graphics_context_set_stroke_color(ctx,col); graphics_context_set_stroke_width(ctx,1);
  int sz = large ? 16 : 12, c = sz/2;
  graphics_draw_line(ctx,GPoint(ox+1,oy+c),GPoint(ox+sz-2,oy+c));
  graphics_draw_line(ctx,GPoint(ox+c,oy+1),GPoint(ox+c,oy+sz-2));
  graphics_draw_line(ctx,GPoint(ox+2,oy+2),GPoint(ox+sz-3,oy+sz-3));
  graphics_draw_line(ctx,GPoint(ox+sz-3,oy+2),GPoint(ox+2,oy+sz-3));
}
static void icon_storm(GContext *ctx, int ox, int oy, GColor col, bool large) {
  icon_cloud(ctx,ox,oy,col,large);
  graphics_context_set_stroke_color(ctx,col); graphics_context_set_stroke_width(ctx,1);
  if (large) {
    graphics_draw_pixel(ctx,GPoint(ox+8,oy+12)); graphics_draw_pixel(ctx,GPoint(ox+8,oy+13));
    graphics_draw_pixel(ctx,GPoint(ox+7,oy+13)); graphics_draw_pixel(ctx,GPoint(ox+7,oy+14));
    graphics_draw_pixel(ctx,GPoint(ox+9,oy+14)); graphics_draw_pixel(ctx,GPoint(ox+9,oy+15));
    graphics_draw_pixel(ctx,GPoint(ox+8,oy+15));
  } else {
    graphics_draw_pixel(ctx,GPoint(ox+6,oy+9));  graphics_draw_pixel(ctx,GPoint(ox+6,oy+10));
    graphics_draw_pixel(ctx,GPoint(ox+5,oy+10)); graphics_draw_pixel(ctx,GPoint(ox+5,oy+11));
    graphics_draw_pixel(ctx,GPoint(ox+7,oy+11));
  }
}
static void icon_haze(GContext *ctx, int ox, int oy, GColor col, bool large) {
  // Three horizontal wavy lines representing hazy/polluted air
  graphics_context_set_stroke_color(ctx, col); graphics_context_set_stroke_width(ctx, 1);
  if (large) {
    graphics_draw_line(ctx, GPoint(ox+1, oy+4),  GPoint(ox+6, oy+4));
    graphics_draw_line(ctx, GPoint(ox+7, oy+5),  GPoint(ox+8, oy+5));
    graphics_draw_line(ctx, GPoint(ox+9, oy+4),  GPoint(ox+14,oy+4));
    graphics_draw_line(ctx, GPoint(ox+1, oy+8),  GPoint(ox+6, oy+8));
    graphics_draw_line(ctx, GPoint(ox+7, oy+9),  GPoint(ox+8, oy+9));
    graphics_draw_line(ctx, GPoint(ox+9, oy+8),  GPoint(ox+14,oy+8));
    graphics_draw_line(ctx, GPoint(ox+1, oy+12), GPoint(ox+6, oy+12));
    graphics_draw_line(ctx, GPoint(ox+7, oy+11), GPoint(ox+8, oy+11));
    graphics_draw_line(ctx, GPoint(ox+9, oy+12), GPoint(ox+14,oy+12));
  } else {
    graphics_draw_line(ctx, GPoint(ox+1, oy+3),  GPoint(ox+4, oy+3));
    graphics_draw_line(ctx, GPoint(ox+5, oy+4),  GPoint(ox+6, oy+4));
    graphics_draw_line(ctx, GPoint(ox+7, oy+3),  GPoint(ox+10,oy+3));
    graphics_draw_line(ctx, GPoint(ox+1, oy+6),  GPoint(ox+4, oy+6));
    graphics_draw_line(ctx, GPoint(ox+5, oy+7),  GPoint(ox+6, oy+7));
    graphics_draw_line(ctx, GPoint(ox+7, oy+6),  GPoint(ox+10,oy+6));
    graphics_draw_line(ctx, GPoint(ox+1, oy+9),  GPoint(ox+4, oy+9));
    graphics_draw_line(ctx, GPoint(ox+5, oy+8),  GPoint(ox+6, oy+8));
    graphics_draw_line(ctx, GPoint(ox+7, oy+9),  GPoint(ox+10,oy+9));
  }
}
static void icon_weather(GContext *ctx, int ox, int oy, GColor col, int code, bool large) {
  if      (code == 0)                                  icon_sun(ctx,ox,oy,col,large);
  else if (code <= 3)                                  icon_partly_cloudy(ctx,ox,oy,col,large);
  else if (code <= 48)                                 icon_cloud(ctx,ox,oy,col,large);
  else if ((code>=51&&code<=69)||(code>=80&&code<=82)) icon_rain(ctx,ox,oy,col,large);
  else if ((code>=71&&code<=77)||(code>=85&&code<=86)) icon_snow(ctx,ox,oy,col,large);
  else if (code >= 95)                                 icon_storm(ctx,ox,oy,col,large);
  else                                                 icon_cloud(ctx,ox,oy,col,large);
}

// ============================================================
// DIGIT DRAWING
// ============================================================
static void fill_arc(GContext *ctx, int cx, int cy, int ro, int ri, int a0, int a1) {
  GRect b = GRect(cx - ro, cy - ro, ro * 2, ro * 2);
  graphics_fill_radial(ctx, b, GOvalScaleModeFitCircle, (uint16_t)(ro - ri),
                       DEG_TO_TRIGANGLE(a0), DEG_TO_TRIGANGLE(a1));
}
static void draw_digit_vec(GContext *ctx, int digit, int slot_x, int cy, int h, int dx, int dy) {
  const int ro = UNIT * 2, ri = UNIT, sw = UNIT;
  int gx = slot_x + HALF_SLOT_PAD + dx, gx_r = gx + GLYPH_W - sw, cap_cx = gx + ro;
  int top_y = cy - h / 2 + dy, bot_y = cy + h / 2 + dy;
  int bar = (h - 7 * UNIT) / 2; if (bar < 0) bar = 0;
  int t_bc = cy - (ro - HALF_UNIT) + dy, t_tc = t_bc - bar;
  int b_tc = cy + (ro - HALF_UNIT) + dy, b_bc = b_tc + bar;
  int tail = (h > H_MIN) ? (h - H_MIN) / 4 : 0;
  int _top_cy = cy - (h - ro*2) / 2 + dy;
  int _bot_cy = cy + (h - ro*2) / 2 + dy;
  #define VBAR(x,y0,y1) if((y1)>(y0)) graphics_fill_rect(ctx,GRect((x),(y0),sw,(y1)-(y0)),0,GCornerNone)
  #define HBAR(y) graphics_fill_rect(ctx,GRect(gx,(y),GLYPH_W,sw),0,GCornerNone)
  #define NUB(x,y) graphics_fill_rect(ctx,GRect((x),(y),sw,sw),0,GCornerNone)
  switch (digit) {
    case 0: fill_arc(ctx,cap_cx,_top_cy,ro,ri,270,450); fill_arc(ctx,cap_cx,_bot_cy,ro,ri,90,270); VBAR(gx,_top_cy,_bot_cy); VBAR(gx_r,_top_cy,_bot_cy); break;
    case 1: { HBAR(bot_y-sw); int sx=gx+GLYPH_W/2-sw/2,cr=sx+sw,dh=cr-gx-dx; VBAR(sx,top_y,bot_y-sw); if(dh>0){GPoint p[5]={{cr-1,top_y},{cr-1,top_y+sw-HALF_UNIT},{gx-1,top_y+sw+dh-HALF_UNIT},{gx-1,top_y+dh-sw},{sx-1,top_y}}; GPathInfo pi={5,p}; GPath*pa=gpath_create(&pi); if(pa){gpath_draw_filled(ctx,pa);gpath_destroy(pa);}} break; }
    case 2: { fill_arc(ctx,cap_cx,_top_cy,ro,ri,270,450); VBAR(gx,_top_cy,_top_cy+tail); VBAR(gx_r,_top_cy,_top_cy+tail); int ddy=(bot_y-sw)-(_top_cy+tail); if(ddy>0){GPoint p[4]={{gx_r-1,_top_cy+tail},{gx_r+sw,_top_cy+tail},{gx-1+sw+1,bot_y-sw},{gx-1,bot_y-sw}}; GPathInfo pi={4,p}; GPath*pa=gpath_create(&pi); if(pa){gpath_draw_filled(ctx,pa);gpath_destroy(pa);}} HBAR(bot_y-sw); break; }
    case 3: fill_arc(ctx,cap_cx,t_tc,ro,ri,270,450); VBAR(gx,t_tc,t_tc+tail); VBAR(gx_r,t_tc,t_bc); fill_arc(ctx,cap_cx,t_bc,ro,ri,90,180); NUB(gx+sw,t_bc+sw); fill_arc(ctx,cap_cx,b_tc,ro,ri,360,450); VBAR(gx,b_bc-tail,b_bc); VBAR(gx_r,b_tc,b_bc); fill_arc(ctx,cap_cx,b_bc,ro,ri,90,270); break;
    case 4: VBAR(gx,top_y,cy-HALF_UNIT+sw+dy); VBAR(gx_r,top_y,bot_y); graphics_fill_rect(ctx,GRect(gx,cy-HALF_UNIT+dy,GLYPH_W,sw),0,GCornerNone); break;
    case 5: HBAR(top_y); VBAR(gx,top_y+sw,b_tc); fill_arc(ctx,cap_cx,b_tc,ro,ri,270,450); fill_arc(ctx,cap_cx,b_bc,ro,ri,90,270); VBAR(gx_r,b_tc,b_bc); if(tail>0)VBAR(gx,b_bc-tail,b_bc); break;
    case 6: fill_arc(ctx,cap_cx,_top_cy,ro,ri,270,450); VBAR(gx_r,_top_cy,_top_cy+tail); VBAR(gx,_top_cy,b_bc); fill_arc(ctx,cap_cx,b_tc,ro,ri,270,450); fill_arc(ctx,cap_cx,b_bc,ro,ri,90,270); VBAR(gx_r,b_tc,b_bc); break;
    case 7: { HBAR(top_y); GPoint p[4]={{gx_r-1,top_y+sw},{gx_r+sw,top_y+sw},{gx+sw,bot_y-1},{gx-1,bot_y-1}}; GPathInfo pi={4,p}; GPath*pa=gpath_create(&pi); if(pa){gpath_draw_filled(ctx,pa);gpath_destroy(pa);} break; }
    case 8: fill_arc(ctx,cap_cx,t_tc,ro,ri,270,450); fill_arc(ctx,cap_cx,t_bc,ro,ri,90,270); VBAR(gx,t_tc,t_bc); VBAR(gx_r,t_tc,t_bc); fill_arc(ctx,cap_cx,b_tc,ro,ri,270,450); fill_arc(ctx,cap_cx,b_bc,ro,ri,90,270); VBAR(gx,b_tc,b_bc); VBAR(gx_r,b_tc,b_bc); break;
    case 9: fill_arc(ctx,cap_cx,t_tc,ro,ri,270,450); fill_arc(ctx,cap_cx,t_bc,ro,ri,90,270); VBAR(gx,t_tc,t_bc); VBAR(gx,_bot_cy-tail,_bot_cy); VBAR(gx_r,t_tc,_bot_cy); fill_arc(ctx,cap_cx,_bot_cy,ro,ri,90,270); break;
    default: break;
  }
  #undef VBAR
  #undef HBAR
  #undef NUB
}

static void draw_colon_dot(GContext *ctx, int slot_x, int cy, int dx, int dy,
                            int ub_top, int ub_bot) {
  int r = UNIT / 2;
  int dot_y = cy + dy;
  if (dot_y + r < ub_top || dot_y - r > ub_bot) return;
  int ddx = slot_x + SLOT_W / 2 + dx;
  graphics_fill_radial(ctx, GRect(ddx - r, dot_y - r, r * 2, r * 2),
    GOvalScaleModeFitCircle, (uint16_t)r, 0, DEG_TO_TRIGANGLE(360));
}
static void draw_colon_vec(GContext *ctx, int slot_x, int cy, int h, int dx, int dy,
                            int ub_top, int ub_bot) {
  draw_colon_dot(ctx, slot_x, cy - h/4 + 2, dx, dy, ub_top, ub_bot);
  draw_colon_dot(ctx, slot_x, cy + h/4 - 2, dx, dy, ub_top, ub_bot);
}
static void draw_colon_split(GContext *ctx, int slot_x,
                              int top_cy, int bot_cy, int dx, int dy,
                              int ub_top, int ub_bot) {
  draw_colon_dot(ctx, slot_x, top_cy, dx, dy, ub_top, ub_bot);
  draw_colon_dot(ctx, slot_x, bot_cy, dx, dy, ub_top, ub_bot);
}
static void draw_pair(GContext *ctx, int tens, int ones, int tens_x, int ones_x,
                       int cy, int h, int dx, int dy, GColor col) {
  graphics_context_set_fill_color(ctx, col);
  draw_digit_vec(ctx, tens, tens_x, cy, h, dx, dy);
  draw_digit_vec(ctx, ones, ones_x, cy, h, dx, dy);
}
static void draw_digits_full(GContext *ctx, int h_tens, int h_ones, int m_tens, int m_ones,
                               int h, int cy, int dx, int dy,
                               int ub_top, int ub_bot,
                               GColor col_h, GColor col_m, GColor col_colon) {
  graphics_context_set_fill_color(ctx, col_h);
  draw_digit_vec(ctx, h_tens, SLOT_H_TENS, cy, h, dx, dy);
  draw_digit_vec(ctx, h_ones, SLOT_H_ONES, cy, h, dx, dy);
  graphics_context_set_fill_color(ctx, col_colon);
  draw_colon_vec(ctx, COLON_SLOT_X, cy, h, dx, dy, ub_top, ub_bot);
  graphics_context_set_fill_color(ctx, col_m);
  draw_digit_vec(ctx, m_tens, SLOT_M_TENS, cy, h, dx, dy);
  draw_digit_vec(ctx, m_ones, SLOT_M_ONES, cy, h, dx, dy);
}
#if defined(PBL_ROUND)
// Round variant: outer digit columns are shorter to follow the circular bezel.
// Each column's height is capped to its circle-inscribed maximum so digits never clip.
// h is the animation height (s_h, driven by s_target_h = ROUND_INNER_H at rest).
static void draw_digits_full_round(GContext *ctx, int h_tens, int h_ones, int m_tens, int m_ones,
                                    int h, int cy, int dx, int dy,
                                    int ub_top, int ub_bot,
                                    GColor col_h, GColor col_m, GColor col_colon) {
  int h_outer = (h < ROUND_OUTER_H) ? h : ROUND_OUTER_H;
  int h_inner = (h < ROUND_INNER_H) ? h : ROUND_INNER_H;
  graphics_context_set_fill_color(ctx, col_h);
  draw_digit_vec(ctx, h_tens, SLOT_H_TENS, cy, h_outer, dx, dy);
  draw_digit_vec(ctx, h_ones, SLOT_H_ONES, cy, h_inner, dx, dy);
  graphics_context_set_fill_color(ctx, col_colon);
  draw_colon_vec(ctx, COLON_SLOT_X, cy, h_inner, dx, dy, ub_top, ub_bot);
  graphics_context_set_fill_color(ctx, col_m);
  draw_digit_vec(ctx, m_tens, SLOT_M_TENS, cy, h_inner, dx, dy);
  draw_digit_vec(ctx, m_ones, SLOT_M_ONES, cy, h_outer, dx, dy);
}
#endif
static void draw_stacked_pass(GContext *ctx, int h_tens, int h_ones, int m_tens, int m_ones,
                               int h, int tens_x, int ones_x,
                               int hr_cy, int mn_cy, int dx, int dy,
                               GColor col_h, GColor col_m) {
  draw_pair(ctx, h_tens, h_ones, tens_x, ones_x, hr_cy, h, dx, dy, col_h);
  draw_pair(ctx, m_tens, m_ones, tens_x, ones_x, mn_cy, h, dx, dy, col_m);
}

// ============================================================
// INFO LINE RENDERING
// ICW = ICON_W, ICG = HALF_UNIT (4px emery, 3px others)
// Stacked text budget with icon: ~75px; without icon: ~99px
// ============================================================
static void draw_info_line(GContext *ctx, InfoLine *line, int y,
                            int col_x, int col_w, GTextAlignment align) {
  GFont font = fonts_get_system_font(INFO_FONT_KEY);
  int iy = y - INFO_TOP_PAD + (INFO_LINE_H - ICON_W) / 2 - ICON_V_ADJUST;
  GColor info_fg = s_fg;
  const int ICW = ICON_W;
  const int ICG = HALF_UNIT;
  if (line->has_icon || line->is_debug_sq) {
    GSize sz = graphics_text_layout_get_content_size(
      line->text, font, GRect(0,0,200,20), GTextOverflowModeFill, GTextAlignmentLeft);
    int block_w = ICW + ICG + sz.w;
    if (block_w > col_w) {
      graphics_context_set_text_color(ctx, info_fg);
      graphics_draw_text(ctx, line->text, font, GRect(col_x, y-INFO_TOP_PAD, col_w, INFO_LINE_H),
        GTextOverflowModeTrailingEllipsis,
        (align == GTextAlignmentRight) ? GTextAlignmentRight : GTextAlignmentLeft, NULL);
      return;
    }
    int icon_sx, text_sx, text_w;
    if (align == GTextAlignmentRight) {
      icon_sx = col_x + col_w - ICW; text_sx = col_x; text_w = col_w - ICW - ICG;
    } else if (align == GTextAlignmentLeft) {
      icon_sx = col_x; text_sx = col_x + ICW + ICG; text_w = col_w - ICW - ICG;
    } else {
      int off = (col_w - block_w) / 2; if (off < 0) off = 0;
      icon_sx = col_x + off; text_sx = icon_sx + ICW + ICG;
      text_w  = (col_x + col_w) - text_sx;
    }
    if (text_w < 0) text_w = 0;
    graphics_context_set_fill_color(ctx, info_fg);
    if (line->is_debug_sq) {
      graphics_fill_rect(ctx, GRect(icon_sx, iy, ICW, ICW), 0, GCornerNone);
    } else if (line->is_battery) {
      icon_battery(ctx, icon_sx, iy, info_fg, line->icon_extra, ICON_LARGE);
    } else if (line->is_weather) {
      icon_weather(ctx, icon_sx, iy, info_fg, line->icon_extra, ICON_LARGE);
    } else {
      line->icon_fn(ctx, icon_sx, iy, info_fg, ICON_LARGE);
    }
    graphics_context_set_text_color(ctx, info_fg);
    if (text_w > 0) {
      GTextAlignment ta = (align == GTextAlignmentRight) ? GTextAlignmentRight : GTextAlignmentLeft;
      graphics_draw_text(ctx, line->text, font, GRect(text_sx, y-INFO_TOP_PAD, text_w, INFO_LINE_H),
        GTextOverflowModeTrailingEllipsis, ta, NULL);
    }
  } else {
    graphics_context_set_text_color(ctx, info_fg);
    graphics_draw_text(ctx, line->text, font, GRect(col_x, y-INFO_TOP_PAD, col_w, INFO_LINE_H),
      GTextOverflowModeTrailingEllipsis, align, NULL);
  }
}

#if defined(PBL_HEALTH)
static void prv_fmt_dist(char *buf, int len) {
  if (s_cfg_dist_mi) { int mx=(s_distance_m*10)/1609; snprintf(buf,len,"%d.%dmi",mx/10,mx%10); }
  else               { int kx=(s_distance_m*10)/1000; snprintf(buf,len,"%d.%dkm",kx/10,kx%10); }
}
static void prv_fmt_steps(char *buf, int len, int steps) {
  if (steps>=1000) snprintf(buf,len,"%d,%03d",steps/1000,steps%1000);
  else snprintf(buf,len,"%d",steps);
}
#endif

static void prv_fmt_time_min(char *buf, int len, int m) {
  if (m<0) { snprintf(buf,len,"--"); return; }
  int h=(m/60)%12; if(!h)h=12;
  snprintf(buf,len,"%d:%02d%s",h,m%60,m<720?"am":"pm");
}

static const char *s_day_names[]        = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
static const char *s_day_short[]        = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *s_month_names[]      = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
static const char *s_month_names_long[] = {"January","February","March","April","May","June",
                                           "July","August","September","October","November","December"};

// ============================================================
// SLOT TEXT GENERATION
// Returns true if text was written; false = hide this slot.
// Philosophy: hide > mislead. Never show zeros or stale defaults.
// ============================================================
static bool prv_slot_text(char *buf, int len, SlotType slot, struct tm *t, bool wide) {
  switch (slot) {
    case SLOT_EMPTY: return false;

    case SLOT_DAY:
      snprintf(buf,len,"%s", t ? s_day_names[t->tm_wday] : "");
      return t != NULL;

    case SLOT_DATE:
      if (t) {
        const char *mon = wide ? s_month_names_long[t->tm_mon] : s_month_names[t->tm_mon];
        snprintf(buf,len,"%s %d", mon, t->tm_mday);
        return true;
      }
      return false;

    case SLOT_DAY_DATE:
      if (t) {
        if (wide) snprintf(buf,len,"%s, %s %d",
                           s_day_names[t->tm_wday], s_month_names_long[t->tm_mon], t->tm_mday);
        else      snprintf(buf,len,"%s, %s %d",
                           s_day_short[t->tm_wday], s_month_names[t->tm_mon], t->tm_mday);
        return true;
      }
      return false;

    case SLOT_TEMP:
      if (s_weather_temp_f > -900) {
        if (s_cfg_temp_f) snprintf(buf,len,"%d F",s_weather_temp_f);
        else snprintf(buf,len,"%d C",s_weather_temp_c);
        return true;
      }
      return false;

    case SLOT_WEATHER: {
      if (s_weather_temp_f <= -900) return false;
      const char *desc = (s_weather_code==0)?"clear":(s_weather_code<=3)?"partly cloudy":
        (s_weather_code<=48)?"foggy":(s_weather_code<=69)?"rainy":
        (s_weather_code<=79)?"snowy":(s_weather_code<=99)?"stormy":"--";
      int t2 = s_cfg_temp_f ? s_weather_temp_f : s_weather_temp_c;
      char u = s_cfg_temp_f ? 'F' : 'C';
      if (wide) snprintf(buf,len,"%d %c, %s",t2,u,desc);
      else      snprintf(buf,len,"%d %c",t2,u);
      return true; }

    case SLOT_STEPS:
#if defined(PBL_HEALTH)
      { char sb[16]; prv_fmt_steps(sb,sizeof(sb),s_steps);
        if (wide) {
          if (s_distance_m>0){char db[16];prv_fmt_dist(db,sizeof(db));snprintf(buf,len,"%s steps"DOT"%s",sb,db);}
          else snprintf(buf,len,"%s steps",sb);
        } else { snprintf(buf,len,"%s",sb); }
      }
      return true;
#else
      return false;
#endif

    case SLOT_DISTANCE:
#if defined(PBL_HEALTH)
      { char db[16]; prv_fmt_dist(db,sizeof(db)); snprintf(buf,len,"%s",db); }
      return true;
#else
      return false;
#endif

    case SLOT_EXP_STEPS:
#if defined(PBL_HEALTH)
      // Typical steps at current time of day (same-day-of-week SDK average)
      if (s_steps_expected>0) { char eb[16]; prv_fmt_steps(eb,sizeof(eb),s_steps_expected); snprintf(buf,len,"typ %s",eb); return true; }
      return false;
#else
      return false;
#endif

    case SLOT_PACE:
#if defined(PBL_HEALTH)
      if (s_steps_expected>0) {
        if (wide) { char eb[16]; prv_fmt_steps(eb,sizeof(eb),s_steps_expected);
          snprintf(buf,len,"typ %s"DOT"%d%%",eb,(s_steps*100)/s_steps_expected); }
        else snprintf(buf,len,"%d%%",(s_steps*100)/s_steps_expected);
        return true;
      }
      return false;
#else
      return false;
#endif

    case SLOT_CALORIES:
#if defined(PBL_HEALTH)
      { if (wide) {
          if (s_heart_rate > 0) snprintf(buf,len,"%d cal"DOT"%d bpm",s_calories,s_heart_rate);
          else snprintf(buf,len,"%d cal",s_calories);
        } else snprintf(buf,len,"%d",s_calories); }
      return true;
#else
      return false;
#endif

    case SLOT_HEART:
#if defined(PBL_HEALTH)
      if (s_heart_rate_valid || s_heart_rate > 0) {
        if (wide) snprintf(buf,len,"%d bpm", s_heart_rate);
        else snprintf(buf,len,"%d", s_heart_rate);
      } else {
        snprintf(buf,len, wide ? "-- bpm" : "--");
      }
      return true;
#else
      return false;
#endif

    case SLOT_SUNRISE:
      if (s_sunrise_min < 0) return false;
      { char rb[12]; prv_fmt_time_min(rb,sizeof(rb),s_sunrise_min); snprintf(buf,len,"%s",rb); }
      return true;

    case SLOT_SUNSET:
      if (s_sunset_min < 0) return false;
      { char sb2[12]; prv_fmt_time_min(sb2,sizeof(sb2),s_sunset_min); snprintf(buf,len,"%s",sb2); }
      return true;

    case SLOT_DAYLIGHT:
      if (s_sunrise_min<0 || s_sunset_min<0) return false;
      { int mins=s_sunset_min-s_sunrise_min; if(mins<0)mins=0;
        snprintf(buf,len,"%dh%02dm",mins/60,mins%60); }
      return true;

    case SLOT_BATTERY:
      if (s_charging) snprintf(buf,len,"%d%% +",s_battery_pct);
      else            snprintf(buf,len,"%d%%",s_battery_pct);
      return true;

    case SLOT_BLUETOOTH:
      if (s_bt_connected) return false;
      snprintf(buf,len,"no bt");
      return true;

    case SLOT_SUN_TIMES: {
      if (s_sunrise_min < 0 || s_sunset_min < 0) return false;
      char rb[12], sb2[12];
      prv_fmt_time_min(rb, sizeof(rb), s_sunrise_min);
      prv_fmt_time_min(sb2, sizeof(sb2), s_sunset_min);
      if (wide) snprintf(buf,len,"%s"DOT"%s",rb,sb2);
      else      snprintf(buf,len,"%s",rb);
      return true; }

    case SLOT_STEPS_EXP:
#if defined(PBL_HEALTH)
      if (!wide) return false;
      if (s_steps_expected<=0) return false;
      { char sb[16], eb[16];
        prv_fmt_steps(sb,sizeof(sb),s_steps);
        prv_fmt_steps(eb,sizeof(eb),s_steps_expected);
        snprintf(buf,len,"%s steps"DOT"typ %s",sb,eb); }
      return true;
#else
      return false;
#endif

    case SLOT_EXP_PACE:
#if defined(PBL_HEALTH)
      if (!wide) return false;
      if (s_steps_expected<=0) return false;
      { char eb[16]; prv_fmt_steps(eb,sizeof(eb),s_steps_expected);
        snprintf(buf,len,"typ %s"DOT"%d%%",eb,(s_steps*100)/s_steps_expected); }
      return true;
#else
      return false;
#endif

    case SLOT_BAT_BT:
      if (!wide) return false;
      if (!s_bt_connected) {
        if (s_charging) snprintf(buf,len,"%d%% +"DOT"no bt",s_battery_pct);
        else            snprintf(buf,len,"%d%%"DOT"no bt",s_battery_pct);
      } else {
        if (s_charging) snprintf(buf,len,"%d%% +",s_battery_pct);
        else            snprintf(buf,len,"%d%%",s_battery_pct);
      }
      return true;

    case SLOT_UV:
      if (s_uv_index < 0) return false;
      { if (wide) snprintf(buf,len,"UV index %d",s_uv_index);
        else      snprintf(buf,len,"UV %d",s_uv_index); }
      return true;

    case SLOT_LIGHT_REM: {
      if (s_sunrise_min < 0 || s_sunset_min < 0) return false;
      time_t now_ts = time(NULL); struct tm *tn = localtime(&now_ts);
      int now_min = tn->tm_hour * 60 + tn->tm_min;
      int rem = -1; bool is_day = false;
      if (now_min < s_sunrise_min)     { rem = s_sunrise_min - now_min; }
      else if (now_min < s_sunset_min) { rem = s_sunset_min - now_min; is_day = true; }
      else if (s_sunrise_tom_min >= 0) { rem = (24*60 - now_min) + s_sunrise_tom_min; }
      else                             { rem = (24*60 - now_min) + s_sunrise_min; }
      if (rem < 0) return false;
      int rh = rem / 60, rm = rem % 60;
      if (wide) {
        if (is_day) snprintf(buf,len,"%dh %02dm daylight left",rh,rm);
        else        snprintf(buf,len,"%dh %02dm til sunrise",rh,rm);
      } else {
        // Stacked budget ~75px with icon; label text overflows, icon carries day/night signal
        snprintf(buf,len,"%dh%02dm",rh,rm);
      }
      return true; }

    case SLOT_UV_LIGHT: {
      if (!wide) return false;
      if (s_uv_index < 0) return false;
      if (s_sunrise_min < 0 || s_sunset_min < 0) return false;
      time_t now_ts = time(NULL); struct tm *tn = localtime(&now_ts);
      int now_min = tn->tm_hour * 60 + tn->tm_min;
      int rem = -1; bool is_day = false;
      if (now_min < s_sunrise_min)     { rem = s_sunrise_min - now_min; }
      else if (now_min < s_sunset_min) { rem = s_sunset_min - now_min; is_day = true; }
      else if (s_sunrise_tom_min >= 0) { rem = (24*60 - now_min) + s_sunrise_tom_min; }
      else                             { rem = (24*60 - now_min) + s_sunrise_min; }
      if (rem < 0) return false;
      int rh = rem / 60, rm2 = rem % 60;
      if (is_day) snprintf(buf,len,"UV %d"DOT"%dh %02dm light",s_uv_index,rh,rm2);
      else        snprintf(buf,len,"UV %d"DOT"%dh %02dm dark",s_uv_index,rh,rm2);
      return true; }

    case SLOT_WEATHER_UV: {
      if (!wide) return false;
      if (s_weather_temp_f <= -900) return false;
      if (s_uv_index < 0) return false;
      int t2 = s_cfg_temp_f ? s_weather_temp_f : s_weather_temp_c;
      char u = s_cfg_temp_f ? 'F' : 'C';
      snprintf(buf,len,"%d %c"DOT"UV %d",t2,u,s_uv_index);
      return true; }

    case SLOT_TYPICAL_DAY:
#if defined(PBL_HEALTH)
      // Typical full-day step total -- same day-of-week average (HealthServiceTimeScopeWeekly)
      if (s_steps_typical_day <= 0) return false;
      { char gb[16]; prv_fmt_steps(gb, sizeof(gb), s_steps_typical_day);
        if (wide) snprintf(buf,len,"typical %s steps",gb);
        else      snprintf(buf,len,"typ %s",gb); }
      return true;
#else
      return false;
#endif

    case SLOT_CALORIES_TOT:
#if defined(PBL_HEALTH)
      // Active + resting (BMR) calories. Resting is typically 4-5x active.
      // Wide: show both; stacked: show total
      { int total = s_calories + s_calories_resting;
        if (total <= 0) return false;
        if (wide) {
          if (s_calories_resting > 0)
            snprintf(buf,len,"%d active"DOT"%d resting",s_calories,s_calories_resting);
          else
            snprintf(buf,len,"%d cal",total);
        } else {
          snprintf(buf,len,"%d",total);
        }
      }
      return true;
#else
      return false;
#endif

    case SLOT_SLEEP:
#if defined(PBL_HEALTH)
      // Sleep last night (sum from 6pm yesterday to now, HealthMetricSleepSeconds)
      if (s_sleep_seconds <= 0) return false;
      { int sh = s_sleep_seconds / 3600, sm = (s_sleep_seconds % 3600) / 60;
        if (wide) snprintf(buf,len,"%dh %02dm sleep",sh,sm);
        else      snprintf(buf,len,"%dh%02dm",sh,sm); }
      return true;
#else
      return false;
#endif

    case SLOT_AQI:
      // Air Quality Index (US EPA scale: 0-50 Good, 51-100 Moderate, 101-150 USG, 151+ Unhealthy)
      if (s_aqi < 0) return false;
      { if (wide) snprintf(buf,len,"AQI %d",s_aqi);
        else      snprintf(buf,len,"AQI %d",s_aqi); }
      return true;

    case SLOT_TEMP_UV_AQI:
      // Wide-only: temperature + UV index + AQI -- gracefully degrades if any is missing
      if (!wide) return false;
      if (s_weather_temp_f <= -900) return false;
      { int t2 = s_cfg_temp_f ? s_weather_temp_f : s_weather_temp_c;
        char u = s_cfg_temp_f ? 'F' : 'C';
        if (s_uv_index >= 0 && s_aqi >= 0)
          snprintf(buf,len,"%d %c"DOT"UV %d"DOT"AQI %d",t2,u,s_uv_index,s_aqi);
        else if (s_uv_index >= 0)
          snprintf(buf,len,"%d %c"DOT"UV %d",t2,u,s_uv_index);
        else if (s_aqi >= 0)
          snprintf(buf,len,"%d %c"DOT"AQI %d",t2,u,s_aqi);
        else
          snprintf(buf,len,"%d %c",t2,u);
      }
      return true;

    case SLOT_DEBUG:
      if (wide) snprintf(buf, len, "[ Tallboy Debug Text ]");
      else       snprintf(buf, len, "[ Debug ]");
      return true;

    default: return false;
  }
}

static IconFn prv_slot_icon(SlotType slot, bool *is_battery, bool *is_weather, bool *is_debug_sq, int *extra) {
  *is_battery=false; *is_weather=false; *is_debug_sq=false; *extra=0;
  switch(slot) {
    case SLOT_STEPS: case SLOT_DISTANCE: case SLOT_EXP_STEPS: case SLOT_PACE:
    case SLOT_STEPS_EXP: case SLOT_EXP_PACE: case SLOT_TYPICAL_DAY: return icon_steps;
    case SLOT_CALORIES: case SLOT_CALORIES_TOT: return icon_calories;
    case SLOT_HEART: return icon_heart;
    case SLOT_SUNRISE: case SLOT_SUNSET: case SLOT_DAYLIGHT: case SLOT_SUN_TIMES:
    case SLOT_LIGHT_REM: case SLOT_SLEEP: return icon_sun;
    case SLOT_BATTERY: case SLOT_BAT_BT: *is_battery=true; *extra=s_battery_pct; return NULL;
    case SLOT_BLUETOOTH: return icon_bt;
    case SLOT_TEMP: case SLOT_WEATHER:
    case SLOT_UV: case SLOT_UV_LIGHT: case SLOT_WEATHER_UV: case SLOT_TEMP_UV_AQI:
      *is_weather=true; *extra=s_weather_code; return NULL;
    case SLOT_AQI: return icon_haze;
    case SLOT_DEBUG: *is_debug_sq=true; return NULL;
    default: return NULL;
  }
}

static int build_lines(InfoLine *lines, int max, int *slots, int n_slots, struct tm *t, bool wide) {
  int count = 0;
  for (int i = 0; i < n_slots && count < max; i++) {
    SlotType slot = (SlotType)slots[i];
    if (slot == SLOT_EMPTY) continue;
    InfoLine *line = &lines[count];
    char buf[INFO_LINE_BUF];
    if (!prv_slot_text(buf, sizeof(buf), slot, t, wide)) continue;
    snprintf(line->text, INFO_LINE_BUF, "%s", buf);
    bool ib, iw, id; int ie;
    line->icon_fn    = prv_slot_icon(slot, &ib, &iw, &id, &ie);
    line->is_battery = ib; line->is_weather = iw; line->is_debug_sq = id; line->icon_extra = ie;
    line->has_icon   = (line->icon_fn != NULL || ib || iw);
    count++;
  }
  return count;
}

static int prv_info_block_h(int n, int step) { return n <= 0 ? 0 : INFO_GLYPH_H + (n-1) * step; }

#if defined(PBL_ROUND)
// ------------------------------------------------------------
// Round geometry helpers (v3.67)
// Column bound = the max digit height a column can take inside the bezel (outer or inner pair).
// Stacked rows: the top row's top and the bottom row's bottom sit at the column's full-view bound;
// the other ends meet at the middle gap (UNIT total, HALF_UNIT each side of centre). Row heights are
// forced even (draw_digit_vec uses h/2 on both sides), so a column whose bound leaves an odd row
// height starts 1px inside its bound; the middle gap edges stay aligned across columns.
// ------------------------------------------------------------
static int prv_isqrt(int n) {
  if (n <= 0) return 0;
  int x = n, y = (x + 1) / 2;
  while (y < x) { x = y; y = (x + n / x) / 2; }
  return x;
}
// Half-width of the screen chord at the worse (farther from centre) of two y values.
static int prv_round_chord_half(int y0, int y1) {
  int R = SCREEN_W / 2, c = SCREEN_H / 2;
  int d0 = y0 - c; if (d0 < 0) d0 = -d0;
  int d1 = y1 - c; if (d1 < 0) d1 = -d1;
  int d = (d0 > d1) ? d0 : d1;
  return prv_isqrt(R * R - d * d);
}
static int prv_round_bound(int slot_x) {
  return (slot_x == SLOT_H_TENS || slot_x == SLOT_M_ONES) ? ROUND_OUTER_H : ROUND_INNER_H;
}
static int prv_round_row_h(int bound) { return (bound / 2 - HALF_UNIT) & ~1; }
static int prv_round_row_cy(int bound, bool top_row, int cy_mid) {
  int hr = prv_round_row_h(bound);
  return top_row ? (cy_mid - HALF_UNIT - hr / 2) : (cy_mid + HALF_UNIT + hr / 2);
}
// Height of a wide-mode digit zone on round, from line counts (info blocks inset to fit the chord).
static int prv_round_wide_h(int ub_h, int an, int bn);
#endif
#if defined(PBL_ROUND)
static int prv_round_wide_h(int ub_h, int an, int bn) {
  int top_res = (an > 0) ? ROUND_INFO_INSET + prv_info_block_h(an, INFO_LINE_STEP_WIDE) + HALF_UNIT : 0;
  int bot_res = (bn > 0) ? ROUND_INFO_INSET + prv_info_block_h(bn, INFO_LINE_STEP_WIDE) + HALF_UNIT + WIDE_BELOW_EXTRA : 0;
  int h = ub_h - top_res - bot_res;
  if (h > ROUND_INNER_H) h = ROUND_INNER_H;
  return h < H_MIN ? H_MIN : h;
}
#endif

static int prv_compute_target_h(int ub_h, int layout) {
  // For wide layout, uses configured slot count as a conservative estimate.
  // draw_layer refines this live using actual rendered count (slots that hide due
  // to missing data collapse, giving digits more space automatically).
#if defined(PBL_ROUND)
  // On round, target height is the inner column max (inner pair fits the circle at dx=30).
  // Outer columns cap themselves independently in draw_digits_full_round().
  if (layout == LAYOUT_INFO) {
    int na = 0, nb = 0;
    for (int i = 0; i < 3 && na < ROUND_WIDE_LINES; i++) if (s_cfg_wide[i] != SLOT_EMPTY) na++;
    for (int i = 3; i < 6 && nb < ROUND_WIDE_LINES; i++) if (s_cfg_wide[i] != SLOT_EMPTY) nb++;
    return prv_round_wide_h(ub_h, na, nb);
  }
  return ROUND_INNER_H;   // stacked layouts use prv_compute_stacked_h
#endif
  if (layout != LAYOUT_INFO) return ub_h - MARGIN_OUTER - BOTTOM_MARGIN(ub_h);
  int n_above = 0, n_below = 0;
  for (int i = 0; i < 3; i++) if (s_cfg_wide[i] != SLOT_EMPTY) n_above++;
  for (int i = 3; i < 6; i++) if (s_cfg_wide[i] != SLOT_EMPTY) n_below++;
  int reserved = HALF_UNIT + prv_info_block_h(n_above, INFO_LINE_STEP_WIDE) + HALF_UNIT
               + HALF_UNIT + WIDE_BELOW_EXTRA
               + prv_info_block_h(n_below, INFO_LINE_STEP_WIDE) + HALF_UNIT;
  int h = ub_h - reserved;
  return h < H_MIN ? H_MIN : h;
}

static int prv_compute_stacked_h(int ub_h) {
#if defined(PBL_ROUND)
  (void)ub_h;
  return prv_round_row_h(ROUND_INNER_H);   // tallest stacked row; outer-column rows are shorter
#endif
  int h = (ub_h - MARGIN_OUTER - BOTTOM_MARGIN(ub_h) - UNIT) / 2;
  return h < H_ABSOLUTE_MIN ? H_ABSOLUTE_MIN : h;
}

static void prv_update_targets(void) {
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  s_target_h = prv_compute_target_h(ub.size.h, s_layout);
}

static void prv_stacked_geom(int layout, int *tens_x, int *ones_x,
                               int *col_x, int *col_w, GTextAlignment *align) {
  if (layout == LAYOUT_STACK_R) {
    *ones_x = SCREEN_W - SIDE_MARGIN - SLOT_W;
    *tens_x = *ones_x - SLOT_W;
    *col_x  = 1;
    *col_w  = *tens_x - UNIT - 1;
    *align  = GTextAlignmentRight;
  } else {
    *tens_x = SIDE_MARGIN;
    *ones_x = SIDE_MARGIN + SLOT_W;
    *col_x  = *ones_x + SLOT_W + UNIT;
    *col_w  = SCREEN_W - *col_x - 1;
    *align  = GTextAlignmentLeft;
  }
}

#if defined(PBL_ROUND)
// Round digit drawing for stacked layouts and the split/merge transitions (v3.67).
// Digits are indexed 0 h_tens, 1 h_ones, 2 m_tens, 3 m_ones; each column has its own height and centre.
static void prv_round_draw_digit4(GContext *ctx, const int *dv, const int *x, const int *h, const int *cy,
                                  int dx, int dy, GColor col_h, GColor col_m) {
  for (int d = 0; d < 4; d++) {
    graphics_context_set_fill_color(ctx, d < 2 ? col_h : col_m);
    draw_digit_vec(ctx, dv[d], x[d], cy[d], h[d], dx, dy);
  }
}

static void prv_round_stacked_draw(GContext *ctx, int layout, int cy_mid, int anim_h,
                                   const int *dv, int dx, int dy, GColor col_h, GColor col_m) {
  int tens_x, ones_x, cxf, cw; GTextAlignment al;
  prv_stacked_geom(layout, &tens_x, &ones_x, &cxf, &cw, &al);
  int x[4], h[4], cy[4];
  for (int d = 0; d < 4; d++) {
    bool top = (d < 2);
    x[d] = (d & 1) ? ones_x : tens_x;
    int b  = prv_round_bound(x[d]);
    int hr = prv_round_row_h(b);
    h[d]  = (anim_h > 0 && anim_h < hr) ? anim_h : hr;
    cy[d] = prv_round_row_cy(b, top, cy_mid);
    // STACK_R fold: the pinned edge stays put, so the centre drifts toward it as the row shrinks
    if (anim_h > 0 && layout == LAYOUT_STACK_R) {
      int drift = (hr - h[d]) / 2;
      cy[d] += top ? drift : -drift;
    }
  }
  prv_round_draw_digit4(ctx, dv, x, h, cy, dx, dy, col_h, col_m);
}

// Split/merge transitions: vertical phases keep the full-view x and move each digit into its stacked
// row (height/centre computed for its own full-view column); horizontal phases then slide it to the
// stacked column and re-fit the height to that column. Everything is continuous at the phase edges.
static void prv_round_split_draw(GContext *ctx, Phase ph, int step, int tl, int cy_mid,
                                 const int *dv, int dx, int dy, GColor col_h, GColor col_m) {
  static const int FX[4] = { SLOT_H_TENS, SLOT_H_ONES, SLOT_M_TENS, SLOT_M_ONES };
  int tens_x, ones_x, cxf, cw; GTextAlignment al;
  prv_stacked_geom(tl, &tens_x, &ones_x, &cxf, &cw, &al);
  int x[4], h[4], cy[4];
  for (int d = 0; d < 4; d++) {
    bool top = (d < 2);
    int sx = (d & 1) ? ones_x : tens_x;
    int fb = prv_round_bound(FX[d]), sb = prv_round_bound(sx);
    int v_h = prv_round_row_h(fb),  v_cy = prv_round_row_cy(fb, top, cy_mid);
    int s_h = prv_round_row_h(sb),  s_cy = prv_round_row_cy(sb, top, cy_mid);
    switch (ph) {
      case PHASE_SPLIT_V:
        x[d] = FX[d]; h[d] = prv_lerp_v(fb, v_h, step); cy[d] = prv_lerp_v(cy_mid, v_cy, step); break;
      case PHASE_MERGE_V:
        x[d] = FX[d]; h[d] = prv_lerp_v(v_h, fb, step); cy[d] = prv_lerp_v(v_cy, cy_mid, step); break;
      case PHASE_SPLIT_H:
        x[d] = prv_lerp_h(FX[d], sx, step); h[d] = prv_lerp_h(v_h, s_h, step); cy[d] = prv_lerp_h(v_cy, s_cy, step); break;
      default: /* PHASE_MERGE_H */
        x[d] = prv_lerp_h(sx, FX[d], step); h[d] = prv_lerp_h(s_h, v_h, step); cy[d] = prv_lerp_h(s_cy, v_cy, step); break;
    }
  }
  prv_round_draw_digit4(ctx, dv, x, h, cy, dx, dy, col_h, col_m);
}
#endif

// col_x is the column's CURRENT x (it slides during the split); col_x_final is where it ends up.
static void prv_draw_stacked_info(GContext *ctx, struct tm *tm_now,
                                   int col_x, int col_w, GTextAlignment align,
                                   int col_x_final,
                                   int ub_top, int ub_bot, int ub_h) {
  if (!tm_now || col_w <= 20) return;
#if defined(PBL_ROUND)
  {
  (void)ub_top; (void)ub_bot; (void)ub_h;
  int cn = build_lines(s_col_lines, ROUND_STACK_LINES, s_cfg_stack, STACK_SLOTS, tm_now, false);
  if (cn <= 0) return;
  // Lines span the outer columns' full-view extent; each is clamped to the bezel chord (1 UNIT margin).
  int top = SCREEN_H / 2 - ROUND_OUTER_H / 2;
  int avail_h = ROUND_OUTER_H - INFO_GLYPH_H;
  for (int i = 0; i < cn; i++) {
    int y = top + (cn > 1 ? (i * avail_h) / (cn - 1) : avail_h / 2);
    int half = prv_round_chord_half(y, y + INFO_GLYPH_H) - UNIT;
    int lx, lw;
    if (align == GTextAlignmentRight) {
      int left = SCREEN_W / 2 - half;
      lx = (left > col_x_final) ? left : col_x_final;
      lw = (col_x_final + col_w) - lx;
    } else {
      lx = col_x_final;
      lw = (SCREEN_W / 2 + half) - lx;
      if (lw > col_w) lw = col_w;
    }
    if (lw <= 20) continue;
    draw_info_line(ctx, &s_col_lines[i], y, lx + (col_x - col_x_final), lw, align);
  }
  return;
  }
#endif
  int cn = build_lines(s_col_lines, INFO_LINES_MAX, s_cfg_stack, STACK_SLOTS, tm_now, false);
  if (cn <= 0) return;
  int draw_top = ub_top + STACK_INFO_TOP_MARGIN;
  int draw_bot = ub_bot - BOTTOM_MARGIN(ub_h);
  int avail_h  = draw_bot - draw_top - INFO_GLYPH_H;
  for (int i = 0; i < cn; i++) {
    int y = draw_top + (cn > 1 && avail_h > 0 ? (i * avail_h) / (cn - 1) : 0);
    draw_info_line(ctx, &s_col_lines[i], y, col_x, col_w, align);
  }
}

// ============================================================
// DRAW LAYER
// ============================================================
static void draw_layer(Layer *layer, GContext *ctx) {
  Layer *root  = window_get_root_layer(s_window);
  GRect ub     = layer_get_unobstructed_bounds(root);
  int ub_top   = ub.origin.y, ub_h = ub.size.h, ub_bot = ub_top + ub_h;
  GRect bounds = layer_get_bounds(layer);

  GColor bg;
#if defined(PBL_COLOR)
  if (s_cfg_color_mode == COLOR_MODE_STATIC) {
    bg           = prv_color_from_idx(s_cfg_col_bg);
    s_fg         = prv_color_from_idx(s_cfg_col_info);
    s_fg_hr      = prv_color_from_idx(s_cfg_col_dig_h);
    s_fg_mn      = prv_color_from_idx(s_cfg_col_dig_m);
    s_fg_colon   = prv_color_from_idx(s_cfg_col_colon);
    s_shadow_col = prv_color_from_idx(s_cfg_col_shadow);
  } else {
    // Pace-based modes: DYNAMIC (compat), PACE_DARK, PACE_MED, PACE_LIGHT
    int pace_mode = s_cfg_color_mode;  // all non-STATIC modes use pace palette
#if defined(PBL_HEALTH)
    bg = prv_pace_color(s_steps, s_steps_expected, pace_mode);
#else
    bg = GColorBlack;
#endif
    bool dark = prv_pace_mode_needs_dark_fg(pace_mode);
    s_fg = dark ? GColorBlack : GColorWhite;
    s_fg_hr = s_fg; s_fg_mn = s_fg; s_fg_colon = s_fg;
    s_shadow_col = dark ? GColorWhite : GColorBlack;
  }
#else
  bg = s_cfg_invert ? GColorWhite : GColorBlack;
  s_fg = s_cfg_invert ? GColorBlack : GColorWhite;
  s_fg_hr = s_fg; s_fg_mn = s_fg; s_fg_colon = s_fg;
  s_shadow_col = s_cfg_invert ? GColorWhite : GColorBlack;
#endif

  // Black base fills the whole layer; rounded rect clips the colored background.
  // Radius UNIT*2 (16px on emery) matches the digit cap stroke weight.
  // On round platforms the OS clips to a circle automatically; no rounded rect needed.
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_fill_color(ctx, bg);
#if defined(PBL_ROUND)
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
#else
  graphics_fill_rect(ctx, bounds, UNIT * 2, GCornersAll);
#endif

  int hr_disp = s_hour;
  if (!clock_is_24h_style()) { hr_disp = s_hour % 12; if (!hr_disp) hr_disp = 12; }
  int h_tens=hr_disp/10, h_ones=hr_disp%10, m_tens=s_minute/10, m_ones=s_minute%10;
  int bot_margin = BOTTOM_MARGIN(ub_h);
  time_t now_t = time(NULL);
  struct tm *tm_now = localtime(&now_t);

  if (s_phase == PHASE_SPLIT_V || s_phase == PHASE_MERGE_V) {
    int step   = s_anim_step;
    int hr_cy  = prv_lerp_v(s_sv_hr_cy_s,   s_sv_hr_cy_e,   step);
    int mn_cy  = prv_lerp_v(s_sv_mn_cy_s,   s_sv_mn_cy_e,   step);
    int dh     = prv_lerp_v(s_sv_h_s,       s_sv_h_e,       step);
    int top_cy = prv_lerp_v(s_sv_top_dot_s, s_sv_top_dot_e, step);
    int bot_cy = prv_lerp_v(s_sv_bot_dot_s, s_sv_bot_dot_e, step);
    graphics_context_set_fill_color(ctx, s_shadow_col);
    draw_colon_split(ctx, COLON_SLOT_X, top_cy, bot_cy, SHADOW_DX, SHADOW_DY, ub_top, ub_bot);
    graphics_context_set_fill_color(ctx, s_fg_colon);
    draw_colon_split(ctx, COLON_SLOT_X, top_cy, bot_cy, 0, 0, ub_top, ub_bot);
#if defined(PBL_ROUND)
    { int dv[4] = { h_tens, h_ones, m_tens, m_ones };
      int cy_mid = ub_top + ub_h / 2;
      (void)hr_cy; (void)mn_cy; (void)dh;
      prv_round_split_draw(ctx, s_phase, step, s_split_target_layout, cy_mid, dv, SHADOW_DX, SHADOW_DY, s_shadow_col, s_shadow_col);
      prv_round_split_draw(ctx, s_phase, step, s_split_target_layout, cy_mid, dv, 0, 0, s_fg_hr, s_fg_mn); }
#else
    draw_pair(ctx, h_tens, h_ones, SLOT_H_TENS, SLOT_H_ONES, hr_cy, dh, SHADOW_DX, SHADOW_DY, s_shadow_col);
    draw_pair(ctx, m_tens, m_ones, SLOT_M_TENS, SLOT_M_ONES, mn_cy, dh, SHADOW_DX, SHADOW_DY, s_shadow_col);
    draw_pair(ctx, h_tens, h_ones, SLOT_H_TENS, SLOT_H_ONES, hr_cy, dh, 0, 0, s_fg_hr);
    draw_pair(ctx, m_tens, m_ones, SLOT_M_TENS, SLOT_M_ONES, mn_cy, dh, 0, 0, s_fg_mn);
#endif
    return;
  }

  if (s_phase == PHASE_SPLIT_H || s_phase == PHASE_MERGE_H) {
    int step      = s_anim_step;
    int hr_tens_x = prv_lerp_h(s_sh_hr_tx_s, s_sh_hr_tx_e, step);
    int mn_tens_x = prv_lerp_h(s_sh_mn_tx_s, s_sh_mn_tx_e, step);
    int col_x     = prv_lerp_h(s_sh_col_s,   s_sh_col_e,   step);
    int tl = s_split_target_layout;
    int dummy_tx, dummy_ox, col_x_final, col_w; GTextAlignment info_align;
    prv_stacked_geom(tl, &dummy_tx, &dummy_ox, &col_x_final, &col_w, &info_align);
    prv_draw_stacked_info(ctx, tm_now, col_x, col_w, info_align, col_x_final, ub_top, ub_bot, ub_h);
#if defined(PBL_ROUND)
    { int dv[4] = { h_tens, h_ones, m_tens, m_ones };
      int cy_mid = ub_top + ub_h / 2;
      (void)hr_tens_x; (void)mn_tens_x;
      prv_round_split_draw(ctx, s_phase, step, tl, cy_mid, dv, SHADOW_DX, SHADOW_DY, s_shadow_col, s_shadow_col);
      prv_round_split_draw(ctx, s_phase, step, tl, cy_mid, dv, 0, 0, s_fg_hr, s_fg_mn); }
#else
    draw_pair(ctx, h_tens, h_ones, hr_tens_x, hr_tens_x+SLOT_W, s_sh_hr_cy, s_sh_h, SHADOW_DX, SHADOW_DY, s_shadow_col);
    draw_pair(ctx, m_tens, m_ones, mn_tens_x, mn_tens_x+SLOT_W, s_sh_mn_cy, s_sh_h, SHADOW_DX, SHADOW_DY, s_shadow_col);
    draw_pair(ctx, h_tens, h_ones, hr_tens_x, hr_tens_x+SLOT_W, s_sh_hr_cy, s_sh_h, 0, 0, s_fg_hr);
    draw_pair(ctx, m_tens, m_ones, mn_tens_x, mn_tens_x+SLOT_W, s_sh_mn_cy, s_sh_h, 0, 0, s_fg_mn);
#endif
    return;
  }

  if (s_layout == LAYOUT_FULL) {
    s_info_slide = 0;
#if defined(PBL_ROUND)
    // On round, center digits at the true midpoint of the unobstructed area.
    // The outer/inner heights differ per column, so MARGIN_OUTER + h/2 would be off-center.
    int cy = ub_top + ub_h / 2;
    draw_digits_full_round(ctx, h_tens, h_ones, m_tens, m_ones, s_h, cy, SHADOW_DX, SHADOW_DY, ub_top, ub_bot, s_shadow_col, s_shadow_col, s_shadow_col);
    draw_digits_full_round(ctx, h_tens, h_ones, m_tens, m_ones, s_h, cy, 0, 0, ub_top, ub_bot, s_fg_hr, s_fg_mn, s_fg_colon);
#else
    int cy = ub_top + MARGIN_OUTER + s_target_h / 2;
    draw_digits_full(ctx, h_tens, h_ones, m_tens, m_ones, s_h, cy, SHADOW_DX, SHADOW_DY, ub_top, ub_bot, s_shadow_col, s_shadow_col, s_shadow_col);
    draw_digits_full(ctx, h_tens, h_ones, m_tens, m_ones, s_h, cy, 0, 0, ub_top, ub_bot, s_fg_hr, s_fg_mn, s_fg_colon);
#endif

  } else if (s_layout == LAYOUT_INFO) {
    int above_s[3], below_s[3];
    for (int i = 0; i < 3; i++) above_s[i] = s_cfg_wide[i];
    for (int i = 0; i < 3; i++) below_s[i] = s_cfg_wide[3+i];
#if defined(PBL_ROUND)
    // Round: short-format lines only (wide=false), capped at ROUND_WIDE_LINES per side, inset so each
    // line fits the bezel chord. Digits fill the zone between the blocks (columns cap themselves).
    int an = build_lines(s_above_lines, ROUND_WIDE_LINES, above_s, 3, tm_now, false);
    int bn = build_lines(s_below_lines, ROUND_WIDE_LINES, below_s, 3, tm_now, false);
    int above_start = ub_top + ROUND_INFO_INSET;
    int above_end   = above_start + prv_info_block_h(an, INFO_LINE_STEP_WIDE);
    int below_end   = ub_bot - ROUND_INFO_INSET;
    int below_top   = below_end - prv_info_block_h(bn, INFO_LINE_STEP_WIDE);
    int zone_top    = (an > 0) ? above_end + HALF_UNIT : ub_top;
    int zone_bot    = (bn > 0) ? below_top - HALF_UNIT - WIDE_BELOW_EXTRA : ub_bot;
    int actual_h    = prv_round_wide_h(ub_h, an, bn);
    int time_cy     = (zone_top + zone_bot) / 2;
    if (s_phase == PHASE_DONE && actual_h != s_target_h) {
      s_target_h = actual_h; s_h = actual_h;
    }
    int draw_h = (s_phase == PHASE_BLINK) ? s_h : actual_h;
    int slide = s_info_slide;
    for (int i = 0; i < an; i++) {
      int y = above_start + i*INFO_LINE_STEP_WIDE;
      int half = prv_round_chord_half(y, y + INFO_GLYPH_H) - UNIT;
      if (half > 10) draw_info_line(ctx, &s_above_lines[i], y - slide, SCREEN_W/2 - half, half*2, GTextAlignmentCenter);
    }
    for (int i = 0; i < bn; i++) {
      int y = below_top + i*INFO_LINE_STEP_WIDE;
      int half = prv_round_chord_half(y, y + INFO_GLYPH_H) - UNIT;
      if (half > 10) draw_info_line(ctx, &s_below_lines[i], y + slide, SCREEN_W/2 - half, half*2, GTextAlignmentCenter);
    }
    draw_digits_full_round(ctx, h_tens, h_ones, m_tens, m_ones, draw_h, time_cy, SHADOW_DX, SHADOW_DY, ub_top, ub_bot, s_shadow_col, s_shadow_col, s_shadow_col);
    draw_digits_full_round(ctx, h_tens, h_ones, m_tens, m_ones, draw_h, time_cy, 0, 0, ub_top, ub_bot, s_fg_hr, s_fg_mn, s_fg_colon);
#else
    int an = build_lines(s_above_lines, 3, above_s, 3, tm_now, true);
    int bn = build_lines(s_below_lines, 3, below_s, 3, tm_now, true);

    // Compute digit height from actual rendered counts, not configured counts.
    // Slots that hide (no data available) collapse to nothing -- digits expand to fill.
    // s_target_h uses configured counts as a conservative init; we refine it here live.
    int reserved = HALF_UNIT + prv_info_block_h(an, INFO_LINE_STEP_WIDE) + HALF_UNIT
                 + HALF_UNIT + WIDE_BELOW_EXTRA
                 + prv_info_block_h(bn, INFO_LINE_STEP_WIDE) + HALF_UNIT;
    int actual_h = ub_h - reserved;
    if (actual_h < H_MIN) actual_h = H_MIN;
    // Sync s_h and s_target_h when at rest so blink animation targets the right height
    if (s_phase == PHASE_DONE && actual_h != s_target_h) {
      s_target_h = actual_h; s_h = actual_h;
    }
    int draw_h    = (s_phase == PHASE_BLINK) ? s_h : actual_h;

    int above_start = ub_top + HALF_UNIT;
    int above_end   = above_start + prv_info_block_h(an, INFO_LINE_STEP_WIDE);
    int below_end   = ub_bot - HALF_UNIT;
    int below_top   = below_end - prv_info_block_h(bn, INFO_LINE_STEP_WIDE);
    int time_cy = (above_end + HALF_UNIT + below_top - HALF_UNIT - WIDE_BELOW_EXTRA) / 2;
    int col_x = 1, col_w = SCREEN_W - 2;
    int slide = s_info_slide;
    for (int i = 0; i < an; i++)
      draw_info_line(ctx, &s_above_lines[i], above_start + i*INFO_LINE_STEP_WIDE - slide, col_x, col_w, GTextAlignmentCenter);
    for (int i = 0; i < bn; i++) {
      int gy = below_top + i * INFO_LINE_STEP_WIDE;
      draw_info_line(ctx, &s_below_lines[i], gy + slide, col_x, col_w, GTextAlignmentCenter);
    }
    draw_digits_full(ctx, h_tens, h_ones, m_tens, m_ones, draw_h, time_cy, SHADOW_DX, SHADOW_DY, ub_top, ub_bot, s_shadow_col, s_shadow_col, s_shadow_col);
    draw_digits_full(ctx, h_tens, h_ones, m_tens, m_ones, draw_h, time_cy, 0, 0, ub_top, ub_bot, s_fg_hr, s_fg_mn, s_fg_colon);

#endif

  } else {
    // Stacked layouts: digits on one side, info column on the other.
    //
    // s_h is meaningful only during PHASE_BLINK (the stacked minute animation).
    // At rest, s_h may equal the full-screen height (set by prv_update_targets for stacked
    // layouts), so we always use dh for at-rest drawing and s_h only while animating.
    //
    // Stacked animation options (both use fast-blink timing, no overshoot):
    // STACK_L -- Option A: each pair squishes symmetrically around its own fixed center.
    // STACK_R -- Option B: "fold" toward screen center --
    //   hours: top folds inward, bottom edge pinned -> center drifts DOWN by drift/2
    //   minutes: bottom folds inward, top edge pinned -> center drifts UP by drift/2
    //   drift = (h_start - h_current) / 2
    int dh = prv_compute_stacked_h(ub_h);
    int h_cy = ub_top + MARGIN_OUTER + dh / 2;
    int m_cy = ub_bot - bot_margin - dh / 2;
    int tens_x, ones_x, col_x, col_w; GTextAlignment info_align;
    prv_stacked_geom(s_layout, &tens_x, &ones_x, &col_x, &col_w, &info_align);
    prv_draw_stacked_info(ctx, tm_now, col_x, col_w, info_align, col_x, ub_top, ub_bot, ub_h);

    bool stk_animating = (s_phase == PHASE_BLINK);
    int draw_h   = stk_animating ? s_h : dh;
    int draw_h_cy = h_cy;
    int draw_m_cy = m_cy;
    if (stk_animating && s_stk_h_start > 0 && s_layout == LAYOUT_STACK_R) {
      int drift = (s_stk_h_start - s_h) / 2;
      draw_h_cy = h_cy + drift;   // hours: top folds inward, center drifts down
      draw_m_cy = m_cy - drift;   // minutes: bottom folds inward, center drifts up
    }
#if defined(PBL_ROUND)
    { int dv[4] = { h_tens, h_ones, m_tens, m_ones };
      int cy_mid = ub_top + ub_h / 2;
      int anim_h = stk_animating ? s_h : -1;
      (void)draw_h; (void)draw_h_cy; (void)draw_m_cy; (void)tens_x; (void)ones_x; (void)h_cy; (void)m_cy;
      prv_round_stacked_draw(ctx, s_layout, cy_mid, anim_h, dv, SHADOW_DX, SHADOW_DY, s_shadow_col, s_shadow_col);
      prv_round_stacked_draw(ctx, s_layout, cy_mid, anim_h, dv, 0, 0, s_fg_hr, s_fg_mn); }
#else
    draw_stacked_pass(ctx, h_tens, h_ones, m_tens, m_ones, draw_h, tens_x, ones_x, draw_h_cy, draw_m_cy, SHADOW_DX, SHADOW_DY, s_shadow_col, s_shadow_col);
    draw_stacked_pass(ctx, h_tens, h_ones, m_tens, m_ones, draw_h, tens_x, ones_x, draw_h_cy, draw_m_cy, 0, 0, s_fg_hr, s_fg_mn);
#endif
  }
}

// ============================================================
// ANIMATION
// ============================================================
static void timer_cb(void *data);
static void schedule(uint32_t ms) {
  if (s_timer) app_timer_cancel(s_timer);
  s_timer = app_timer_register(ms, timer_cb, NULL);
}
static bool prv_ease_expand_step(void) {
  if (s_overshot) { s_h = s_target_h; s_overshot = false; return true; }
  int step = (s_ease_idx < EASE_LEN) ? EASE[s_ease_idx] : EASE[EASE_LEN-1]; s_ease_idx++;
  s_h += step;
  int peak = s_target_h + ANIM_OVERSHOOT;
  if (s_h >= peak) { s_h = peak; s_overshot = true; schedule(ANIM_SNAP_MS); return false; }
  if (s_h >= s_target_h) { s_h = s_target_h; return true; }
  schedule(ANIM_STEP_MS); return false;
}
static bool prv_ease_expand_step_clean(void) {
  int step = (s_ease_idx < EASE_LEN) ? EASE[s_ease_idx] : EASE[EASE_LEN-1]; s_ease_idx++;
  s_h += step;
  if (s_h >= s_target_h) { s_h = s_target_h; return true; }
  schedule(ANIM_STEP_MS); return false;
}
static bool prv_ease_shrink_step(void) {
  int step = (s_ease_idx < EASE_LEN) ? EASE[EASE_LEN-1-s_ease_idx] : EASE[0]; s_ease_idx++;
  s_h -= step;
  if (s_h <= s_h_min) { s_h = s_h_min; return true; }
  return false;
}
static bool prv_info_slide_step_fn(void) {
  if (s_info_slide_dir == 0) return true;
  s_info_slide_step++;
  int t = s_info_slide_step;
  int f = INFO_SLIDE_FRAMES;
  if (s_info_slide_dir < 0) s_info_slide = INFO_SLIDE_DIST - (INFO_SLIDE_DIST * t) / f;
  else                       s_info_slide = (INFO_SLIDE_DIST * t) / f;
  if (t >= f) {
    s_info_slide = (s_info_slide_dir < 0) ? 0 : INFO_SLIDE_DIST;
    s_info_slide_dir = 0; s_info_slide_step = 0;
    return true;
  }
  return false;
}
static void prv_start_blink(void) {
  s_h = s_target_h; s_going_down = true; s_anim_rep = 0; s_overshot = false;
  s_phase = PHASE_BLINK; layer_mark_dirty(s_canvas_layer); schedule(ANIM_STEP_MS);
}
static void prv_info_slide_in(void) {
  s_info_slide = INFO_SLIDE_DIST; s_info_slide_dir = -1; s_info_slide_step = 0;
}
static void prv_info_slide_out(void) {
  s_info_slide = 0; s_info_slide_dir = +1; s_info_slide_step = 0;
}
static void prv_start_fast_blink(void) {
  s_going_down = true; s_anim_rep = 0; s_overshot = false; s_ease_idx = 0;
  s_blink_single = true; s_expand_no_overshoot = true;
  s_phase = PHASE_BLINK; schedule(ANIM_STEP_MS);
}

static void prv_start_split_v(int target_layout) {
  if (s_h > s_target_h) s_h = s_target_h;
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  int ub_top = ub.origin.y, ub_h = ub.size.h, ub_bot = ub_top + ub_h;
  int bot_margin = BOTTOM_MARGIN(ub_h);
#if defined(PBL_ROUND)
  s_sv_cy = ub_top + ub_h / 2;
#else
  s_sv_cy = ub_top + MARGIN_OUTER + s_target_h / 2;
#endif
  int dh_final   = prv_compute_stacked_h(ub_h);
  int h_cy_final = ub_top + MARGIN_OUTER + dh_final / 2;
  int m_cy_final = ub_bot - bot_margin - dh_final / 2;
  s_sv_hr_cy_s = s_sv_cy;  s_sv_hr_cy_e = h_cy_final;
  s_sv_mn_cy_s = s_sv_cy;  s_sv_mn_cy_e = m_cy_final;
  s_sv_h_s     = s_h;      s_sv_h_e     = dh_final;
  int colon_start_top = s_sv_cy - s_h/4 + 2;
  int colon_start_bot = s_sv_cy + s_h/4 - 2;
  s_sv_top_dot_s = colon_start_top;  s_sv_top_dot_e = ub_top - COLON_OFFSCREEN;
  s_sv_bot_dot_s = colon_start_bot;  s_sv_bot_dot_e = ub_bot + COLON_OFFSCREEN;
  s_split_target_layout = target_layout;
  s_anim_step = 0; s_phase = PHASE_SPLIT_V; schedule(SPLIT_MS);
}
static void prv_start_split_h(void) {
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  int ub_h = ub.size.h, ub_top = ub.origin.y, ub_bot = ub_top + ub_h;
  int bot_margin = BOTTOM_MARGIN(ub_h);
  int tl = s_split_target_layout;
  int stk_tens_x, stk_ones_x, col_x_final, col_w; GTextAlignment align;
  prv_stacked_geom(tl, &stk_tens_x, &stk_ones_x, &col_x_final, &col_w, &align);
  int dh_final   = prv_compute_stacked_h(ub_h);
  int h_cy_final = ub_top + MARGIN_OUTER + dh_final / 2;
  int m_cy_final = ub_bot - bot_margin - dh_final / 2;
  s_sh_hr_cy = h_cy_final; s_sh_mn_cy = m_cy_final; s_sh_h = dh_final;
  s_sh_hr_tx_s = SLOT_H_TENS;  s_sh_hr_tx_e = stk_tens_x;
  s_sh_mn_tx_s = SLOT_M_TENS;  s_sh_mn_tx_e = stk_tens_x;
  int info_start = (tl == LAYOUT_STACK_L) ? SCREEN_W : (0 - col_w - UNIT);
  s_sh_col_s = info_start;  s_sh_col_e = col_x_final;
  s_anim_step = 0; s_phase = PHASE_SPLIT_H; schedule(SPLIT_MS);
}
static void prv_start_merge_h(void) {
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  int ub_h = ub.size.h, ub_top = ub.origin.y, ub_bot = ub_top + ub_h;
  int bot_margin = BOTTOM_MARGIN(ub_h);
  int tl = s_layout;
  int stk_tens_x, stk_ones_x, col_x_final, col_w; GTextAlignment align;
  prv_stacked_geom(tl, &stk_tens_x, &stk_ones_x, &col_x_final, &col_w, &align);
  int dh_final   = prv_compute_stacked_h(ub_h);
  int h_cy_final = ub_top + MARGIN_OUTER + dh_final / 2;
  int m_cy_final = ub_bot - bot_margin - dh_final / 2;
  s_sh_hr_cy = h_cy_final; s_sh_mn_cy = m_cy_final; s_sh_h = dh_final;
  s_sh_hr_tx_s = stk_tens_x;  s_sh_hr_tx_e = SLOT_H_TENS;
  s_sh_mn_tx_s = stk_tens_x;  s_sh_mn_tx_e = SLOT_M_TENS;
  int info_end = (tl == LAYOUT_STACK_L) ? SCREEN_W : (0 - col_w - UNIT);
  s_sh_col_s = col_x_final;  s_sh_col_e = info_end;
  s_split_target_layout = tl;
  s_anim_step = 0; s_phase = PHASE_MERGE_H; schedule(SPLIT_MS);
}
static void prv_start_merge_v(void) {
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  int ub_h = ub.size.h, ub_top = ub.origin.y, ub_bot = ub_top + ub_h;
  int bot_margin = BOTTOM_MARGIN(ub_h);
#if defined(PBL_ROUND)
  s_sv_cy = ub_top + ub_h / 2;
#else
  s_sv_cy = ub_top + MARGIN_OUTER + s_target_h / 2;
#endif
  int dh_final   = prv_compute_stacked_h(ub_h);
  int h_cy_final = ub_top + MARGIN_OUTER + dh_final / 2;
  int m_cy_final = ub_bot - bot_margin - dh_final / 2;
  s_sv_hr_cy_s = h_cy_final;  s_sv_hr_cy_e = s_sv_cy;
  s_sv_mn_cy_s = m_cy_final;  s_sv_mn_cy_e = s_sv_cy;
  s_sv_h_s     = dh_final;    s_sv_h_e     = s_target_h;
  int colon_end_top = s_sv_cy - s_target_h/4 + 2;
  int colon_end_bot = s_sv_cy + s_target_h/4 - 2;
  s_sv_top_dot_s = ub_top - COLON_OFFSCREEN;  s_sv_top_dot_e = colon_end_top;
  s_sv_bot_dot_s = ub_bot + COLON_OFFSCREEN;  s_sv_bot_dot_e = colon_end_bot;
  s_anim_step = 0; s_phase = PHASE_MERGE_V; schedule(SPLIT_MS);
}

static void prv_return_to_full(void);
static void shake1_timer_cb(void *data) {
  s_shake1_timer = NULL;
  if (s_layout != LAYOUT_FULL && s_phase == PHASE_DONE) prv_return_to_full();
}
static void prv_schedule_shake1(void) {
  if (s_shake1_timer) app_timer_cancel(s_shake1_timer);
  s_shake1_timer = app_timer_register(SHAKE1_TIMEOUT_MS, shake1_timer_cb, NULL);
}
static void prv_cancel_shake1(void) {
  if (s_shake1_timer) { app_timer_cancel(s_shake1_timer); s_shake1_timer = NULL; }
}
static void prv_return_to_full(void) {
  prv_cancel_shake1();
  if (s_layout == LAYOUT_STACK_L || s_layout == LAYOUT_STACK_R) {
    prv_start_merge_h();
  } else {
    prv_info_slide_out();
    s_layout = LAYOUT_FULL;
    prv_update_targets();
    s_h_min = H_MIN;
    prv_start_fast_blink();
  }
  layer_mark_dirty(s_canvas_layer);
}

static void timer_cb(void *data) {
  s_timer = NULL;
  if (s_info_slide_dir != 0) { prv_info_slide_step_fn(); layer_mark_dirty(s_canvas_layer); }
  switch (s_phase) {
    case PHASE_ANTICIPATE:
      s_h += ANTICIPATION_PX; layer_mark_dirty(s_canvas_layer);
      s_phase = PHASE_SQUISH; s_ease_idx = 0; s_h_min = H_MIN; schedule(ANIM_STEP_MS); break;
    case PHASE_COUNTDOWN:
      switch (s_cd_sub) {
        case CD_SHRINK:
          s_h -= BLINK_STEP; layer_mark_dirty(s_canvas_layer);
          if (s_h<=H_MIN) { s_h=H_MIN; s_cd_sub=CD_HOLD_MIN; schedule(ANIM_SNAP_MS); }
          else { schedule(ANIM_STEP_MS); } break;
        case CD_HOLD_MIN:
          if (s_countdown_digit==0) { prv_start_blink(); break; }
          s_countdown_digit--; layer_mark_dirty(s_canvas_layer);
          s_cd_sub=CD_EXPAND; s_ease_idx=0; s_overshot=false; schedule(ANIM_STEP_MS); break;
        case CD_EXPAND: { bool d=prv_ease_expand_step(); layer_mark_dirty(s_canvas_layer);
          if(d){s_cd_sub=CD_HOLD_MAX;schedule(COUNTDOWN_HOLD_MS);} break; }
        case CD_HOLD_MAX: s_cd_sub=CD_SHRINK; schedule(ANIM_STEP_MS); break;
      } break;
    case PHASE_BLINK:
      if (s_going_down) {
        s_h -= BLINK_STEP; layer_mark_dirty(s_canvas_layer);
        if (s_h<=s_h_min) { s_h=s_h_min; s_going_down=false; s_overshot=false; s_ease_idx=0;
          // Digit swap at minimum -- applies to LAYOUT_INFO and both stacked layouts.
          if(s_digit_pending){s_hour=s_pending_hour;s_minute=s_pending_minute;s_digit_pending=false;} }
        schedule(ANIM_STEP_MS);
      } else {
        bool d = s_expand_no_overshoot ? prv_ease_expand_step_clean() : prv_ease_expand_step();
        layer_mark_dirty(s_canvas_layer);
        if(d) {
          int max_reps = s_blink_single ? 1 : BLINK_REPS;
          if (++s_anim_rep < max_reps) {
            s_going_down=true; s_overshot=false; s_ease_idx=0; schedule(ANIM_STEP_MS);
          } else {
            s_blink_single=false; s_expand_no_overshoot=false;
            s_phase=PHASE_DONE; layer_mark_dirty(s_canvas_layer);
          }
        }
      } break;
    case PHASE_SQUISH: {
      bool d=prv_ease_shrink_step(); layer_mark_dirty(s_canvas_layer);
      if(d) {
        if(s_digit_pending){s_hour=s_pending_hour;s_minute=s_pending_minute;s_digit_pending=false;}
        s_phase=PHASE_EXPAND; s_ease_idx=0; s_overshot=false; schedule(ANIM_STEP_MS);
      } else { schedule(ANIM_STEP_MS); } break; }
    case PHASE_EXPAND: {
      bool d = s_expand_no_overshoot ? prv_ease_expand_step_clean() : prv_ease_expand_step();
      layer_mark_dirty(s_canvas_layer);
      if(d){ s_expand_no_overshoot=false; s_phase=PHASE_DONE; layer_mark_dirty(s_canvas_layer); }
      break; }
    case PHASE_SHAKE_CYCLE: {
      static const int OFF[SHAKE_LEN]={0,-UNIT,-(UNIT*2),-(UNIT*3),-(UNIT*2),-UNIT,0};
      if(++s_anim_step<SHAKE_LEN) {
        int h=s_target_h+OFF[s_anim_step]; s_h=h<H_ABSOLUTE_MIN?H_ABSOLUTE_MIN:h;
        layer_mark_dirty(s_canvas_layer); schedule(ANIM_STEP_MS);
      } else { s_h=s_target_h; s_phase=PHASE_DONE; layer_mark_dirty(s_canvas_layer); } break; }
    case PHASE_SPLIT_V:
      if (++s_anim_step < SPLIT_V_FRAMES) { layer_mark_dirty(s_canvas_layer); schedule(SPLIT_MS); }
      else { layer_mark_dirty(s_canvas_layer); prv_start_split_h(); } break;
    case PHASE_SPLIT_H:
      if (++s_anim_step < SPLIT_H_FRAMES) { layer_mark_dirty(s_canvas_layer); schedule(SPLIT_MS); }
      else { s_layout = s_split_target_layout; prv_update_targets(); s_phase = PHASE_DONE; layer_mark_dirty(s_canvas_layer); } break;
    case PHASE_MERGE_H:
      if (++s_anim_step < SPLIT_H_FRAMES) { layer_mark_dirty(s_canvas_layer); schedule(SPLIT_MS); }
      else { s_layout = LAYOUT_FULL; prv_update_targets(); prv_start_merge_v(); } break;
    case PHASE_MERGE_V:
      if (++s_anim_step < SPLIT_V_FRAMES) { layer_mark_dirty(s_canvas_layer); schedule(SPLIT_MS); }
      else { s_h = s_target_h; s_phase = PHASE_DONE; layer_mark_dirty(s_canvas_layer); } break;
    case PHASE_DONE:
      if (s_info_slide_dir != 0) schedule(SPLIT_MS);
      break;
  }
}

// ============================================================
// EVENT HANDLERS
// ============================================================
static void unobstructed_change(AnimationProgress progress, void *ctx) {
  prv_update_targets();
  if (s_phase == PHASE_DONE) {
    if (s_layout == LAYOUT_STACK_L || s_layout == LAYOUT_STACK_R) {
      // For stacked, s_target_h is full-screen height; reset s_h to per-pair dh instead
      GRect ub2 = layer_get_unobstructed_bounds(window_get_root_layer(s_window));
      s_h = prv_compute_stacked_h(ub2.size.h);
      s_target_h = s_h;
    } else {
      s_h = s_target_h;
    }
    layer_mark_dirty(s_canvas_layer);
  }
}
static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_phase != PHASE_DONE) return;
  if (s_cfg_info_mode == INFO_MODE_OFF || s_cfg_info_mode == INFO_MODE_ALWAYS) return;
  if (s_layout == LAYOUT_FULL) {
    int target = prv_info_layout_for_shake();
    if (target == LAYOUT_STACK_L || target == LAYOUT_STACK_R) {
      prv_start_split_v(target);
    } else {
      s_layout = target;
      prv_update_targets();
      s_h_min = H_MIN;
      prv_info_slide_in();
      prv_start_fast_blink();
    }
    if (s_cfg_info_mode == INFO_MODE_SHAKE1) prv_schedule_shake1();
  } else {
    prv_return_to_full();
  }
  layer_mark_dirty(s_canvas_layer);
}
static void prv_noop_click(ClickRecognizerRef ref, void *ctx) { (void)ref; (void)ctx; }
static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP,     prv_noop_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_noop_click);
  window_single_click_subscribe(BUTTON_ID_DOWN,   prv_noop_click);
}
static void tick_handler(struct tm *t, TimeUnits units) {
  // All layouts now animate on minute change
  bool animated = (s_layout == LAYOUT_FULL || s_layout == LAYOUT_INFO
                   || s_layout == LAYOUT_STACK_L || s_layout == LAYOUT_STACK_R);
  if (s_phase == PHASE_DONE && animated) {
    s_pending_hour=t->tm_hour; s_pending_minute=t->tm_min; s_digit_pending=true;
    s_h_min=H_MIN;
    if (s_layout == LAYOUT_INFO) {
      prv_start_fast_blink();
    } else if (s_layout == LAYOUT_STACK_L || s_layout == LAYOUT_STACK_R) {
      // Stacked blink: set s_h and s_target_h to per-pair height dh so the blink
      // animates dh -> H_ABSOLUTE_MIN -> dh (not full-screen height -> H_ABSOLUTE_MIN).
      // H_ABSOLUTE_MIN gives enough travel for a visible animation on small stacked digits.
      Layer *_root = window_get_root_layer(s_window);
      GRect _ub = layer_get_unobstructed_bounds(_root);
      int _dh = prv_compute_stacked_h(_ub.size.h);
      s_stk_h_start = _dh;
      s_h = _dh;
      s_target_h = _dh;
      s_h_min = H_ABSOLUTE_MIN;  // deeper floor -> more visible animation on small digits
      prv_start_fast_blink();
    } else {
      s_phase = PHASE_ANTICIPATE; schedule(ANTICIPATION_MS);
    }
  } else if (s_phase == PHASE_SQUISH) {
    s_pending_hour=t->tm_hour; s_pending_minute=t->tm_min; s_digit_pending=true;
  } else { s_hour=t->tm_hour; s_minute=t->tm_min; layer_mark_dirty(s_canvas_layer); }
#if defined(PBL_HEALTH)
  prv_update_health();
#endif
  // Age out weather/UV/AQI after 3 hours; sunrise/sunset stay valid all day
  { time_t _now = time(NULL);
    if (s_weather_ts > 0 && (_now - s_weather_ts) > WEATHER_MAX_AGE) {
      s_weather_temp_f = -999;
      s_weather_temp_c = -999;
      s_uv_index = -1;
    }
    if (s_aqi_ts > 0 && (_now - s_aqi_ts) > WEATHER_MAX_AGE) {
      s_aqi = -1;
    }
  }
}
static void battery_handler(BatteryChargeState state) {
  s_battery_pct=state.charge_percent; s_charging=state.is_charging; layer_mark_dirty(s_canvas_layer);
}
static void bt_handler(bool connected) {
  s_bt_connected=connected; layer_mark_dirty(s_canvas_layer);
}
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;
  t = dict_find(iter, MESSAGE_KEY_WeatherTempF); if(t) { s_weather_temp_f=(int)t->value->int32; s_weather_ts = time(NULL); }
  t = dict_find(iter, MESSAGE_KEY_WeatherTempC); if(t) s_weather_temp_c=(int)t->value->int32;
  t = dict_find(iter, MESSAGE_KEY_WeatherCode);  if(t) s_weather_code=(int)t->value->int32;
  t = dict_find(iter, MESSAGE_KEY_SunriseTime);
  if(t){time_t ts=(time_t)(uint32_t)t->value->uint32;struct tm*lt=localtime(&ts);if(lt)s_sunrise_min=lt->tm_hour*60+lt->tm_min;}
  t = dict_find(iter, MESSAGE_KEY_SunsetTime);
  if(t){time_t ts=(time_t)(uint32_t)t->value->uint32;struct tm*lt=localtime(&ts);if(lt)s_sunset_min=lt->tm_hour*60+lt->tm_min;}
  t = dict_find(iter, MESSAGE_KEY_UvIndex);
  if(t) { s_uv_index = (int)t->value->int32; s_weather_ts = time(NULL); }
  t = dict_find(iter, MESSAGE_KEY_AqiIndex);
  if(t) { s_aqi = (int)t->value->int32; s_aqi_ts = time(NULL); }
  t = dict_find(iter, MESSAGE_KEY_SunriseTomorrow);
  if(t){time_t ts=(time_t)(uint32_t)t->value->uint32;struct tm*lt=localtime(&ts);if(lt)s_sunrise_tom_min=lt->tm_hour*60+lt->tm_min;}
  t = dict_find(iter, MESSAGE_KEY_CfgInfoMode);
  if(t) s_cfg_info_mode = (InfoMode)(t->value->int32 & 0x7);
  t = dict_find(iter, MESSAGE_KEY_CfgInfoLayout);
  if(t) s_cfg_info_layout = (InfoLayout)(t->value->int32 & 0x3);
  t = dict_find(iter, MESSAGE_KEY_CfgTempUnit);
  if(t) s_cfg_temp_f = (t->value->int32 == 0);
  t = dict_find(iter, MESSAGE_KEY_CfgDistUnit);
  if(t) s_cfg_dist_mi = (t->value->int32 == 0);
  t = dict_find(iter, MESSAGE_KEY_CfgInvert);
  if(t) s_cfg_invert = (t->value->int32 != 0);
  t = dict_find(iter, MESSAGE_KEY_CfgColorMode);
  if(t) s_cfg_color_mode = (int)t->value->int32;
  t = dict_find(iter, MESSAGE_KEY_CfgColorBg);
  if(t) s_cfg_col_bg = (uint8_t)(t->value->int32 & 0x3f);
  t = dict_find(iter, MESSAGE_KEY_CfgColorDigH);
  if(t) s_cfg_col_dig_h = (uint8_t)(t->value->int32 & 0x3f);
  t = dict_find(iter, MESSAGE_KEY_CfgColorDigM);
  if(t) s_cfg_col_dig_m = (uint8_t)(t->value->int32 & 0x3f);
  t = dict_find(iter, MESSAGE_KEY_CfgColorShadow);
  if(t) s_cfg_col_shadow = (uint8_t)(t->value->int32 & 0x3f);
  t = dict_find(iter, MESSAGE_KEY_CfgColorInfo);
  if(t) s_cfg_col_info = (uint8_t)(t->value->int32 & 0x3f);
  t = dict_find(iter, MESSAGE_KEY_CfgColorColon);
  if(t) s_cfg_col_colon = (uint8_t)(t->value->int32 & 0x3f);
  for (int i = 0; i < WIDE_SLOTS; i++) {
    t = dict_find(iter, MESSAGE_KEY_CfgWide1 + i);
    if(t) s_cfg_wide[i] = (int)t->value->int32;
  }
  for (int i = 0; i < STACK_SLOTS; i++) {
    t = dict_find(iter, MESSAGE_KEY_CfgStack1 + i);
    if(t) s_cfg_stack[i] = (int)t->value->int32;
  }
  prv_save_config(); prv_update_targets(); layer_mark_dirty(s_canvas_layer);
}
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas_layer, draw_layer);
  layer_add_child(root, s_canvas_layer);
  GRect ub = layer_get_unobstructed_bounds(root);
  s_target_h = prv_compute_target_h(ub.size.h, s_layout); s_h = s_target_h;
  { UnobstructedAreaHandlers ua = { .change = unobstructed_change };
    unobstructed_area_service_subscribe(ua, NULL); }
  accel_tap_service_subscribe(accel_tap_handler);
  s_phase = PHASE_DONE; s_overshot = false; s_ease_idx = 0;
  layer_mark_dirty(s_canvas_layer);
}
static void window_unload(Window *window) {
  unobstructed_area_service_unsubscribe();
  accel_tap_service_unsubscribe();
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  prv_cancel_shake1();
  layer_destroy(s_canvas_layer);
}
static void init(void) {
  s_fg = GColorWhite; s_fg_hr = GColorWhite; s_fg_mn = GColorWhite; s_fg_colon = GColorWhite;
  s_bg = GColorBlack; s_shadow_col = GColorBlack;
  prv_load_config();
  if (s_cfg_info_mode == INFO_MODE_ALWAYS) {
    switch (s_cfg_info_layout) {
      case INFO_LAYOUT_WIDE:    s_layout = LAYOUT_INFO; break;
      case INFO_LAYOUT_STACK_L: s_layout = LAYOUT_STACK_L; break;
      case INFO_LAYOUT_STACK_R: s_layout = LAYOUT_STACK_R; break;
    }
  }
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers){ .load=window_load, .unload=window_unload });
  window_stack_push(s_window, true);
  time_t now = time(NULL); struct tm *t = localtime(&now);
  s_hour = t->tm_hour; s_minute = t->tm_min;
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  battery_handler(battery_state_service_peek());
  bluetooth_connection_service_subscribe(bt_handler);
  s_bt_connected = bluetooth_connection_service_peek();
#if defined(PBL_HEALTH)
  prv_update_health();
#endif
  app_message_register_inbox_received(inbox_received);
  app_message_open(512, 64);
}
static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  prv_cancel_shake1();
  window_destroy(s_window);
}
int main(void) { init(); app_event_loop(); deinit(); }
