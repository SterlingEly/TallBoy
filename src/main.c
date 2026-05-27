#include <pebble.h>

// ============================================================
// TallBoy — main.c  v3.46
// Design: Sterling Ely. Code: Sterling Ely + Claude. 2026.
//
// v3.46:
//   Touch restored (emery/gabbro, PBL_TOUCH):
//     Confirmed API: touch_service_subscribe(handler, context)
//     Handler signature: void(*)(const TouchEvent *, void *)
//     TouchEvent struct fields (type enum, x, y) unknown —
//     fires on any touch. TODO: filter to touch-down only once
//     event type enum name is confirmed from pebble.h / docs.
//
//   Interaction model redesigned:
//     TOUCH  = advance layout forward through cycle (squish anim)
//     SHAKE  = always return to LAYOUT_FULL (shake anim)
//     This gives two independent directional gestures.
//     Shake-in-place on LAYOUT_FULL still gives satisfying feedback.
//
//   Also removed unused s_lseq_idx (leftover from old seq system).
//   Bare % in SLOT_PACE stacked fallback fixed: "87%%" not "87%".
// ============================================================

// #define DEBUG_TEXT_BOXES

typedef enum {
  SLOT_EMPTY   = 0,
  SLOT_DATE    = 1,
  SLOT_WEATHER = 2,
  SLOT_SOLAR   = 3,
  SLOT_STEPS   = 4,
  SLOT_PACE    = 5,
  SLOT_HRCAL   = 6,
  SLOT_BATTERY = 7
} SlotType;

#define LAYOUT_FULL    0
#define LAYOUT_INFO    1
#define LAYOUT_STACK_R 2
#define LAYOUT_STACK_L 3

#define SHAKE_LEN  7

#if defined(PBL_PLATFORM_EMERY)
  #define SCREEN_W          200
  #define SCREEN_H          228
  #define SLOT_W             40
  #define COLON_SLOT_X       80
  #define SLOT_H_TENS        12
  #define SLOT_H_ONES        52
  #define SLOT_M_TENS       108
  #define SLOT_M_ONES       148
  #define SIDE_MARGIN        12
  #define HALF_UNIT           4
  #define INFO_LINE_H        28
  #define INFO_GLYPH_H       18
  #define INFO_TOP_PAD       10
  #define INFO_DESCENDER      4
  #define INFO_CAP_OFFSET     3
  #define INFO_BOT_ADJUST     2
  #define INFO_LINE_STEP     26
  #define INFO_LINE_STEP_WIDE 22
  #define INFO_FONT_KEY    FONT_KEY_GOTHIC_24_BOLD
  #define UNIT                8
  static const uint16_t s_radius_opts[] = { UNIT*2, UNIT*3, UNIT*4 };
  #define RADIUS_COUNT 3
  static int s_radius_idx = 0;
#else
  #define SCREEN_W          144
  #define SCREEN_H          168
  #define SLOT_W             30
  #define COLON_SLOT_X       57
  #define SLOT_H_TENS         6
  #define SLOT_H_ONES        36
  #define SLOT_M_TENS        78
  #define SLOT_M_ONES       108
  #define SIDE_MARGIN         6
  #define HALF_UNIT           3
  #define INFO_LINE_H        20
  #define INFO_GLYPH_H       14
  #define INFO_TOP_PAD        6
  #define INFO_DESCENDER      3
  #define INFO_CAP_OFFSET     2
  #define INFO_BOT_ADJUST     1
  #define INFO_LINE_STEP     20
  #define INFO_LINE_STEP_WIDE 17
  #define INFO_FONT_KEY    FONT_KEY_GOTHIC_18_BOLD
  #define UNIT                6
#endif

#define MARGIN_CANVAS   UNIT
#define MARGIN_DIGIT    UNIT
#define MARGIN_OUTER    (MARGIN_CANVAS + MARGIN_DIGIT)
#define HALF_SLOT_PAD   (UNIT / 2)
#define GLYPH_W         (UNIT * 4)
#define H_MIN           (UNIT * 11)
#define H_ABSOLUTE_MIN  (UNIT * 5)
#define INFO_LINE_BUF   40
#define INFO_LINES_MAX  8
#define NUM_SLOTS       6

static const int EASE[10] = { 4, 6, 8, 10, 12, 12, 10, 8, 6, 4 };
#define EASE_LEN  10

#define ANIM_STEP_MS      16
#define ANIM_SNAP_MS      80
#define ANIM_OVERSHOOT    UNIT
#define ANTICIPATION_PX   HALF_UNIT
#define ANTICIPATION_MS   16
#define COUNTDOWN_HOLD_MS 120
#define BLINK_REPS          2
#define STEPS_AVG_MAX_MIN 120
#define DOT " \xc2\xb7 "

#define QUICK_LOOK_ACTIVE(ub_h)  ((ub_h) < (SCREEN_H - UNIT))
#define BOTTOM_MARGIN(ub_h)      (QUICK_LOOK_ACTIVE(ub_h) ? MARGIN_DIGIT : MARGIN_OUTER)

typedef enum { CD_SHRINK, CD_HOLD_MIN, CD_EXPAND, CD_HOLD_MAX } CdSubPhase;
typedef enum {
  PHASE_COUNTDOWN,
  PHASE_BLINK,
  PHASE_DONE,
  PHASE_ANTICIPATE,
  PHASE_SQUISH,
  PHASE_EXPAND,
  PHASE_SHAKE_CYCLE
} Phase;

static Window    *s_window;
static Layer     *s_canvas_layer;
static int        s_hour = 0, s_minute = 0;
static int        s_h = 0, s_target_h = 0;
static int        s_h_min = 0;
static int        s_ease_idx = 0;
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
static int        s_steps = 0, s_distance_m = 0, s_calories = 0, s_heart_rate = 0;
static int        s_steps_expected = -1;
static int        s_sunrise_min = -1, s_sunset_min = -1;
static char       s_weather_temp[8] = "";
static char       s_weather_desc[32] = "";
static int        s_layout = LAYOUT_FULL;

static int  s_cfg_time_pos = 3;
static int  s_cfg_slots[NUM_SLOTS] = {1,2,3,4,5,6};
static bool s_cfg_temp_f    = true;
static bool s_cfg_dist_mi   = true;
static bool s_cfg_24h       = false;

#define PERSIST_CFG_TIME_POS   1
#define PERSIST_CFG_SLOTS      2
#define PERSIST_CFG_FLAGS      3

static void prv_save_config(void) {
  persist_write_int(PERSIST_CFG_TIME_POS, s_cfg_time_pos);
  int32_t packed = 0;
  for (int i = 0; i < NUM_SLOTS; i++) packed |= (s_cfg_slots[i] & 0x7) << (i * 4);
  persist_write_int(PERSIST_CFG_SLOTS, packed);
  int32_t flags = (s_cfg_temp_f ? 1 : 0) | (s_cfg_dist_mi ? 2 : 0) | (s_cfg_24h ? 4 : 0);
  persist_write_int(PERSIST_CFG_FLAGS, flags);
}

static void prv_load_config(void) {
  if (persist_exists(PERSIST_CFG_TIME_POS))
    s_cfg_time_pos = persist_read_int(PERSIST_CFG_TIME_POS);
  if (persist_exists(PERSIST_CFG_SLOTS)) {
    int32_t packed = persist_read_int(PERSIST_CFG_SLOTS);
    for (int i = 0; i < NUM_SLOTS; i++) s_cfg_slots[i] = (packed >> (i * 4)) & 0x7;
  }
  if (persist_exists(PERSIST_CFG_FLAGS)) {
    int32_t flags = persist_read_int(PERSIST_CFG_FLAGS);
    s_cfg_temp_f  = (flags & 1) != 0;
    s_cfg_dist_mi = (flags & 2) != 0;
    s_cfg_24h     = (flags & 4) != 0;
  }
}

// FULL → INFO (if configured) → STACK_L → STACK_R → FULL
static int prv_next_layout(int current) {
  bool has_info = (s_cfg_time_pos > 0 && s_cfg_time_pos < NUM_SLOTS);
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
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
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

static void fill_arc(GContext *ctx, int cx, int cy, int ro, int ri, int a0, int a1) {
  GRect b = GRect(cx - ro, cy - ro, ro * 2, ro * 2);
  graphics_fill_radial(ctx, b, GOvalScaleModeFitCircle, (uint16_t)(ro - ri),
                       DEG_TO_TRIGANGLE(a0), DEG_TO_TRIGANGLE(a1));
}

static void draw_digit_vec(GContext *ctx, int digit, int slot_x, int cy, int h) {
  graphics_context_set_fill_color(ctx, s_fg);
  const int ro = UNIT * 2, ri = UNIT, sw = UNIT;
  int gx = slot_x + HALF_SLOT_PAD, gx_r = gx + GLYPH_W - sw, cap_cx = gx + ro;
  int top_y = cy - h / 2, bot_y = cy + h / 2;
  int bar = (h - 7 * UNIT) / 2; if (bar < 0) bar = 0;
  int t_bc = cy - (ro - HALF_UNIT), t_tc = t_bc - bar;
  int b_tc = cy + (ro - HALF_UNIT), b_bc = b_tc + bar;
  int tail = (h > H_MIN) ? (h - H_MIN) / 4 : 0;
  #define top_cy  (cy - (h - ro*2) / 2)
  #define bot_cy  (cy + (h - ro*2) / 2)
  #define VBAR(x,y0,y1) if((y1)>(y0)) graphics_fill_rect(ctx,GRect((x),(y0),sw,(y1)-(y0)),0,GCornerNone)
  #define HBAR(y) graphics_fill_rect(ctx,GRect(gx,(y),GLYPH_W,sw),0,GCornerNone)
  #define NUB(x,y) graphics_fill_rect(ctx,GRect((x),(y),sw,sw),0,GCornerNone)
  switch (digit) {
    case 0: fill_arc(ctx,cap_cx,top_cy,ro,ri,270,450); fill_arc(ctx,cap_cx,bot_cy,ro,ri,90,270); VBAR(gx,top_cy,bot_cy); VBAR(gx_r,top_cy,bot_cy); break;
    case 1: { HBAR(bot_y-sw); int sx=gx+GLYPH_W/2-sw/2,cr=sx+sw,dh=cr-gx; VBAR(sx,top_y,bot_y-sw); if(dh>0){GPoint p[5]={{cr-1,top_y},{cr-1,top_y+sw-HALF_UNIT},{gx-1,top_y+sw+dh-HALF_UNIT},{gx-1,top_y+dh-sw},{sx-1,top_y}}; GPathInfo pi={5,p}; GPath*pa=gpath_create(&pi); if(pa){gpath_draw_filled(ctx,pa);gpath_destroy(pa);}} break; }
    case 2: { fill_arc(ctx,cap_cx,top_cy,ro,ri,270,450); VBAR(gx,top_cy,top_cy+tail); VBAR(gx_r,top_cy,top_cy+tail); int dy=(bot_y-sw)-(top_cy+tail); if(dy>0){GPoint p[4]={{gx_r-1,top_cy+tail},{gx_r+sw,top_cy+tail},{gx-1+sw+1,bot_y-sw},{gx-1,bot_y-sw}}; GPathInfo pi={4,p}; GPath*pa=gpath_create(&pi); if(pa){gpath_draw_filled(ctx,pa);gpath_destroy(pa);}} HBAR(bot_y-sw); break; }
    case 3: fill_arc(ctx,cap_cx,t_tc,ro,ri,270,450); VBAR(gx,t_tc,t_tc+tail); VBAR(gx_r,t_tc,t_bc); fill_arc(ctx,cap_cx,t_bc,ro,ri,90,180); NUB(gx+sw,t_bc+sw); fill_arc(ctx,cap_cx,b_tc,ro,ri,360,450); VBAR(gx,b_bc-tail,b_bc); VBAR(gx_r,b_tc,b_bc); fill_arc(ctx,cap_cx,b_bc,ro,ri,90,270); break;
    case 4: VBAR(gx,top_y,cy-HALF_UNIT+sw); VBAR(gx_r,top_y,bot_y); graphics_fill_rect(ctx,GRect(gx,cy-HALF_UNIT,GLYPH_W,sw),0,GCornerNone); break;
    case 5: HBAR(top_y); VBAR(gx,top_y+sw,b_tc); fill_arc(ctx,cap_cx,b_tc,ro,ri,270,450); fill_arc(ctx,cap_cx,b_bc,ro,ri,90,270); VBAR(gx_r,b_tc,b_bc); if(tail>0)VBAR(gx,b_bc-tail,b_bc); break;
    case 6: fill_arc(ctx,cap_cx,top_cy,ro,ri,270,450); VBAR(gx_r,top_cy,top_cy+tail); VBAR(gx,top_cy,b_bc); fill_arc(ctx,cap_cx,b_tc,ro,ri,270,450); fill_arc(ctx,cap_cx,b_bc,ro,ri,90,270); VBAR(gx_r,b_tc,b_bc); break;
    case 7: { HBAR(top_y); GPoint p[4]={{gx_r-1,top_y+sw},{gx_r+sw,top_y+sw},{gx+sw,bot_y-1},{gx-1,bot_y-1}}; GPathInfo pi={4,p}; GPath*pa=gpath_create(&pi); if(pa){gpath_draw_filled(ctx,pa);gpath_destroy(pa);} break; }
    case 8: fill_arc(ctx,cap_cx,t_tc,ro,ri,270,450); fill_arc(ctx,cap_cx,t_bc,ro,ri,90,270); VBAR(gx,t_tc,t_bc); VBAR(gx_r,t_tc,t_bc); fill_arc(ctx,cap_cx,b_tc,ro,ri,270,450); fill_arc(ctx,cap_cx,b_bc,ro,ri,90,270); VBAR(gx,b_tc,b_bc); VBAR(gx_r,b_tc,b_bc); break;
    case 9: fill_arc(ctx,cap_cx,t_tc,ro,ri,270,450); fill_arc(ctx,cap_cx,t_bc,ro,ri,90,270); VBAR(gx,t_tc,t_bc); VBAR(gx,bot_cy-tail,bot_cy); VBAR(gx_r,t_tc,bot_cy); fill_arc(ctx,cap_cx,bot_cy,ro,ri,90,270); break;
    default: break;
  }
  #undef top_cy
  #undef bot_cy
  #undef VBAR
  #undef HBAR
  #undef NUB
}

static void draw_colon_vec(GContext *ctx, int slot_x, int cy, int h) {
  graphics_context_set_fill_color(ctx, s_fg);
  int r = UNIT / 2, dx = slot_x + SLOT_W / 2;
  graphics_fill_radial(ctx, GRect(dx-r, cy-h/4-r+2, r*2, r*2),
                       GOvalScaleModeFitCircle, (uint16_t)r, 0, DEG_TO_TRIGANGLE(360));
  graphics_fill_radial(ctx, GRect(dx-r, cy+h/4-r-2, r*2, r*2),
                       GOvalScaleModeFitCircle, (uint16_t)r, 0, DEG_TO_TRIGANGLE(360));
}

static void draw_digits_vec(GContext *ctx, int h_tens, int h_ones,
                             int m_tens, int m_ones, int h, int cy) {
  draw_digit_vec(ctx, h_tens, SLOT_H_TENS, cy, h);
  draw_digit_vec(ctx, h_ones, SLOT_H_ONES, cy, h);
  draw_colon_vec(ctx, COLON_SLOT_X, cy, h);
  draw_digit_vec(ctx, m_tens, SLOT_M_TENS, cy, h);
  draw_digit_vec(ctx, m_ones, SLOT_M_ONES, cy, h);
}

static void draw_stacked_vec(GContext *ctx, int h_tens, int h_ones,
                              int m_tens, int m_ones, int dh,
                              int tens_x, int ones_x, int h_cy, int m_cy) {
  draw_digit_vec(ctx, h_tens, tens_x, h_cy, dh);
  draw_digit_vec(ctx, h_ones, ones_x, h_cy, dh);
  draw_digit_vec(ctx, m_tens, tens_x, m_cy, dh);
  draw_digit_vec(ctx, m_ones, ones_x, m_cy, dh);
}

static int prv_info_block_h(int n, int step) {
  if (n <= 0) return 0;
  return INFO_GLYPH_H + (n - 1) * step;
}

static void prv_info_split(int *above, int *below) {
  int total = 0;
  for (int i = 0; i < NUM_SLOTS; i++) if (s_cfg_slots[i] != SLOT_EMPTY) total++;
  int pos = s_cfg_time_pos; if (pos > total) pos = total;
  *above = pos; *below = total - pos;
}

static int prv_compute_target_h(int ub_h, int layout) {
  if (layout != LAYOUT_INFO) return ub_h - MARGIN_OUTER - BOTTOM_MARGIN(ub_h);
  int above, below; prv_info_split(&above, &below);
  int reserved = HALF_UNIT + prv_info_block_h(above, INFO_LINE_STEP_WIDE) + HALF_UNIT;
  reserved    += HALF_UNIT + prv_info_block_h(below, INFO_LINE_STEP_WIDE) + HALF_UNIT;
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

static const char *s_day_names[] = {
  "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};
static const char *s_month_names[] = {
  "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

static void prv_fmt_distance(char *buf, int len) {
  if (s_cfg_dist_mi) {
    int mx = (s_distance_m * 10) / 1609; snprintf(buf, len, "%d.%d mi", mx/10, mx%10);
  } else {
    int kx = (s_distance_m * 10) / 1000; snprintf(buf, len, "%d.%d km", kx/10, kx%10);
  }
}
static void prv_fmt_steps_short(char *buf, int len, int steps) {
  if (steps >= 10000) snprintf(buf, len, "%d,%03d", steps/1000, steps%1000);
  else if (steps >= 1000) snprintf(buf, len, "%d.%dk", steps/1000, (steps%1000)/100);
  else snprintf(buf, len, "%d", steps);
}
static void prv_fmt_steps_long(char *buf, int len, int steps) {
  if (steps >= 1000) snprintf(buf, len, "%d,%03d", steps/1000, steps%1000);
  else snprintf(buf, len, "%d", steps);
}
static void prv_fmt_time_min(char *buf, int len, int total_min) {
  if (total_min < 0) { snprintf(buf, len, "--"); return; }
  int h = (total_min / 60) % 12; if (!h) h = 12;
  snprintf(buf, len, "%d:%02d%s", h, total_min % 60, total_min < 720 ? "am" : "pm");
}
static void prv_fmt_bat_bt(char *buf, int len) {
  if (!s_bt_connected) snprintf(buf, len, "no phone");
  else if (s_charging) snprintf(buf, len, "%d%% bat +", s_battery_pct);
  else snprintf(buf, len, "%d%% bat", s_battery_pct);
}
static void prv_fmt_temp(char *buf, int len) {
  if (s_weather_temp[0]) snprintf(buf, len, "%s", s_weather_temp);
  else snprintf(buf, len, "--");
}

static void prv_render_slot_wide(char *buf, int len, SlotType slot, struct tm *t) {
  switch (slot) {
    case SLOT_DATE:
      if (t) snprintf(buf, len, "%s, %s %d", s_day_names[t->tm_wday], s_month_names[t->tm_mon], t->tm_mday);
      else   snprintf(buf, len, "Mon, Jan 1");
      break;
    case SLOT_WEATHER:
      if (s_weather_temp[0] && s_weather_desc[0])
        snprintf(buf, len, "%.7s & %.29s", s_weather_temp, s_weather_desc);
      else if (s_weather_temp[0]) snprintf(buf, len, "%s", s_weather_temp);
      else snprintf(buf, len, "72 F & sunny");
      break;
    case SLOT_SOLAR:
      if (s_sunrise_min >= 0 || s_sunset_min >= 0) {
        char rise[12], set[12];
        prv_fmt_time_min(rise, sizeof(rise), s_sunrise_min);
        prv_fmt_time_min(set,  sizeof(set),  s_sunset_min);
        snprintf(buf, len, "%s" DOT "%s", rise, set);
      } else snprintf(buf, len, "6:02am" DOT "8:14pm");
      break;
    case SLOT_STEPS:
#if defined(PBL_HEALTH)
      { char sb[16]; prv_fmt_steps_long(sb, sizeof(sb), s_steps);
        if (s_distance_m > 0) {
          char db[12]; prv_fmt_distance(db, sizeof(db));
          snprintf(buf, len, "%s steps" DOT "%s", sb, db);
        } else snprintf(buf, len, "%s steps", sb); }
#else
      snprintf(buf, len, "3,500 steps" DOT "2.1 mi");
#endif
      break;
    case SLOT_PACE:
#if defined(PBL_HEALTH)
      if (s_steps_expected > 0) {
        char eb[16]; prv_fmt_steps_long(eb, sizeof(eb), s_steps_expected);
        snprintf(buf, len, "exp %s" DOT "%d%%", eb, (s_steps*100)/s_steps_expected);
      } else snprintf(buf, len, "exp -- " DOT " --");
#else
      snprintf(buf, len, "exp 3,500" DOT "100%%");
#endif
      break;
    case SLOT_HRCAL:
#if defined(PBL_HEALTH)
      { bool hr = s_heart_rate > 0, cal = s_calories > 0;
        if (hr && cal)  snprintf(buf, len, "%d bpm" DOT "%d cal", s_heart_rate, s_calories);
        else if (hr)    snprintf(buf, len, "%d bpm", s_heart_rate);
        else if (cal)   snprintf(buf, len, "%d cal", s_calories);
        else            snprintf(buf, len, "-- bpm" DOT "-- cal"); }
#else
      snprintf(buf, len, "72 bpm" DOT "212 cal");
#endif
      break;
    case SLOT_BATTERY: prv_fmt_bat_bt(buf, len); break;
    default: buf[0] = '\0'; break;
  }
}

static void prv_render_slot_stacked(char *buf, int len, SlotType slot, struct tm *t) {
  switch (slot) {
    case SLOT_DATE:
      if (t) snprintf(buf, len, "%s %d", s_month_names[t->tm_mon], t->tm_mday);
      else   snprintf(buf, len, "Jan 1");
      break;
    case SLOT_WEATHER: prv_fmt_temp(buf, len); break;
    case SLOT_SOLAR:
      if (s_sunrise_min >= 0 || s_sunset_min >= 0) {
        char rise[12]; prv_fmt_time_min(rise, sizeof(rise), s_sunrise_min);
        snprintf(buf, len, "%s rise", rise);
      } else snprintf(buf, len, "no solar");
      break;
    case SLOT_STEPS:
#if defined(PBL_HEALTH)
      { char sb[16]; prv_fmt_steps_short(sb, sizeof(sb), s_steps);
        snprintf(buf, len, "%s steps", sb); }
#else
      snprintf(buf, len, "3.5k steps");
#endif
      break;
    case SLOT_PACE:
#if defined(PBL_HEALTH)
      if (s_steps_expected > 0) snprintf(buf, len, "%d%% pace", (s_steps*100)/s_steps_expected);
      else snprintf(buf, len, "-- pace");
#else
      snprintf(buf, len, "87%% pace");  // %% = literal % in snprintf
#endif
      break;
    case SLOT_HRCAL:
#if defined(PBL_HEALTH)
      if (s_heart_rate > 0) snprintf(buf, len, "%d bpm", s_heart_rate);
      else if (s_calories > 0) snprintf(buf, len, "%d cal", s_calories);
      else snprintf(buf, len, "-- bpm");
#else
      snprintf(buf, len, "72 bpm");
#endif
      break;
    case SLOT_BATTERY: prv_fmt_bat_bt(buf, len); break;
    default: buf[0] = '\0'; break;
  }
}

static int build_info_lines_wide(char lines[][INFO_LINE_BUF], int max_lines, struct tm *t) {
  int n = 0;
  for (int i = 0; i < NUM_SLOTS && n < max_lines; i++) {
    SlotType slot = (SlotType)s_cfg_slots[i];
    if (slot == SLOT_EMPTY) continue;
    prv_render_slot_wide(lines[n], INFO_LINE_BUF, slot, t);
    if (lines[n][0]) n++;
  }
  return n;
}

static int build_info_lines_stacked(char lines[][INFO_LINE_BUF], int max_lines, struct tm *t) {
  int n = 0;
  for (int i = 0; i < NUM_SLOTS && n < max_lines; i++) {
    SlotType slot = (SlotType)s_cfg_slots[i];
    if (slot == SLOT_EMPTY) continue;
    prv_render_slot_stacked(lines[n], INFO_LINE_BUF, slot, t);
    if (lines[n][0]) n++;
  }
  return n;
}

static GFont prv_info_font(void) { return fonts_get_system_font(INFO_FONT_KEY); }

static void draw_text_at_glyph_y(GContext *ctx, const char *text,
                                  int x, int glyph_y, int width) {
  GRect r = GRect(x, glyph_y - INFO_TOP_PAD, width, INFO_LINE_H);
#if defined(DEBUG_TEXT_BOXES)
  graphics_context_set_fill_color(ctx, GColorMagenta);
  graphics_fill_rect(ctx, r, 0, GCornerNone);
#endif
  graphics_context_set_text_color(ctx, s_fg);
  graphics_draw_text(ctx, text, prv_info_font(), r,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_info_column(GContext *ctx, GRect area, struct tm *t) {
  char lines[INFO_LINES_MAX][INFO_LINE_BUF];
  int n = build_info_lines_stacked(lines, INFO_LINES_MAX, t);
  if (!n) return;
  int glyph_top = area.origin.y + MARGIN_OUTER;
  int glyph_bot = area.origin.y + area.size.h - MARGIN_OUTER + INFO_GLYPH_H;
  int slot_h = n > 1 ? (glyph_bot - glyph_top - INFO_GLYPH_H) / (n - 1) : 0;
  for (int i = 0; i < n; i++)
    draw_text_at_glyph_y(ctx, lines[i], area.origin.x, glyph_top + i * slot_h, area.size.w);
}

static void draw_info_block_down(GContext *ctx, char lines[][INFO_LINE_BUF], int n,
                                  int glyph_y_start, int width, int x) {
  for (int i = 0; i < n; i++)
    draw_text_at_glyph_y(ctx, lines[i], x, glyph_y_start + i * INFO_LINE_STEP_WIDE, width);
}

static void draw_info_block_up(GContext *ctx, char lines[][INFO_LINE_BUF],
                                int first_idx, int n, int glyph_bot, int width, int x) {
  if (n <= 0) return;
  for (int i = 0; i < n; i++) {
    int glyph_y = glyph_bot - INFO_GLYPH_H - (n - 1 - i) * INFO_LINE_STEP_WIDE;
    draw_text_at_glyph_y(ctx, lines[first_idx + i], x, glyph_y, width);
  }
}

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
  int h_tens = hr_12/10, h_ones = hr_12%10, m_tens = s_minute/10, m_ones = s_minute%10;
  int bot_margin = BOTTOM_MARGIN(ub_h);

  time_t now_t = time(NULL);
  struct tm *tm_now = (s_phase == PHASE_DONE) ? localtime(&now_t) : NULL;

  if (s_layout == LAYOUT_FULL) {
    int cy = ub_top + MARGIN_OUTER + s_target_h / 2;
    draw_digits_vec(ctx, h_tens, h_ones, m_tens, m_ones, s_h, cy);

  } else if (s_layout == LAYOUT_INFO) {
    int lines_above, lines_below;
    prv_info_split(&lines_above, &lines_below);

    int above_start = ub_top + HALF_UNIT;
    int above_end   = above_start + prv_info_block_h(lines_above, INFO_LINE_STEP_WIDE);
    int below_end   = ub_bot - HALF_UNIT;
    int below_start = below_end - prv_info_block_h(lines_below, INFO_LINE_STEP_WIDE);
    int time_cy     = (above_end + HALF_UNIT + below_start - HALF_UNIT) / 2;

    char all_lines[INFO_LINES_MAX][INFO_LINE_BUF];
    int total = build_info_lines_wide(all_lines, INFO_LINES_MAX, tm_now);

    int an = lines_above < total ? lines_above : total;
    draw_info_block_down(ctx, all_lines, an, above_start, SCREEN_W - 2*SIDE_MARGIN, SIDE_MARGIN);
    int bn = total - lines_above; if (bn > lines_below) bn = lines_below;
    if (bn > 0)
      draw_info_block_up(ctx, all_lines, lines_above, bn, below_end, SCREEN_W - 2*SIDE_MARGIN, SIDE_MARGIN);

    draw_digits_vec(ctx, h_tens, h_ones, m_tens, m_ones, s_h, time_cy);

  } else {
    int dh    = prv_compute_stacked_h(ub_h);
    int h_cy  = ub_top + MARGIN_OUTER + dh / 2;
    int m_cy  = ub_bot - bot_margin - dh / 2;
    int tens_x, ones_x, info_x, info_w;

    if (s_layout == LAYOUT_STACK_R) {
      ones_x = SCREEN_W - SIDE_MARGIN - SLOT_W; tens_x = ones_x - SLOT_W;
      info_x = SIDE_MARGIN; info_w = tens_x - SIDE_MARGIN * 2;
    } else {
      tens_x = SIDE_MARGIN; ones_x = SIDE_MARGIN + SLOT_W;
      info_x = ones_x + SLOT_W + SIDE_MARGIN; info_w = SCREEN_W - info_x - SIDE_MARGIN;
    }

    if (tm_now && info_w > 20)
      draw_info_column(ctx, GRect(info_x, ub_top, info_w, ub_h), tm_now);
    draw_stacked_vec(ctx, h_tens, h_ones, m_tens, m_ones, dh, tens_x, ones_x, h_cy, m_cy);
  }
}

// ---------------------------------------------------------------------------
// Shared layout-advance logic (used by both touch and shake-advance)
// ---------------------------------------------------------------------------
static void timer_cb(void *data);

static void schedule(uint32_t ms) {
  if (s_timer) app_timer_cancel(s_timer);
  s_timer = app_timer_register(ms, timer_cb, NULL);
}

static bool prv_ease_expand_step(void) {
  if (s_overshot) { s_h = s_target_h; s_overshot = false; return true; }
  int step = (s_ease_idx < EASE_LEN) ? EASE[s_ease_idx] : EASE[EASE_LEN - 1];
  s_ease_idx++;
  s_h += step;
  int peak = s_target_h + ANIM_OVERSHOOT;
  if (s_h >= peak) { s_h = peak; s_overshot = true; schedule(ANIM_SNAP_MS); return false; }
  if (s_h >= s_target_h) { s_h = s_target_h; return true; }
  schedule(ANIM_STEP_MS);
  return false;
}

static bool prv_ease_shrink_step(void) {
  int step = (s_ease_idx < EASE_LEN) ? EASE[EASE_LEN - 1 - s_ease_idx] : EASE[0];
  s_ease_idx++;
  s_h -= step;
  if (s_h <= s_h_min) { s_h = s_h_min; return true; }
  return false;
}

static void prv_start_anticipate(void) { s_phase = PHASE_ANTICIPATE; schedule(ANTICIPATION_MS); }
static void prv_start_expand(void) { s_phase = PHASE_EXPAND; s_ease_idx = 0; s_overshot = false; schedule(ANIM_STEP_MS); }
static void prv_start_blink(void) {
  s_h = s_target_h; s_going_down = true; s_anim_rep = 0; s_overshot = false;
  s_phase = PHASE_BLINK; layer_mark_dirty(s_canvas_layer); schedule(ANIM_STEP_MS);
}

// Advance to the next layout in the forward cycle.
// If phase is busy, ignore — prevents input stacking during animation.
static void prv_advance_layout(void) {
  if (s_phase != PHASE_DONE) return;
  s_layout = prv_next_layout(s_layout);
  prv_update_targets();
  if (s_layout == LAYOUT_FULL) {
    // Wrapping back to full: use shake animation for satisfying feedback
    s_phase = PHASE_SHAKE_CYCLE; s_anim_step = 0;
    s_h = s_target_h; schedule(ANIM_STEP_MS);
  } else {
    s_h_min = H_MIN; prv_start_anticipate();
  }
  layer_mark_dirty(s_canvas_layer);
}

static void timer_cb(void *data) {
  s_timer = NULL;
  switch (s_phase) {

    case PHASE_ANTICIPATE:
      s_h += ANTICIPATION_PX; layer_mark_dirty(s_canvas_layer);
      s_phase = PHASE_SQUISH; s_ease_idx = 0; s_h_min = H_MIN; schedule(ANIM_STEP_MS);
      break;

    case PHASE_COUNTDOWN:
      switch (s_cd_sub) {
        case CD_SHRINK:
          s_h -= 6; layer_mark_dirty(s_canvas_layer);
          if (s_h <= H_MIN) { s_h = H_MIN; s_cd_sub = CD_HOLD_MIN; schedule(ANIM_SNAP_MS); }
          else schedule(ANIM_STEP_MS); break;
        case CD_HOLD_MIN:
          if (s_countdown_digit == 0) { prv_start_blink(); break; }
          s_countdown_digit--; layer_mark_dirty(s_canvas_layer);
          s_cd_sub = CD_EXPAND; s_ease_idx = 0; s_overshot = false; schedule(ANIM_STEP_MS); break;
        case CD_EXPAND: {
          bool done = prv_ease_expand_step(); layer_mark_dirty(s_canvas_layer);
          if (done) { s_cd_sub = CD_HOLD_MAX; schedule(COUNTDOWN_HOLD_MS); } break; }
        case CD_HOLD_MAX: s_cd_sub = CD_SHRINK; schedule(ANIM_STEP_MS); break;
      }
      break;

    case PHASE_BLINK:
      if (s_going_down) {
        s_h -= 6; layer_mark_dirty(s_canvas_layer);
        if (s_h <= H_MIN) { s_h = H_MIN; s_going_down = false; s_overshot = false; }
        schedule(ANIM_STEP_MS);
      } else {
        bool done = prv_ease_expand_step(); layer_mark_dirty(s_canvas_layer);
        if (done) {
          if (++s_anim_rep < BLINK_REPS) { s_going_down = true; s_overshot = false; s_ease_idx = 0; schedule(ANIM_STEP_MS); }
          else { s_phase = PHASE_DONE; layer_mark_dirty(s_canvas_layer); }
        }
      }
      break;

    case PHASE_SQUISH: {
      bool done = prv_ease_shrink_step(); layer_mark_dirty(s_canvas_layer);
      if (done) {
        if (s_digit_pending) { s_hour = s_pending_hour; s_minute = s_pending_minute; s_digit_pending = false; }
        prv_start_expand();
      } else schedule(ANIM_STEP_MS);
      break;
    }

    case PHASE_EXPAND: {
      bool done = prv_ease_expand_step(); layer_mark_dirty(s_canvas_layer);
      if (done) { s_phase = PHASE_DONE; layer_mark_dirty(s_canvas_layer); }
      break;
    }

    case PHASE_SHAKE_CYCLE: {
      static const int OFF[SHAKE_LEN] = { 0,-UNIT,-(UNIT*2),-(UNIT*3),-(UNIT*2),-UNIT,0 };
      if (++s_anim_step < SHAKE_LEN) {
        int h = s_target_h + OFF[s_anim_step];
        s_h = h < H_ABSOLUTE_MIN ? H_ABSOLUTE_MIN : h;
        layer_mark_dirty(s_canvas_layer); schedule(ANIM_STEP_MS);
      } else {
        s_h = s_target_h; s_phase = PHASE_DONE; layer_mark_dirty(s_canvas_layer);
      }
      break;
    }

    case PHASE_DONE: break;
  }
}

static void unobstructed_change(AnimationProgress progress, void *ctx) {
  prv_update_targets();
  if (s_phase == PHASE_DONE) { s_h = s_target_h; layer_mark_dirty(s_canvas_layer); }
}

// Shake: always return to LAYOUT_FULL.
// Shake-in-place when already FULL gives pulse feedback.
static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_phase != PHASE_DONE) return;
  s_layout = LAYOUT_FULL;
  prv_update_targets();
  s_phase = PHASE_SHAKE_CYCLE; s_anim_step = 0;
  s_h = s_target_h; schedule(ANIM_STEP_MS);
  layer_mark_dirty(s_canvas_layer);
}

// Touch: advance to next layout in forward cycle.
// API: touch_service_subscribe(handler, context)
//      handler = void(*)(const TouchEvent *, void *)
// TouchEvent fields (type enum, x, y) unknown — any touch advances.
// TODO: filter to touch-down type only once enum name is confirmed.
#if defined(PBL_TOUCH)
static void touch_handler(const TouchEvent *event, void *context) {
  (void)event; (void)context;
  prv_advance_layout();
}
#endif

static void tick_handler(struct tm *t, TimeUnits units) {
  bool animated = (s_layout == LAYOUT_FULL || s_layout == LAYOUT_INFO);
  if (s_phase == PHASE_DONE && animated) {
    s_pending_hour = t->tm_hour; s_pending_minute = t->tm_min; s_digit_pending = true;
    s_h_min = H_MIN; prv_start_anticipate();
  } else if (s_phase == PHASE_SQUISH) {
    s_pending_hour = t->tm_hour; s_pending_minute = t->tm_min; s_digit_pending = true;
  } else {
    s_hour = t->tm_hour; s_minute = t->tm_min; layer_mark_dirty(s_canvas_layer);
  }
#if defined(PBL_HEALTH)
  prv_update_health();
#endif
}

static void battery_handler(BatteryChargeState state) {
  s_battery_pct = state.charge_percent; s_charging = state.is_charging;
  layer_mark_dirty(s_canvas_layer);
}
static void bt_handler(bool connected) {
  s_bt_connected = connected; layer_mark_dirty(s_canvas_layer);
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;
  t = dict_find(iter, MESSAGE_KEY_WeatherTempF);
  if (t) snprintf(s_weather_temp, sizeof(s_weather_temp), "%dF", (int)t->value->int32);
  t = dict_find(iter, MESSAGE_KEY_WeatherTempC);
  if (t && !s_cfg_temp_f && !s_weather_temp[0])
    snprintf(s_weather_temp, sizeof(s_weather_temp), "%dC", (int)t->value->int32);
  t = dict_find(iter, MESSAGE_KEY_WeatherCode);
  if (t) {
    int c = (int)t->value->int32;
    const char *d = (c==0)?"clear":(c<=3)?"cloudy":(c<=49)?"fog":(c<=69)?"rain":(c<=79)?"snow":(c<=99)?"storm":"weather";
    snprintf(s_weather_desc, sizeof(s_weather_desc), "%s", d);
  }
  t = dict_find(iter, MESSAGE_KEY_SunriseTime);
  if (t) { time_t ts = (time_t)(uint32_t)t->value->uint32; struct tm *lt = localtime(&ts); if (lt) s_sunrise_min = lt->tm_hour * 60 + lt->tm_min; }
  t = dict_find(iter, MESSAGE_KEY_SunsetTime);
  if (t) { time_t ts = (time_t)(uint32_t)t->value->uint32; struct tm *lt = localtime(&ts); if (lt) s_sunset_min = lt->tm_hour * 60 + lt->tm_min; }
  t = dict_find(iter, MESSAGE_KEY_CfgTimePos);
  if (t) s_cfg_time_pos = (int)t->value->int32;
  for (int i = 0; i < NUM_SLOTS; i++) {
    t = dict_find(iter, MESSAGE_KEY_CfgSlot1 + i);
    if (t) s_cfg_slots[i] = (int)t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_CfgTempUnit);    if (t) s_cfg_temp_f  = (t->value->int32 == 0);
  t = dict_find(iter, MESSAGE_KEY_CfgDistUnit);    if (t) s_cfg_dist_mi = (t->value->int32 == 0);
  t = dict_find(iter, MESSAGE_KEY_CfgClockFormat); if (t) s_cfg_24h     = (t->value->int32 == 1);
  prv_save_config(); prv_update_targets(); layer_mark_dirty(s_canvas_layer);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas_layer, draw_layer);
  layer_add_child(root, s_canvas_layer);

  GRect ub = layer_get_unobstructed_bounds(root);
  s_target_h = prv_compute_target_h(ub.size.h, s_layout);
  s_h = s_target_h;

  UnobstructedAreaHandlers ua = { .change = unobstructed_change };
  unobstructed_area_service_subscribe(ua, NULL);
  accel_tap_service_subscribe(accel_tap_handler);
#if defined(PBL_TOUCH)
  touch_service_subscribe(touch_handler, NULL);
#endif

  s_phase = PHASE_DONE;  // countdown disabled; re-enable in future pass
  s_overshot = false; s_ease_idx = 0;
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
  window_set_window_handlers(s_window, (WindowHandlers){ .load = window_load, .unload = window_unload });
  window_stack_push(s_window, true);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
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
