#include <pebble.h>

// ============================================================
// TallBoy — main.c  v3.49
// Design: Sterling Ely. Code: Sterling Ely + Claude. 2026.
//
// v3.49 changes:
//   STACKED INFO SPACING: removed double-margin bug. Info lines
//     now span from UNIT*2 below top of screen to UNIT*2 above
//     bottom, using a fixed step computed for STACK_INFO_LINES=8.
//     Fewer lines use same step starting from the top.
//   STACKED INFO ICON ALIGN: right-aligned icon lines now draw
//     icon+text block flush to right edge of column. Icon draws
//     first (left of text), whole unit right-justified.
//   WIDE CONFIG / STACKED CONFIG: added separate slot lists for
//     wide mode vs stacked mode. Wide uses s_cfg_wide_slots[6]
//     (combo lines), stacked uses s_cfg_stack_slots[8] (singles).
//     Both persist independently.
// ============================================================

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
  SLOT_BLUETOOTH  = 16
} SlotType;

#define LAYOUT_FULL    0
#define LAYOUT_INFO    1
#define LAYOUT_STACK_R 2
#define LAYOUT_STACK_L 3
#define SHAKE_LEN      7

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
  #define ICON_W              14
  static const uint16_t s_radius_opts[] = { UNIT*2, UNIT*3, UNIT*4 };
  #define RADIUS_COUNT 3
  static int s_radius_idx = 0;
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
  #define ICON_W              11
#endif

#define ICON_LARGE       (ICON_W == 14)
#define ICON_TEXT_GAP    2
#define MARGIN_CANVAS   UNIT
#define MARGIN_DIGIT    UNIT
#define MARGIN_OUTER    (MARGIN_CANVAS + MARGIN_DIGIT)
#define HALF_SLOT_PAD   (UNIT / 2)
#define GLYPH_W         (UNIT * 4)
#define H_MIN           (UNIT * 11)
#define H_ABSOLUTE_MIN  (UNIT * 5)
#define INFO_LINE_BUF   48
#define INFO_LINES_MAX  8
#define NUM_SLOTS       6

// Stacked info line spacing:
// Fixed step so that STACK_INFO_LINES=8 lines fill from UNIT*2 top
// to UNIT*2 bottom. Fewer lines use same step from the top.
#define STACK_INFO_MARGIN  (UNIT * 2)
#define STACK_INFO_LINES   8

// Digit shadow
#define SHADOW_DX  HALF_UNIT
#define SHADOW_DY  HALF_UNIT

static const int EASE[10] = { 4, 6, 8, 10, 12, 12, 10, 8, 6, 4 };
#define EASE_LEN  10
#define ANIM_STEP_MS      16
#define ANIM_SNAP_MS      80
#define ANIM_OVERSHOOT    UNIT
#define ANTICIPATION_PX   HALF_UNIT
#define ANTICIPATION_MS   16
#define COUNTDOWN_HOLD_MS 120
#define BLINK_REPS        2
#define STEPS_AVG_MAX_MIN 120
#define DOT               " \xc2\xb7 "

#if defined(PBL_TOUCH)
static const uint32_t TOUCH_VIBE_MS[] = {50};
static const VibePattern TOUCH_VIBE = { .durations = TOUCH_VIBE_MS, .num_segments = 1 };
#endif

#define QUICK_LOOK_ACTIVE(ub_h)  ((ub_h) < (SCREEN_H - UNIT))
#define BOTTOM_MARGIN(ub_h)      (QUICK_LOOK_ACTIVE(ub_h) ? MARGIN_DIGIT : MARGIN_OUTER)

typedef enum { CD_SHRINK, CD_HOLD_MIN, CD_EXPAND, CD_HOLD_MAX } CdSubPhase;
typedef enum {
  PHASE_COUNTDOWN, PHASE_BLINK, PHASE_DONE,
  PHASE_ANTICIPATE, PHASE_SQUISH, PHASE_EXPAND, PHASE_SHAKE_CYCLE
} Phase;

typedef void (*IconFn)(GContext*,int,int,GColor,bool);
typedef struct {
  char text[INFO_LINE_BUF];
  bool has_icon;
  IconFn icon_fn;
  bool is_battery;
  bool is_weather;
  int  icon_extra;
} InfoLine;

// ============================================================
// STATE
// ============================================================
static Window    *s_window;
static Layer     *s_canvas_layer;
static int        s_hour = 0, s_minute = 0;
static int        s_h = 0, s_target_h = 0, s_h_min = 0, s_ease_idx = 0;
static GColor     s_fg, s_bg;
static Phase      s_phase = PHASE_DONE;
static int        s_anim_step = 0, s_anim_rep = 0;
static bool       s_going_down = true, s_overshot = false;
static int        s_countdown_digit = 9;
static CdSubPhase s_cd_sub = CD_HOLD_MAX;
static AppTimer  *s_timer = NULL;
static bool       s_digit_pending = false;
static int        s_pending_hour = 0, s_pending_minute = 0;
static int        s_battery_pct = 100;
static bool       s_charging = false, s_bt_connected = true;
#if defined(PBL_HEALTH)
static int        s_steps = 0, s_distance_m = 0, s_calories = 0, s_heart_rate = 0;
static int        s_steps_expected = -1;
#endif
static int        s_sunrise_min = -1, s_sunset_min = -1;
static int        s_weather_temp_f = -999, s_weather_temp_c = -999;
static int        s_weather_code = 0;
static int        s_layout = LAYOUT_FULL;

// InfoLine static globals — MUST NOT be stack-allocated (crashes)
static InfoLine s_above_lines[INFO_LINES_MAX];
static InfoLine s_below_lines[INFO_LINES_MAX];
static InfoLine s_col_lines[INFO_LINES_MAX];

#define TIME_MARKER 17

// ---- Config: wide mode uses the old order array (6 slots + time marker) ----
static int  s_cfg_order[NUM_SLOTS + 1] = { 1,2,3,17,4,5,6 };

// ---- Config: stacked mode has its own flat 8-slot list ----
// Defaults to a sensible set of individual data fields
#define STACK_SLOTS 8
static int s_cfg_stack[STACK_SLOTS] = { 1, 2, 6, 9, 11, 4, 15, 16 };

static bool s_cfg_temp_f  = true;
static bool s_cfg_dist_mi = true;
static bool s_cfg_24h     = false;

#define PERSIST_CFG_ORDER  1
#define PERSIST_CFG_FLAGS  2
#define PERSIST_CFG_STACK  3   // packed: 8 slots × 5 bits = 40 bits, use int64 or two int32s
// Use two int32s: low word has slots 0-5, high word has slots 6-7
#define PERSIST_CFG_STACK_HI 4

static void prv_save_config(void) {
  int32_t packed = 0;
  for (int i = 0; i < NUM_SLOTS + 1; i++) packed |= (s_cfg_order[i] & 0x1f) << (i * 5);
  persist_write_int(PERSIST_CFG_ORDER, packed);
  int32_t flags = (s_cfg_temp_f ? 1 : 0) | (s_cfg_dist_mi ? 2 : 0) | (s_cfg_24h ? 4 : 0);
  persist_write_int(PERSIST_CFG_FLAGS, flags);
  // Stack: 6 slots in low word, 2 in high word
  int32_t sp_lo = 0, sp_hi = 0;
  for (int i = 0; i < 6; i++) sp_lo |= (s_cfg_stack[i] & 0x1f) << (i * 5);
  for (int i = 0; i < 2; i++) sp_hi |= (s_cfg_stack[6+i] & 0x1f) << (i * 5);
  persist_write_int(PERSIST_CFG_STACK, sp_lo);
  persist_write_int(PERSIST_CFG_STACK_HI, sp_hi);
}
static void prv_load_config(void) {
  if (persist_exists(PERSIST_CFG_ORDER)) {
    int32_t packed = persist_read_int(PERSIST_CFG_ORDER);
    for (int i = 0; i < NUM_SLOTS + 1; i++) s_cfg_order[i] = (packed >> (i * 5)) & 0x1f;
  }
  if (persist_exists(PERSIST_CFG_FLAGS)) {
    int32_t flags = persist_read_int(PERSIST_CFG_FLAGS);
    s_cfg_temp_f  = (flags & 1) != 0;
    s_cfg_dist_mi = (flags & 2) != 0;
    s_cfg_24h     = (flags & 4) != 0;
  }
  if (persist_exists(PERSIST_CFG_STACK)) {
    int32_t sp_lo = persist_read_int(PERSIST_CFG_STACK);
    int32_t sp_hi = persist_exists(PERSIST_CFG_STACK_HI) ? persist_read_int(PERSIST_CFG_STACK_HI) : 0;
    for (int i = 0; i < 6; i++) s_cfg_stack[i] = (sp_lo >> (i * 5)) & 0x1f;
    for (int i = 0; i < 2; i++) s_cfg_stack[6+i] = (sp_hi >> (i * 5)) & 0x1f;
  }
}
static int prv_time_pos(void) {
  for (int i = 0; i < NUM_SLOTS + 1; i++)
    if (s_cfg_order[i] == TIME_MARKER) return i;
  return 3;
}
static void prv_split_slots(int *above_slots, int *n_above, int *below_slots, int *n_below) {
  int tp = prv_time_pos(); *n_above = 0; *n_below = 0;
  for (int i = 0; i < NUM_SLOTS + 1; i++) {
    if (s_cfg_order[i] == TIME_MARKER) continue;
    if (i < tp) above_slots[(*n_above)++] = s_cfg_order[i];
    else        below_slots[(*n_below)++] = s_cfg_order[i];
  }
}
static void prv_info_count(int *above, int *below) {
  int ab[NUM_SLOTS], bb[NUM_SLOTS], na = 0, nb = 0;
  prv_split_slots(ab, &na, bb, &nb);
  int a = 0, b = 0;
  for (int i = 0; i < na; i++) if (ab[i] != SLOT_EMPTY) a++;
  for (int i = 0; i < nb; i++) if (bb[i] != SLOT_EMPTY) b++;
  *above = a; *below = b;
}
static int prv_next_layout(int current) {
  int a, b; prv_info_count(&a, &b);
  bool has_info = (a > 0 || b > 0);
  switch (current) {
    case LAYOUT_FULL:    return has_info ? LAYOUT_INFO : LAYOUT_STACK_L;
    case LAYOUT_INFO:    return LAYOUT_STACK_L;
    case LAYOUT_STACK_L: return LAYOUT_STACK_R;
    case LAYOUT_STACK_R: return LAYOUT_FULL;
    default:             return LAYOUT_FULL;
  }
}

#if defined(PBL_COLOR)
static GColor prv_pace_color(int steps_today, int steps_expected) {
  if (steps_expected <= 0 || steps_today <= 0) return GColorBlack;
  int pct = (steps_today * 100) / steps_expected;
  if (pct <= 0)    return GColorBlack;
  if (pct <= 30)   return GColorRed;
  if (pct <= 60)   return GColorOrange;
  if (pct <= 90)   return GColorYellow;
  if (pct <= 200)  return GColorGreen;
  if (pct <= 400)  return GColorBlue;
  if (pct <= 700)  return GColorIndigo;
  if (pct <= 1000) return GColorVividViolet;
  return GColorBlack;
}
static bool prv_bg_needs_dark_fg(GColor bg) {
  return gcolor_equal(bg, GColorYellow) || gcolor_equal(bg, GColorGreen);
}
#endif

#if defined(PBL_HEALTH)
static HealthMinuteData s_minute_buf[STEPS_AVG_MAX_MIN];
static int prv_calc_steps_expected(void) {
  time_t now = time(NULL); struct tm *t = localtime(&now);
  int elapsed_min = t->tm_hour * 60 + t->tm_min;
  if (elapsed_min < 2) return -1;
  int window = elapsed_min < STEPS_AVG_MAX_MIN ? elapsed_min : STEPS_AVG_MAX_MIN;
  int total = 0, day_count = 0;
  for (int day = 1; day <= 7; day++) {
    time_t day_start = time_start_of_today() - (time_t)day * SECONDS_PER_DAY;
    uint32_t n = health_service_get_minute_history(s_minute_buf, (uint32_t)window, &day_start, NULL);
    if (n == 0) continue;
    int day_steps = 0;
    for (uint32_t i = 0; i < n; i++)
      if (!s_minute_buf[i].is_invalid) day_steps += s_minute_buf[i].steps;
    if (day_steps > 0) {
      if (elapsed_min > window) day_steps = (day_steps * elapsed_min) / window;
      total += day_steps; day_count++;
    }
  }
  return day_count > 0 ? total / day_count : -1;
}
static void prv_update_health(void) {
  HealthServiceAccessibilityMask mask;
  time_t start = time_start_of_today(), now = time(NULL);
  mask = health_service_metric_accessible(HealthMetricStepCount, start, now);
  s_steps = (mask & HealthServiceAccessibilityMaskAvailable)
    ? (int)health_service_sum_today(HealthMetricStepCount) : 0;
  mask = health_service_metric_accessible(HealthMetricWalkedDistanceMeters, start, now);
  s_distance_m = (mask & HealthServiceAccessibilityMaskAvailable)
    ? (int)health_service_sum_today(HealthMetricWalkedDistanceMeters) : 0;
  mask = health_service_metric_accessible(HealthMetricActiveKCalories, start, now);
  s_calories = (mask & HealthServiceAccessibilityMaskAvailable)
    ? (int)health_service_sum_today(HealthMetricActiveKCalories) : 0;
  mask = health_service_metric_accessible(HealthMetricHeartRateBPM, start, now);
  s_heart_rate = (mask & HealthServiceAccessibilityMaskAvailable)
    ? (int)health_service_peek_current_value(HealthMetricHeartRateBPM) : 0;
  s_steps_expected = prv_calc_steps_expected();
}
#endif

// ============================================================
// ICONS
// ============================================================
static void icon_footprint(GContext *ctx, int fx, int fy, GColor col, bool large) {
  graphics_context_set_fill_color(ctx, col);
  if (!large) {
    graphics_fill_rect(ctx, GRect(fx,   fy,   4, 5), 2, GCornersAll);
    graphics_fill_rect(ctx, GRect(fx+1, fy+4, 2, 4), 1, GCornersAll);
  } else {
    graphics_fill_rect(ctx, GRect(fx,   fy,   5, 7), 2, GCornersAll);
    graphics_fill_rect(ctx, GRect(fx+1, fy+6, 3, 4), 1, GCornersAll);
  }
}
static void icon_steps(GContext *ctx, int ox, int oy, GColor col, bool large) {
  if (!large) { icon_footprint(ctx, ox+5, oy-1, col, false); icon_footprint(ctx, ox, oy+2, col, false); }
  else        { icon_footprint(ctx, ox+7, oy,   col, true);  icon_footprint(ctx, ox, oy+3, col, true); }
}
static void icon_battery(GContext *ctx, int ox, int oy, GColor col, int pct, bool large) {
  graphics_context_set_fill_color(ctx, col);
  if (s_charging) {
    if (!large) {
      graphics_fill_rect(ctx, GRect(ox+5,oy+0,4,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+4,oy+1,4,1),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+3,oy+2,4,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+2,oy+3,4,1),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+1,oy+4,7,2),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+3,oy+6,4,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+2,oy+7,4,1),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+1,oy+8,4,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+0,oy+9,4,1),0,GCornerNone);
    } else {
      graphics_fill_rect(ctx, GRect(ox+7,oy+0,5,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+6,oy+1,5,1),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+5,oy+2,5,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+4,oy+3,5,1),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+3,oy+4,5,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+1,oy+5,11,2),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+5,oy+7,5,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+4,oy+8,5,1),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+3,oy+9,5,1),0,GCornerNone); graphics_fill_rect(ctx, GRect(ox+2,oy+10,5,1),0,GCornerNone);
      graphics_fill_rect(ctx, GRect(ox+1,oy+11,5,1),0,GCornerNone);
    }
  } else {
    graphics_context_set_stroke_color(ctx, col); graphics_context_set_stroke_width(ctx, 1);
    if (!large) {
      graphics_draw_rect(ctx, GRect(ox,oy+2,9,7));
      graphics_fill_rect(ctx, GRect(ox+9,oy+4,2,3),0,GCornerNone);
      int fw = (7*pct)/100; if (fw<1&&pct>0) fw=1;
      if (fw>0) graphics_fill_rect(ctx, GRect(ox+1,oy+3,fw,5),0,GCornerNone);
    } else {
      graphics_draw_rect(ctx, GRect(ox,oy+1,12,10));
      graphics_fill_rect(ctx, GRect(ox+12,oy+4,2,4),0,GCornerNone);
      int fw = (10*pct)/100; if (fw<1&&pct>0) fw=1;
      if (fw>0) graphics_fill_rect(ctx, GRect(ox+1,oy+2,fw,8),0,GCornerNone);
    }
  }
}
static void icon_bt(GContext *ctx, int ox, int oy, GColor col, bool large) {
  graphics_context_set_stroke_color(ctx, col); graphics_context_set_stroke_width(ctx, 1);
  if (!large) {
    graphics_draw_line(ctx, GPoint(ox+3,oy+0), GPoint(ox+3,oy+10));
    graphics_draw_pixel(ctx, GPoint(ox+2,oy+4)); graphics_draw_pixel(ctx, GPoint(ox+1,oy+3));
    graphics_draw_pixel(ctx, GPoint(ox+2,oy+7)); graphics_draw_pixel(ctx, GPoint(ox+1,oy+8));
    graphics_draw_pixel(ctx, GPoint(ox+4,oy+1)); graphics_draw_pixel(ctx, GPoint(ox+5,oy+2));
    graphics_draw_pixel(ctx, GPoint(ox+6,oy+3)); graphics_draw_pixel(ctx, GPoint(ox+5,oy+4));
    graphics_draw_pixel(ctx, GPoint(ox+4,oy+5)); graphics_draw_pixel(ctx, GPoint(ox+4,oy+6));
    graphics_draw_pixel(ctx, GPoint(ox+5,oy+7)); graphics_draw_pixel(ctx, GPoint(ox+6,oy+8));
    graphics_draw_pixel(ctx, GPoint(ox+5,oy+9)); graphics_draw_pixel(ctx, GPoint(ox+4,oy+10));
    graphics_draw_line(ctx, GPoint(ox+9,oy+1),  GPoint(ox+9,oy+6));
    graphics_draw_line(ctx, GPoint(ox+10,oy+1), GPoint(ox+10,oy+6));
    graphics_draw_pixel(ctx, GPoint(ox+9,oy+8)); graphics_draw_pixel(ctx, GPoint(ox+10,oy+8));
    graphics_draw_pixel(ctx, GPoint(ox+9,oy+9)); graphics_draw_pixel(ctx, GPoint(ox+10,oy+9));
  } else {
    graphics_draw_line(ctx, GPoint(ox+4,oy+0),  GPoint(ox+4,oy+13));
    graphics_draw_pixel(ctx, GPoint(ox+3,oy+4)); graphics_draw_pixel(ctx, GPoint(ox+2,oy+3)); graphics_draw_pixel(ctx, GPoint(ox+1,oy+2));
    graphics_draw_pixel(ctx, GPoint(ox+3,oy+9)); graphics_draw_pixel(ctx, GPoint(ox+2,oy+10)); graphics_draw_pixel(ctx, GPoint(ox+1,oy+11));
    graphics_draw_pixel(ctx, GPoint(ox+5,oy+1)); graphics_draw_pixel(ctx, GPoint(ox+6,oy+2)); graphics_draw_pixel(ctx, GPoint(ox+7,oy+3));
    graphics_draw_pixel(ctx, GPoint(ox+8,oy+4)); graphics_draw_pixel(ctx, GPoint(ox+7,oy+5)); graphics_draw_pixel(ctx, GPoint(ox+6,oy+6)); graphics_draw_pixel(ctx, GPoint(ox+5,oy+7));
    graphics_draw_pixel(ctx, GPoint(ox+5,oy+8)); graphics_draw_pixel(ctx, GPoint(ox+6,oy+9)); graphics_draw_pixel(ctx, GPoint(ox+7,oy+10));
    graphics_draw_pixel(ctx, GPoint(ox+8,oy+11)); graphics_draw_pixel(ctx, GPoint(ox+7,oy+12)); graphics_draw_pixel(ctx, GPoint(ox+6,oy+13)); graphics_draw_pixel(ctx, GPoint(ox+5,oy+14));
    graphics_draw_line(ctx, GPoint(ox+11,oy+1),  GPoint(ox+11,oy+9));
    graphics_draw_line(ctx, GPoint(ox+12,oy+1),  GPoint(ox+12,oy+9));
    graphics_draw_line(ctx, GPoint(ox+11,oy+11), GPoint(ox+12,oy+11));
  }
}
static void icon_heart(GContext *ctx, int ox, int oy, GColor col, bool large) {
  graphics_context_set_fill_color(ctx, col);
  if (!large) {
    int y=oy+1;
    graphics_fill_rect(ctx,GRect(ox+1,y+0,3,2),1,GCornersTop); graphics_fill_rect(ctx,GRect(ox+7,y+0,3,2),1,GCornersTop);
    graphics_fill_rect(ctx,GRect(ox+0,y+1,4,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+7,y+1,3,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+0,y+2,10,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+1,y+3,8,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+2,y+4,6,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+3,y+5,4,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+4,y+6,2,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+5,y+7,1,1),0,GCornerNone);
  } else {
    int y=oy+1;
    graphics_fill_rect(ctx,GRect(ox+1,y+0,4,2),1,GCornersTop); graphics_fill_rect(ctx,GRect(ox+9,y+0,4,2),1,GCornersTop);
    graphics_fill_rect(ctx,GRect(ox+0,y+1,5,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+9,y+1,4,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+0,y+2,6,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+8,y+2,5,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+0,y+3,13,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+1,y+4,11,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+2,y+5,9,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+3,y+6,7,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+4,y+7,5,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+5,y+8,3,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+6,y+9,1,1),0,GCornerNone);
  }
}
static void icon_calories(GContext *ctx, int ox, int oy, GColor col, bool large) {
  graphics_context_set_fill_color(ctx, col);
  if (!large) {
    graphics_fill_rect(ctx,GRect(ox+2,oy+7,7,3),2,GCornersBottom);
    graphics_fill_rect(ctx,GRect(ox+3,oy+4,5,4),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+4,oy+1,3,4),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+5,oy+0,1,2),0,GCornerNone);
  } else {
    graphics_fill_rect(ctx,GRect(ox+2,oy+9,10,5),2,GCornersBottom);
    graphics_fill_rect(ctx,GRect(ox+3,oy+5,8,5),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+5,oy+2,4,4),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+6,oy+0,2,3),0,GCornerNone);
  }
}
static void icon_sun(GContext *ctx, int ox, int oy, GColor col, bool large) {
  graphics_context_set_stroke_color(ctx, col); graphics_context_set_stroke_width(ctx, 1);
  int sz=large?14:11, icx=ox+sz/2, icy=oy+sz/2;
  graphics_draw_circle(ctx, GPoint(icx,icy), 3);
  graphics_draw_pixel(ctx,GPoint(icx,oy));      graphics_draw_pixel(ctx,GPoint(icx,oy+1));
  graphics_draw_pixel(ctx,GPoint(icx,oy+sz-1)); graphics_draw_pixel(ctx,GPoint(icx,oy+sz-2));
  graphics_draw_pixel(ctx,GPoint(ox,icy));       graphics_draw_pixel(ctx,GPoint(ox+1,icy));
  graphics_draw_pixel(ctx,GPoint(ox+sz-1,icy));  graphics_draw_pixel(ctx,GPoint(ox+sz-2,icy));
}
static void icon_cloud(GContext *ctx, int ox, int oy, GColor col, bool large) {
  graphics_context_set_fill_color(ctx, col);
  if (!large) {
    graphics_fill_rect(ctx,GRect(ox+2,oy+2,4,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+3,oy+1,3,2),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+6,oy+2,3,2),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+1,oy+3,9,4),0,GCornerNone);
  } else {
    graphics_fill_rect(ctx,GRect(ox+3,oy+2,5,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+4,oy+0,4,3),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+8,oy+1,4,3),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+1,oy+3,13,6),0,GCornerNone);
  }
}
static void icon_partly_cloudy(GContext *ctx, int ox, int oy, GColor col, bool large) {
  graphics_context_set_stroke_color(ctx, col); graphics_context_set_stroke_width(ctx, 1);
  if (!large) {
    graphics_draw_circle(ctx, GPoint(ox+7,oy+3), 2);
    graphics_draw_pixel(ctx,GPoint(ox+7,oy)); graphics_draw_pixel(ctx,GPoint(ox+10,oy+3)); graphics_draw_pixel(ctx,GPoint(ox+7,oy+6));
    graphics_context_set_fill_color(ctx, col);
    graphics_fill_rect(ctx,GRect(ox+2,oy+4,3,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+5,oy+4,2,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+1,oy+5,7,4),0,GCornerNone);
  } else {
    graphics_draw_circle(ctx, GPoint(ox+10,oy+3), 3);
    graphics_draw_pixel(ctx,GPoint(ox+10,oy)); graphics_draw_pixel(ctx,GPoint(ox+14,oy+3)); graphics_draw_pixel(ctx,GPoint(ox+10,oy+7));
    graphics_context_set_fill_color(ctx, col);
    graphics_fill_rect(ctx,GRect(ox+2,oy+5,4,1),0,GCornerNone); graphics_fill_rect(ctx,GRect(ox+6,oy+5,3,1),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(ox+1,oy+6,10,6),0,GCornerNone);
  }
}
static void icon_rain(GContext *ctx, int ox, int oy, GColor col, bool large) {
  icon_cloud(ctx,ox,oy,col,large);
  graphics_context_set_stroke_color(ctx,col); graphics_context_set_stroke_width(ctx,1);
  if (!large) {
    graphics_draw_pixel(ctx,GPoint(ox+2,oy+8)); graphics_draw_pixel(ctx,GPoint(ox+2,oy+10));
    graphics_draw_pixel(ctx,GPoint(ox+5,oy+9)); graphics_draw_pixel(ctx,GPoint(ox+5,oy+11));
    graphics_draw_pixel(ctx,GPoint(ox+8,oy+8)); graphics_draw_pixel(ctx,GPoint(ox+8,oy+10));
  } else {
    graphics_draw_pixel(ctx,GPoint(ox+2,oy+9));  graphics_draw_pixel(ctx,GPoint(ox+2,oy+11));
    graphics_draw_pixel(ctx,GPoint(ox+6,oy+10)); graphics_draw_pixel(ctx,GPoint(ox+6,oy+12));
    graphics_draw_pixel(ctx,GPoint(ox+10,oy+9)); graphics_draw_pixel(ctx,GPoint(ox+10,oy+11));
  }
}
static void icon_snow(GContext *ctx, int ox, int oy, GColor col, bool large) {
  graphics_context_set_stroke_color(ctx,col); graphics_context_set_stroke_width(ctx,1);
  int sz=large?14:11, icx=ox+sz/2, icy=oy+sz/2;
  graphics_draw_line(ctx,GPoint(ox+1,icy),GPoint(ox+sz-2,icy));
  graphics_draw_line(ctx,GPoint(icx,oy+1),GPoint(icx,oy+sz-2));
  graphics_draw_line(ctx,GPoint(ox+2,oy+2),GPoint(ox+sz-3,oy+sz-3));
  graphics_draw_line(ctx,GPoint(ox+sz-3,oy+2),GPoint(ox+2,oy+sz-3));
}
static void icon_storm(GContext *ctx, int ox, int oy, GColor col, bool large) {
  icon_cloud(ctx,ox,oy,col,large);
  graphics_context_set_stroke_color(ctx,col); graphics_context_set_stroke_width(ctx,1);
  if (!large) {
    graphics_draw_pixel(ctx,GPoint(ox+5,oy+7)); graphics_draw_pixel(ctx,GPoint(ox+5,oy+8));
    graphics_draw_pixel(ctx,GPoint(ox+4,oy+8)); graphics_draw_pixel(ctx,GPoint(ox+4,oy+9));
    graphics_draw_pixel(ctx,GPoint(ox+6,oy+9)); graphics_draw_pixel(ctx,GPoint(ox+6,oy+10));
    graphics_draw_pixel(ctx,GPoint(ox+5,oy+10));graphics_draw_pixel(ctx,GPoint(ox+5,oy+11));
  } else {
    graphics_draw_pixel(ctx,GPoint(ox+7,oy+9));  graphics_draw_pixel(ctx,GPoint(ox+7,oy+10));
    graphics_draw_pixel(ctx,GPoint(ox+6,oy+10)); graphics_draw_pixel(ctx,GPoint(ox+6,oy+11));
    graphics_draw_pixel(ctx,GPoint(ox+8,oy+11)); graphics_draw_pixel(ctx,GPoint(ox+8,oy+12));
    graphics_draw_pixel(ctx,GPoint(ox+7,oy+12)); graphics_draw_pixel(ctx,GPoint(ox+7,oy+13));
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
// dx, dy = offset for shadow pass (use SHADOW_DX/DY) or 0,0 for normal.
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
static void draw_colon_vec(GContext *ctx, int slot_x, int cy, int h, int dx, int dy) {
  int r = UNIT / 2, ddx = slot_x + SLOT_W / 2 + dx;
  graphics_fill_radial(ctx, GRect(ddx-r, cy-h/4-r+2+dy, r*2, r*2), GOvalScaleModeFitCircle, (uint16_t)r, 0, DEG_TO_TRIGANGLE(360));
  graphics_fill_radial(ctx, GRect(ddx-r, cy+h/4-r-2+dy, r*2, r*2), GOvalScaleModeFitCircle, (uint16_t)r, 0, DEG_TO_TRIGANGLE(360));
}
static void draw_digits_pass(GContext *ctx, int h_tens, int h_ones, int m_tens, int m_ones,
                              int h, int cy, int dx, int dy) {
  draw_digit_vec(ctx, h_tens, SLOT_H_TENS, cy, h, dx, dy);
  draw_digit_vec(ctx, h_ones, SLOT_H_ONES, cy, h, dx, dy);
  draw_colon_vec(ctx, COLON_SLOT_X, cy, h, dx, dy);
  draw_digit_vec(ctx, m_tens, SLOT_M_TENS, cy, h, dx, dy);
  draw_digit_vec(ctx, m_ones, SLOT_M_ONES, cy, h, dx, dy);
}
static void draw_stacked_pass(GContext *ctx, int h_tens, int h_ones, int m_tens, int m_ones,
                               int dh, int tens_x, int ones_x, int h_cy, int m_cy, int dx, int dy) {
  draw_digit_vec(ctx, h_tens, tens_x, h_cy, dh, dx, dy);
  draw_digit_vec(ctx, h_ones, ones_x, h_cy, dh, dx, dy);
  draw_digit_vec(ctx, m_tens, tens_x, m_cy, dh, dx, dy);
  draw_digit_vec(ctx, m_ones, ones_x, m_cy, dh, dx, dy);
}

// ============================================================
// INFO LINE RENDERING
// col_x  = screen-space left edge of info column
// col_w  = column width in pixels
// align  = how text is positioned within the column
//
// For icon lines:
//   Layout is always [icon][gap][text] as a unit.
//   Left-align:   unit starts at col_x
//   Right-align:  unit ends at col_x + col_w (icon at left of unit, text at right)
//   Center:       unit centered in column
// ============================================================
static void draw_info_line(GContext *ctx, InfoLine *line, int y,
                            int col_x, int col_w, GTextAlignment align) {
  GFont font = fonts_get_system_font(INFO_FONT_KEY);
  int iy = y - INFO_TOP_PAD + (INFO_LINE_H - ICON_W) / 2;

  if (line->has_icon) {
    bool large = ICON_LARGE;
    GSize sz = graphics_text_layout_get_content_size(
      line->text, font, GRect(0,0,200,20), GTextOverflowModeFill, GTextAlignmentLeft);
    // Total width of [icon+gap+text] block, clamped to column
    int block_w = ICON_W + ICON_TEXT_GAP + sz.w;
    if (block_w > col_w) block_w = col_w;
    // Where the icon starts within the column
    int icon_off;
    if      (align == GTextAlignmentLeft)  { icon_off = 0; }
    else if (align == GTextAlignmentRight) { icon_off = col_w - block_w; }
    else                                   { icon_off = (col_w - block_w) / 2; }
    if (icon_off < 0) icon_off = 0;
    int icon_sx = col_x + icon_off;
    int text_sx = icon_sx + ICON_W + ICON_TEXT_GAP;
    int text_w  = (col_x + col_w) - text_sx;
    if (text_w < 0) text_w = 0;
    if      (line->is_battery) icon_battery(ctx, icon_sx, iy, s_fg, line->icon_extra, large);
    else if (line->is_weather) icon_weather(ctx, icon_sx, iy, s_fg, line->icon_extra, large);
    else                       line->icon_fn(ctx, icon_sx, iy, s_fg, large);
    graphics_context_set_text_color(ctx, s_fg);
    if (text_w > 0) {
      graphics_draw_text(ctx, line->text, font,
        GRect(text_sx, y - INFO_TOP_PAD, text_w, INFO_LINE_H),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
  } else {
    graphics_context_set_text_color(ctx, s_fg);
    graphics_draw_text(ctx, line->text, font,
      GRect(col_x, y - INFO_TOP_PAD, col_w, INFO_LINE_H),
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
static const char *s_day_names[]   = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
static const char *s_day_short[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *s_month_names[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static bool prv_slot_text(char *buf, int len, SlotType slot, struct tm *t, bool wide) {
  switch (slot) {
    case SLOT_EMPTY: return false;
    case SLOT_DAY:
      snprintf(buf,len,"%s", t ? (wide?s_day_names[t->tm_wday]:s_day_short[t->tm_wday]) : "Mon");
      return true;
    case SLOT_DATE:
      if (t) { snprintf(buf,len,"%s %d",s_month_names[t->tm_mon],t->tm_mday); }
      else   { snprintf(buf,len,"Jan 1"); }
      return true;
    case SLOT_DAY_DATE:
      if (t) { snprintf(buf,len,"%s, %s %d",s_day_short[t->tm_wday],s_month_names[t->tm_mon],t->tm_mday); }
      else   { snprintf(buf,len,"Mon, Jan 1"); }
      return true;
    case SLOT_TEMP:
      if (s_weather_temp_f > -900) {
        if (s_cfg_temp_f) snprintf(buf,len,"%dF",s_weather_temp_f);
        else snprintf(buf,len,"%dC",s_weather_temp_c);
      } else snprintf(buf,len,"--");
      return true;
    case SLOT_WEATHER: {
      const char *desc = (s_weather_code==0)?"clear":(s_weather_code<=3)?"partly cloudy":
        (s_weather_code<=48)?"foggy":(s_weather_code<=69)?"rainy":
        (s_weather_code<=79)?"snowy":(s_weather_code<=99)?"stormy":"weather";
      if (s_weather_temp_f > -900) {
        int t2 = s_cfg_temp_f ? s_weather_temp_f : s_weather_temp_c;
        char u = s_cfg_temp_f ? 'F' : 'C';
        if (wide) snprintf(buf,len,"%d%c & %s",t2,u,desc);
        else snprintf(buf,len,"%d%c",t2,u);
      } else snprintf(buf,len,"--");
      return true; }
    case SLOT_STEPS:
#if defined(PBL_HEALTH)
      { char sb[16]; prv_fmt_steps(sb,sizeof(sb),s_steps);
        if (wide&&s_distance_m>0){char db[16];prv_fmt_dist(db,sizeof(db));snprintf(buf,len,"%s steps"DOT"%s",sb,db);}
        else snprintf(buf,len,"%s steps",sb); }
#else
      snprintf(buf,len,wide?"3,450 steps\xc2\xb7 1.7mi":"3,450 steps");
#endif
      return true;
    case SLOT_DISTANCE:
#if defined(PBL_HEALTH)
      { char db[16]; prv_fmt_dist(db,sizeof(db)); snprintf(buf,len,"%s",db); }
#else
      snprintf(buf,len,"1.7mi");
#endif
      return true;
    case SLOT_EXP_STEPS:
#if defined(PBL_HEALTH)
      if (s_steps_expected>0) { char eb[16]; prv_fmt_steps(eb,sizeof(eb),s_steps_expected); snprintf(buf,len,"exp %s",eb); }
      else snprintf(buf,len,"exp --");
#else
      snprintf(buf,len,"exp 3,200");
#endif
      return true;
    case SLOT_PACE:
#if defined(PBL_HEALTH)
      if (s_steps_expected>0) {
        if (wide) { char eb[16]; prv_fmt_steps(eb,sizeof(eb),s_steps_expected);
          snprintf(buf,len,"exp %s"DOT"%d%%",eb,(s_steps*100)/s_steps_expected); }
        else snprintf(buf,len,"%d%% pace",(s_steps*100)/s_steps_expected);
      } else snprintf(buf,len,"-- pace");
#else
      snprintf(buf,len,wide?"exp 3,200\xc2\xb7 108%%":"108%% pace");
#endif
      return true;
    case SLOT_CALORIES:
#if defined(PBL_HEALTH)
      if (s_calories>0) {
        if(wide&&s_heart_rate>0) snprintf(buf,len,"%d cal"DOT"%d bpm",s_calories,s_heart_rate);
        else snprintf(buf,len,"%d cal",s_calories);
      } else snprintf(buf,len,"-- cal");
#else
      snprintf(buf,len,wide?"212 cal\xc2\xb7 72 bpm":"212 cal");
#endif
      return true;
    case SLOT_HEART:
#if defined(PBL_HEALTH)
      if (s_heart_rate>0) snprintf(buf,len,"%d bpm",s_heart_rate);
      else snprintf(buf,len,"-- bpm");
#else
      snprintf(buf,len,"72 bpm");
#endif
      return true;
    case SLOT_SUNRISE:
      { char rb[12]; prv_fmt_time_min(rb,sizeof(rb),s_sunrise_min); snprintf(buf,len,"%s",rb); }
      return true;
    case SLOT_SUNSET:
      { char sb2[12]; prv_fmt_time_min(sb2,sizeof(sb2),s_sunset_min); snprintf(buf,len,"%s",sb2); }
      return true;
    case SLOT_DAYLIGHT:
      if (s_sunrise_min>=0&&s_sunset_min>=0) {
        int mins=s_sunset_min-s_sunrise_min; if(mins<0)mins=0;
        snprintf(buf,len,"%dh%02dm",mins/60,mins%60);
      } else snprintf(buf,len,"--h--m");
      return true;
    case SLOT_BATTERY:
      if (s_charging) { snprintf(buf,len,"%d%% +",s_battery_pct); }
      else            { snprintf(buf,len,"%d%%",s_battery_pct); }
      return true;
    case SLOT_BLUETOOTH:
      snprintf(buf,len,"%s",s_bt_connected?"":"no bt");
      return true;
    default: return false;
  }
}

static IconFn prv_slot_icon(SlotType slot, bool *is_battery, bool *is_weather, int *extra) {
  *is_battery=false; *is_weather=false; *extra=0;
  switch(slot) {
    case SLOT_STEPS: case SLOT_DISTANCE: case SLOT_EXP_STEPS: case SLOT_PACE: return icon_steps;
    case SLOT_CALORIES: return icon_calories;
    case SLOT_HEART: return icon_heart;
    case SLOT_SUNRISE: case SLOT_SUNSET: case SLOT_DAYLIGHT: return icon_sun;
    case SLOT_BATTERY: *is_battery=true; *extra=s_battery_pct; return NULL;
    case SLOT_BLUETOOTH: return icon_bt;
    case SLOT_TEMP: case SLOT_WEATHER: *is_weather=true; *extra=s_weather_code; return NULL;
    default: return NULL;
  }
}

static int build_lines(InfoLine *lines, int max, int *slots, int n_slots, struct tm *t, bool wide) {
  int count = 0;
  for (int i = 0; i < n_slots && count < max; i++) {
    SlotType slot = (SlotType)slots[i];
    if (slot == SLOT_EMPTY) continue;
    if (slot == SLOT_BLUETOOTH && s_bt_connected) continue;
    InfoLine *line = &lines[count];
    char buf[INFO_LINE_BUF];
    if (!prv_slot_text(buf, sizeof(buf), slot, t, wide)) continue;
    snprintf(line->text, INFO_LINE_BUF, "%s", buf);
    bool ib, iw; int ie;
    line->icon_fn    = prv_slot_icon(slot, &ib, &iw, &ie);
    line->is_battery = ib; line->is_weather = iw; line->icon_extra = ie;
    line->has_icon   = (line->icon_fn != NULL || ib || iw);
    count++;
  }
  return count;
}

static int prv_info_block_h(int n, int step) { return n <= 0 ? 0 : INFO_GLYPH_H + (n-1) * step; }
static int prv_compute_target_h(int ub_h, int layout) {
  if (layout != LAYOUT_INFO) return ub_h - MARGIN_OUTER - BOTTOM_MARGIN(ub_h);
  int a, b; prv_info_count(&a, &b);
  int reserved = HALF_UNIT + prv_info_block_h(a, INFO_LINE_STEP_WIDE) + HALF_UNIT
               + HALF_UNIT + prv_info_block_h(b, INFO_LINE_STEP_WIDE) + HALF_UNIT;
  int h = ub_h - reserved;
  return h < H_MIN ? H_MIN : h;
}
static int prv_compute_stacked_h(int ub_h) {
  int h = (ub_h - MARGIN_OUTER - BOTTOM_MARGIN(ub_h) - UNIT) / 2;
  return h < H_ABSOLUTE_MIN ? H_ABSOLUTE_MIN : h;
}
static void prv_update_targets(void) {
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  s_target_h = prv_compute_target_h(ub.size.h, s_layout);
}

// ============================================================
// DRAW LAYER
// ============================================================
static void draw_layer(Layer *layer, GContext *ctx) {
  Layer *root  = window_get_root_layer(s_window);
  GRect ub     = layer_get_unobstructed_bounds(root);
  int ub_top   = ub.origin.y, ub_h = ub.size.h, ub_bot = ub_top + ub_h;
  GRect bounds = layer_get_bounds(layer);

#if defined(PBL_COLOR) && defined(PBL_HEALTH)
  GColor bg = prv_pace_color(s_steps, s_steps_expected);
  s_fg = prv_bg_needs_dark_fg(bg) ? GColorBlack : GColorWhite;
#else
  GColor bg = s_bg; s_fg = GColorWhite;
#endif
  GColor shadow_col = gcolor_equal(s_fg, GColorWhite) ? GColorBlack : GColorWhite;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
#if defined(PBL_PLATFORM_EMERY)
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, s_radius_opts[s_radius_idx], GCornersAll);
#else
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
#endif

  int hr_12 = s_hour;
  if (!s_cfg_24h) { hr_12 = s_hour % 12; if (!hr_12) hr_12 = 12; }
  int h_tens=hr_12/10, h_ones=hr_12%10, m_tens=s_minute/10, m_ones=s_minute%10;
  int bot_margin = BOTTOM_MARGIN(ub_h);
  time_t now_t = time(NULL);
  struct tm *tm_now = (s_phase == PHASE_DONE) ? localtime(&now_t) : NULL;

  if (s_layout == LAYOUT_FULL) {
    int cy = ub_top + MARGIN_OUTER + s_target_h / 2;
    graphics_context_set_fill_color(ctx, shadow_col);
    draw_digits_pass(ctx, h_tens, h_ones, m_tens, m_ones, s_h, cy, SHADOW_DX, SHADOW_DY);
    graphics_context_set_fill_color(ctx, s_fg);
    draw_digits_pass(ctx, h_tens, h_ones, m_tens, m_ones, s_h, cy, 0, 0);

  } else if (s_layout == LAYOUT_INFO) {
    int above_s[NUM_SLOTS], below_s[NUM_SLOTS], na=0, nb=0;
    prv_split_slots(above_s, &na, below_s, &nb);
    int an = build_lines(s_above_lines, INFO_LINES_MAX, above_s, na, tm_now, true);
    int bn = build_lines(s_below_lines, INFO_LINES_MAX, below_s, nb, tm_now, true);
    int above_start = ub_top + HALF_UNIT;
    int above_end   = above_start + prv_info_block_h(an, INFO_LINE_STEP_WIDE);
    int below_end   = ub_bot - HALF_UNIT;
    int time_cy     = (above_end + HALF_UNIT + (below_end - prv_info_block_h(bn, INFO_LINE_STEP_WIDE)) - HALF_UNIT) / 2;
    int col_x = SIDE_MARGIN, col_w = SCREEN_W - 2 * SIDE_MARGIN;
    for (int i = 0; i < an; i++)
      draw_info_line(ctx, &s_above_lines[i],
        above_start + i * INFO_LINE_STEP_WIDE, col_x, col_w, GTextAlignmentCenter);
    for (int i = 0; i < bn; i++) {
      int gy = below_end - prv_info_block_h(bn, INFO_LINE_STEP_WIDE) + i * INFO_LINE_STEP_WIDE;
      draw_info_line(ctx, &s_below_lines[i], gy, col_x, col_w, GTextAlignmentCenter);
    }
    graphics_context_set_fill_color(ctx, shadow_col);
    draw_digits_pass(ctx, h_tens, h_ones, m_tens, m_ones, s_h, time_cy, SHADOW_DX, SHADOW_DY);
    graphics_context_set_fill_color(ctx, s_fg);
    draw_digits_pass(ctx, h_tens, h_ones, m_tens, m_ones, s_h, time_cy, 0, 0);

  } else {
    // Stacked layouts
    int dh    = prv_compute_stacked_h(ub_h);
    int h_cy  = ub_top + MARGIN_OUTER + dh / 2;
    int m_cy  = ub_bot - bot_margin - dh / 2;
    int tens_x, ones_x, col_x, col_w;
    GTextAlignment info_align;

    if (s_layout == LAYOUT_STACK_R) {
      // Digits right, info column left — text LEFT-aligned
      ones_x = SCREEN_W - SIDE_MARGIN - SLOT_W;
      tens_x = ones_x - SLOT_W;
      col_x  = SIDE_MARGIN;
      col_w  = tens_x - SIDE_MARGIN - UNIT;
      info_align = GTextAlignmentLeft;
    } else {
      // Digits left, info column right — text RIGHT-aligned
      tens_x = SIDE_MARGIN;
      ones_x = SIDE_MARGIN + SLOT_W;
      col_x  = ones_x + SLOT_W + UNIT;
      col_w  = SCREEN_W - col_x - SIDE_MARGIN;
      info_align = GTextAlignmentRight;
    }

    if (tm_now && col_w > 20) {
      // Use stacked-specific slot list (not the wide order array)
      int cn = build_lines(s_col_lines, INFO_LINES_MAX, s_cfg_stack, STACK_SLOTS, tm_now, false);
      if (cn > 0) {
        // Fixed step: size for STACK_INFO_LINES from STACK_INFO_MARGIN top to bottom.
        // draw_top/bot are relative to the full screen (ub_top/ub_bot), no MARGIN_OUTER added.
        int draw_top = ub_top + STACK_INFO_MARGIN;
        int draw_bot = ub_bot - STACK_INFO_MARGIN;
        int avail_h  = draw_bot - draw_top - INFO_GLYPH_H;
        int step     = (avail_h > 0 && STACK_INFO_LINES > 1) ? avail_h / (STACK_INFO_LINES - 1) : INFO_LINE_STEP;
        for (int i = 0; i < cn; i++)
          draw_info_line(ctx, &s_col_lines[i],
            draw_top + i * step, col_x, col_w, info_align);
      }
    }
    graphics_context_set_fill_color(ctx, shadow_col);
    draw_stacked_pass(ctx, h_tens, h_ones, m_tens, m_ones, dh, tens_x, ones_x, h_cy, m_cy, SHADOW_DX, SHADOW_DY);
    graphics_context_set_fill_color(ctx, s_fg);
    draw_stacked_pass(ctx, h_tens, h_ones, m_tens, m_ones, dh, tens_x, ones_x, h_cy, m_cy, 0, 0);
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
static bool prv_ease_shrink_step(void) {
  int step = (s_ease_idx < EASE_LEN) ? EASE[EASE_LEN-1-s_ease_idx] : EASE[0]; s_ease_idx++;
  s_h -= step;
  if (s_h <= s_h_min) { s_h = s_h_min; return true; }
  return false;
}
static void prv_start_anticipate(void) { s_phase = PHASE_ANTICIPATE; schedule(ANTICIPATION_MS); }
static void prv_start_expand(void)     { s_phase = PHASE_EXPAND; s_ease_idx = 0; s_overshot = false; schedule(ANIM_STEP_MS); }
static void prv_start_blink(void) {
  s_h = s_target_h; s_going_down = true; s_anim_rep = 0; s_overshot = false;
  s_phase = PHASE_BLINK; layer_mark_dirty(s_canvas_layer); schedule(ANIM_STEP_MS);
}
static void timer_cb(void *data) {
  s_timer = NULL;
  switch (s_phase) {
    case PHASE_ANTICIPATE:
      s_h += ANTICIPATION_PX; layer_mark_dirty(s_canvas_layer);
      s_phase = PHASE_SQUISH; s_ease_idx = 0; s_h_min = H_MIN; schedule(ANIM_STEP_MS); break;
    case PHASE_COUNTDOWN:
      switch (s_cd_sub) {
        case CD_SHRINK:
          s_h -= 6; layer_mark_dirty(s_canvas_layer);
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
        s_h -= 6; layer_mark_dirty(s_canvas_layer);
        if (s_h<=H_MIN) { s_h=H_MIN; s_going_down=false; s_overshot=false; }
        schedule(ANIM_STEP_MS);
      } else {
        bool d=prv_ease_expand_step(); layer_mark_dirty(s_canvas_layer);
        if(d) { if(++s_anim_rep<BLINK_REPS){s_going_down=true;s_overshot=false;s_ease_idx=0;schedule(ANIM_STEP_MS);}
          else{s_phase=PHASE_DONE;layer_mark_dirty(s_canvas_layer);} }
      } break;
    case PHASE_SQUISH: {
      bool d=prv_ease_shrink_step(); layer_mark_dirty(s_canvas_layer);
      if(d) { if(s_digit_pending){s_hour=s_pending_hour;s_minute=s_pending_minute;s_digit_pending=false;} prv_start_expand(); }
      else { schedule(ANIM_STEP_MS); } break; }
    case PHASE_EXPAND: {
      bool d=prv_ease_expand_step(); layer_mark_dirty(s_canvas_layer);
      if(d){s_phase=PHASE_DONE;layer_mark_dirty(s_canvas_layer);} break; }
    case PHASE_SHAKE_CYCLE: {
      static const int OFF[SHAKE_LEN]={0,-UNIT,-(UNIT*2),-(UNIT*3),-(UNIT*2),-UNIT,0};
      if(++s_anim_step<SHAKE_LEN) {
        int h=s_target_h+OFF[s_anim_step]; s_h=h<H_ABSOLUTE_MIN?H_ABSOLUTE_MIN:h;
        layer_mark_dirty(s_canvas_layer); schedule(ANIM_STEP_MS);
      } else { s_h=s_target_h; s_phase=PHASE_DONE; layer_mark_dirty(s_canvas_layer); } break; }
    case PHASE_DONE: break;
  }
}

// ============================================================
// EVENT HANDLERS
// ============================================================
static void unobstructed_change(AnimationProgress progress, void *ctx) {
  prv_update_targets();
  if (s_phase == PHASE_DONE) { s_h = s_target_h; layer_mark_dirty(s_canvas_layer); }
}
static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_phase != PHASE_DONE) return;
  s_layout = prv_next_layout(s_layout);
  prv_update_targets();
  if (s_layout == LAYOUT_FULL) {
    s_phase = PHASE_SHAKE_CYCLE; s_anim_step = 0;
    s_h = s_target_h; schedule(ANIM_STEP_MS);
  } else { s_h_min = H_MIN; prv_start_anticipate(); }
  layer_mark_dirty(s_canvas_layer);
}
#if defined(PBL_TOUCH)
static void touch_handler(const TouchEvent *event, void *context) {
  if (event->type != TouchEvent_Touchdown) return;
  vibes_enqueue_custom_pattern(TOUCH_VIBE);
#if defined(PBL_PLATFORM_EMERY)
  s_radius_idx = (s_radius_idx + 1) % RADIUS_COUNT;
  layer_mark_dirty(s_canvas_layer);
#endif
}
#endif
static void prv_noop_click(ClickRecognizerRef ref, void *ctx) { (void)ref; (void)ctx; }
static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP,     prv_noop_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_noop_click);
  window_single_click_subscribe(BUTTON_ID_DOWN,   prv_noop_click);
}
static void tick_handler(struct tm *t, TimeUnits units) {
  bool animated = (s_layout == LAYOUT_FULL || s_layout == LAYOUT_INFO);
  if (s_phase == PHASE_DONE && animated) {
    s_pending_hour=t->tm_hour; s_pending_minute=t->tm_min; s_digit_pending=true;
    s_h_min=H_MIN; prv_start_anticipate();
  } else if (s_phase == PHASE_SQUISH) {
    s_pending_hour=t->tm_hour; s_pending_minute=t->tm_min; s_digit_pending=true;
  } else { s_hour=t->tm_hour; s_minute=t->tm_min; layer_mark_dirty(s_canvas_layer); }
#if defined(PBL_HEALTH)
  prv_update_health();
#endif
}
static void battery_handler(BatteryChargeState state) {
  s_battery_pct=state.charge_percent; s_charging=state.is_charging; layer_mark_dirty(s_canvas_layer);
}
static void bt_handler(bool connected) {
  s_bt_connected=connected; layer_mark_dirty(s_canvas_layer);
}
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;
  t = dict_find(iter, MESSAGE_KEY_WeatherTempF); if(t) s_weather_temp_f=(int)t->value->int32;
  t = dict_find(iter, MESSAGE_KEY_WeatherTempC); if(t) s_weather_temp_c=(int)t->value->int32;
  t = dict_find(iter, MESSAGE_KEY_WeatherCode);  if(t) s_weather_code=(int)t->value->int32;
  t = dict_find(iter, MESSAGE_KEY_SunriseTime);
  if(t){time_t ts=(time_t)(uint32_t)t->value->uint32;struct tm*lt=localtime(&ts);if(lt)s_sunrise_min=lt->tm_hour*60+lt->tm_min;}
  t = dict_find(iter, MESSAGE_KEY_SunsetTime);
  if(t){time_t ts=(time_t)(uint32_t)t->value->uint32;struct tm*lt=localtime(&ts);if(lt)s_sunset_min=lt->tm_hour*60+lt->tm_min;}
  for (int i = 0; i < NUM_SLOTS + 1; i++) {
    t = dict_find(iter, MESSAGE_KEY_CfgSlot1 + i);
    if (t) s_cfg_order[i] = (int)t->value->int32;
  }
  // Stacked slots: CfgSlot8..CfgSlot15 (keys 12..19 offset from CfgSlot1=5 → keys 12..19)
  // Actually use MESSAGE_KEY_CfgSlot1 + 7 through +14 for stack slots 0..7
  // TODO: add dedicated message keys in appinfo.json in next session
  t = dict_find(iter, MESSAGE_KEY_CfgTempUnit);    if(t) s_cfg_temp_f  =(t->value->int32==0);
  t = dict_find(iter, MESSAGE_KEY_CfgDistUnit);    if(t) s_cfg_dist_mi =(t->value->int32==0);
  t = dict_find(iter, MESSAGE_KEY_CfgClockFormat); if(t) s_cfg_24h     =(t->value->int32==1);
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
#if defined(PBL_TOUCH)
  touch_service_subscribe(touch_handler, NULL);
#endif
  s_phase = PHASE_DONE; s_overshot = false; s_ease_idx = 0;
  layer_mark_dirty(s_canvas_layer);
}
static void window_unload(Window *window) {
  unobstructed_area_service_unsubscribe();
  accel_tap_service_unsubscribe();
#if defined(PBL_TOUCH)
  touch_service_unsubscribe();
#endif
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  layer_destroy(s_canvas_layer);
}
static void init(void) {
  s_fg = GColorWhite; s_bg = GColorBlack;
  prv_load_config();
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
  window_destroy(s_window);
}
int main(void) { init(); app_event_loop(); deinit(); }
