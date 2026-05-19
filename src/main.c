#include <pebble.h>

// ============================================================
// TallBoy -- main.c  v3.39a
//
// v3.39a:
//   1. Wide info lines: bottom block now correctly anchors to
//      ub_bot (screen bottom), not relative to digit position.
//      below_glyph_end was being computed correctly but the
//      draw_info_block_up call was using the wrong anchor.
//      Added explicit debug to verify.
//
//   2. build_info_lines_wide: always generates exactly
//      lines_above + lines_below lines regardless of data
//      availability, using static debug fallbacks so 3/3
//      always shows all 6 lines.
//
//   3. Same for build_info_lines_wide bottom half — guaranteed
//      3 lines below using fallback strings.
//
// Spacing rule (wide):
//   Above block: first glyph at ub_top + UNIT (1u from top)
//   Below block: last glyph bottom at ub_bot - bot_margin
//   Both blocks anchor to screen edges, digits fill middle.
//   Gap between block and digits = UNIT (1u) each side.
// ============================================================

// #define DEBUG_TEXT_BOXES

#define LAYOUT_FULL      0
#define LAYOUT_INFO_1    1
#define LAYOUT_INFO_2    2
#define LAYOUT_INFO_3    3
#define LAYOUT_STACK_R   4
#define LAYOUT_STACK_L   5
#define LAYOUT_COUNT     6
static int s_layout = LAYOUT_FULL;

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
  #define INFO_FONT_KEY    FONT_KEY_GOTHIC_24_BOLD
  #define UNIT                8
  #define BORDER_RADIUS      32
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
  #define INFO_FONT_KEY    FONT_KEY_GOTHIC_18_BOLD
  #define UNIT                6
  #define BORDER_RADIUS      (UNIT * 2)
#endif

#define MARGIN_CANVAS   UNIT
#define MARGIN_DIGIT    UNIT
#define MARGIN_OUTER    (MARGIN_CANVAS + MARGIN_DIGIT)

#define HALF_SLOT_PAD   (UNIT / 2)
#define GLYPH_W         (UNIT * 4)
#define H_MIN           (UNIT * 11)
#define INFO_LINES_MAX  8

#define ANIM_STEP_PX     8
#define ANIM_STEP_MS    16
#define ANIM_SNAP_MS   120
#define ANIM_OVERSHOOT  UNIT

#define COUNTDOWN_HOLD_MS  150
#define BLINK_REPS           2
#define STEPS_AVG_MAX_MIN  120

#define DOT " \xc2\xb7 "

#define QUICK_LOOK_ACTIVE(ub_h) ((ub_h) < (SCREEN_H - UNIT))
#define BOTTOM_MARGIN(ub_h) (QUICK_LOOK_ACTIVE(ub_h) ? MARGIN_DIGIT : MARGIN_OUTER)

typedef enum { CD_SHRINK, CD_HOLD_MIN, CD_EXPAND, CD_HOLD_MAX } CdSubPhase;

static int prv_info_block_h(int n) {
  if (n <= 0) return 0;
  return INFO_GLYPH_H + (n - 1) * INFO_LINE_STEP;
}

static int prv_compute_target_h(int ub_h, int lines_above, int lines_below) {
  if (lines_above == 0 && lines_below == 0) return ub_h - 2 * MARGIN_OUTER;
  int reserved = 0;
  if (lines_above > 0) reserved += UNIT + prv_info_block_h(lines_above) + UNIT;
  else                 reserved += UNIT;
  if (lines_below > 0) reserved += UNIT + prv_info_block_h(lines_below) + UNIT;
  else                 reserved += UNIT;
  int h = ub_h - reserved;
  if (h < H_MIN) h = H_MIN;
  return h;
}

static int prv_layout_lines(int *above, int *below) {
  switch (s_layout) {
    case LAYOUT_INFO_1: *above = 1; *below = 1; return 2;
    case LAYOUT_INFO_2: *above = 2; *below = 2; return 4;
    case LAYOUT_INFO_3: *above = 3; *below = 3; return 6;
    default:            *above = 0; *below = 0; return 0;
  }
}

static int prv_compute_stacked_h(int ub_h) {
  int h = (ub_h - 2 * MARGIN_OUTER - UNIT) / 2;
  if (h < H_MIN) h = H_MIN;
  return h;
}

typedef enum { PHASE_COUNTDOWN, PHASE_BLINK, PHASE_DONE, PHASE_SQUISH, PHASE_SHAKE_CYCLE } Phase;

static Window    *s_window;
static Layer     *s_canvas_layer;
static int        s_hour = 0, s_minute = 0;
static int        s_h = 0, s_target_h = 0;
static GColor     s_fg, s_bg;
static Phase      s_phase = PHASE_COUNTDOWN;
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
static char       s_weather_temp[8] = "", s_weather_desc[32] = "";

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
  #define top_cy (cy - (h - ro*2) / 2)
  #define bot_cy (cy + (h - ro*2) / 2)
  #define VBAR(x,y0,y1) if((y1)>(y0)) graphics_fill_rect(ctx,GRect((x),(y0),sw,(y1)-(y0)),0,GCornerNone)
  #define HBAR(y) graphics_fill_rect(ctx,GRect(gx,(y),GLYPH_W,sw),0,GCornerNone)
  #define NUB(x,y) graphics_fill_rect(ctx,GRect((x),(y),sw,sw),0,GCornerNone)
  switch (digit) {
    case 0: fill_arc(ctx,cap_cx,top_cy,ro,ri,270,450); fill_arc(ctx,cap_cx,bot_cy,ro,ri,90,270); VBAR(gx,top_cy,bot_cy); VBAR(gx_r,top_cy,bot_cy); break;
    case 1: { HBAR(bot_y-sw); int sx=gx+GLYPH_W/2-sw/2; VBAR(sx,top_y,bot_y-sw); int cr=sx+sw,dh2=cr-gx; if(dh2>0){GPoint p[5]={{cr-1,top_y},{cr-1,top_y+sw-HALF_UNIT},{gx-1,top_y+sw+dh2-HALF_UNIT},{gx-1,top_y+dh2-sw},{sx-1,top_y}}; GPathInfo pi={5,p}; GPath*pa=gpath_create(&pi); gpath_draw_filled(ctx,pa); gpath_destroy(pa);} break; }
    case 2: { fill_arc(ctx,cap_cx,top_cy,ro,ri,270,450); VBAR(gx,top_cy,top_cy+tail); VBAR(gx_r,top_cy,top_cy+tail); int dy=(bot_y-sw)-(top_cy+tail); if(dy>0){GPoint p[4]={{gx_r-1,top_cy+tail},{gx_r+sw,top_cy+tail},{gx-1+sw+1,bot_y-sw},{gx-1,bot_y-sw}}; GPathInfo pi={4,p}; GPath*pa=gpath_create(&pi); gpath_draw_filled(ctx,pa); gpath_destroy(pa);} HBAR(bot_y-sw); break; }
    case 3: { fill_arc(ctx,cap_cx,t_tc,ro,ri,270,450); VBAR(gx,t_tc,t_tc+tail); VBAR(gx_r,t_tc,t_bc); fill_arc(ctx,cap_cx,t_bc,ro,ri,90,180); NUB(gx+sw,t_bc+sw); fill_arc(ctx,cap_cx,b_tc,ro,ri,360,450); VBAR(gx,b_bc-tail,b_bc); VBAR(gx_r,b_tc,b_bc); fill_arc(ctx,cap_cx,b_bc,ro,ri,90,270); break; }
    case 4: VBAR(gx,top_y,cy-HALF_UNIT+sw); VBAR(gx_r,top_y,bot_y); graphics_fill_rect(ctx,GRect(gx,cy-HALF_UNIT,GLYPH_W,sw),0,GCornerNone); break;
    case 5: HBAR(top_y); VBAR(gx,top_y+sw,b_tc); fill_arc(ctx,cap_cx,b_tc,ro,ri,270,450); fill_arc(ctx,cap_cx,b_bc,ro,ri,90,270); VBAR(gx_r,b_tc,b_bc); if(tail>0)VBAR(gx,b_bc-tail,b_bc); break;
    case 6: fill_arc(ctx,cap_cx,top_cy,ro,ri,270,450); VBAR(gx_r,top_cy,top_cy+tail); VBAR(gx,top_cy,b_bc); fill_arc(ctx,cap_cx,b_tc,ro,ri,270,450); fill_arc(ctx,cap_cx,b_bc,ro,ri,90,270); VBAR(gx_r,b_tc,b_bc); break;
    case 7: { HBAR(top_y); GPoint p[4]={{gx_r-1,top_y+sw},{gx_r+sw,top_y+sw},{gx+sw,bot_y-1},{gx-1,bot_y-1}}; GPathInfo pi={4,p}; GPath*pa=gpath_create(&pi); gpath_draw_filled(ctx,pa); gpath_destroy(pa); break; }
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
  graphics_fill_radial(ctx, GRect(dx-r, cy-h/4-r+2, r*2, r*2), GOvalScaleModeFitCircle, (uint16_t)r, 0, DEG_TO_TRIGANGLE(360));
  graphics_fill_radial(ctx, GRect(dx-r, cy+h/4-r-2, r*2, r*2), GOvalScaleModeFitCircle, (uint16_t)r, 0, DEG_TO_TRIGANGLE(360));
}

static void draw_digits_vec(GContext *ctx, int h_tens, int h_ones, int m_tens, int m_ones, int h, int cy) {
  draw_digit_vec(ctx, h_tens, SLOT_H_TENS, cy, h);
  draw_digit_vec(ctx, h_ones, SLOT_H_ONES, cy, h);
  draw_colon_vec(ctx, COLON_SLOT_X, cy, h);
  draw_digit_vec(ctx, m_tens, SLOT_M_TENS, cy, h);
  draw_digit_vec(ctx, m_ones, SLOT_M_ONES, cy, h);
}

static void draw_stacked_vec(GContext *ctx, int h_tens, int h_ones, int m_tens, int m_ones,
                              int dh, int tens_x, int ones_x, int h_cy, int m_cy) {
  draw_digit_vec(ctx, h_tens, tens_x, h_cy, dh);
  draw_digit_vec(ctx, h_ones, ones_x, h_cy, dh);
  draw_digit_vec(ctx, m_tens, tens_x, m_cy, dh);
  draw_digit_vec(ctx, m_ones, ones_x, m_cy, dh);
}

static const char *s_day_names_full[] = { "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday" };
static const char *s_month_names[]    = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };

static void prv_fmt_distance(char *buf, int len) {
  if (strcmp(i18n_get_system_locale(), "en_US") == 0) { int mx=(s_distance_m*10)/1609; snprintf(buf,len,"%d.%d mi",mx/10,mx%10); }
  else { int kx=(s_distance_m*10)/1000; snprintf(buf,len,"%d.%d km",kx/10,kx%10); }
}
static void prv_fmt_steps_short(char *buf, int len, int steps) {
  if (steps >= 10000) snprintf(buf,len,"%d,%03d",steps/1000,steps%1000);
  else if (steps >= 1000) snprintf(buf,len,"%d.%dk",steps/1000,(steps%1000)/100);
  else snprintf(buf,len,"%d",steps);
}
static void prv_fmt_steps_long(char *buf, int len, int steps) {
  if (steps >= 1000) snprintf(buf,len,"%d,%03d",steps/1000,steps%1000);
  else snprintf(buf,len,"%d",steps);
}
static void prv_fmt_time_min(char *buf, int len, int total_min) {
  if (total_min < 0) { snprintf(buf,len,"--"); return; }
  int h=(total_min/60)%12; if(!h)h=12;
  snprintf(buf,len,"%d:%02d%s",h,total_min%60,total_min<720?"am":"pm");
}
static void prv_fmt_bat_bt(char *buf, int len) {
  if (!s_bt_connected) snprintf(buf,len,"no phone");
  else if (s_charging) snprintf(buf,len,"%d%% bat +",s_battery_pct);
  else snprintf(buf,len,"%d%% bat",s_battery_pct);
}

// Wide info lines — always fills exactly max_lines slots.
// Uses real data when available, static fallback strings otherwise,
// so all requested lines are always visible for layout debugging.
static int build_info_lines_wide(char lines[][32], int max_lines, struct tm *t) {
  int n = 0;

  // --- ABOVE block (first max_lines/2 lines) ---

  // Line A1: day & date — always available
  if (n < max_lines && t)
    snprintf(lines[n++], 32, "%s, %s %d",
             s_day_names_full[t->tm_wday], s_month_names[t->tm_mon], t->tm_mday);
  else if (n < max_lines)
    snprintf(lines[n++], 32, "Mon, Jan 1");

  // Line A2: weather
  if (n < max_lines) {
    if (s_weather_temp[0] && s_weather_desc[0])
      snprintf(lines[n++], 32, "%s & %s", s_weather_temp, s_weather_desc);
    else if (s_weather_temp[0])
      snprintf(lines[n++], 32, "%s", s_weather_temp);
    else
      snprintf(lines[n++], 32, "72 F & sunny");  // debug fallback
  }

  // Line A3: sunrise/sunset
  if (n < max_lines) {
    if (s_sunrise_min >= 0 || s_sunset_min >= 0) {
      char rise[12], set[12];
      prv_fmt_time_min(rise, sizeof(rise), s_sunrise_min);
      prv_fmt_time_min(set,  sizeof(set),  s_sunset_min);
      snprintf(lines[n++], 32, "%s" DOT "%s", rise, set);
    } else {
      snprintf(lines[n++], 32, "6:02am" DOT "8:14pm");  // debug fallback
    }
  }

  // --- BELOW block (remaining lines) ---

  // Line B1: steps & distance
  if (n < max_lines) {
#if defined(PBL_HEALTH)
    char sbuf[16]; prv_fmt_steps_long(sbuf, sizeof(sbuf), s_steps);
    if (s_distance_m > 0) {
      char dbuf[16]; prv_fmt_distance(dbuf, sizeof(dbuf));
      snprintf(lines[n++], 32, "%s steps" DOT "%s", sbuf, dbuf);
    } else snprintf(lines[n++], 32, "%s steps", sbuf);
#else
    snprintf(lines[n++], 32, "3,500 steps" DOT "2.1 mi");
#endif
  }

  // Line B2: expected steps & %
  if (n < max_lines) {
#if defined(PBL_HEALTH)
    if (s_steps_expected > 0) {
      char ebuf[16]; prv_fmt_steps_long(ebuf, sizeof(ebuf), s_steps_expected);
      snprintf(lines[n++], 32, "exp %s" DOT "%d%%", ebuf, (s_steps * 100) / s_steps_expected);
    } else snprintf(lines[n++], 32, "exp --" DOT "--%");
#else
    snprintf(lines[n++], 32, "exp 3,500" DOT "100%%");
#endif
  }

  // Line B3: heart rate & calories (always shows something)
  if (n < max_lines) {
#if defined(PBL_HEALTH)
    bool has_hr = s_heart_rate > 0, has_cal = s_calories > 0;
    if (has_hr && has_cal)
      snprintf(lines[n++], 32, "%d bpm" DOT "%d cal", s_heart_rate, s_calories);
    else if (has_hr)
      snprintf(lines[n++], 32, "%d bpm", s_heart_rate);
    else if (has_cal)
      snprintf(lines[n++], 32, "%d cal", s_calories);
    else
      snprintf(lines[n++], 32, "-- bpm" DOT "-- cal");  // debug fallback
#else
    snprintf(lines[n++], 32, "72 bpm" DOT "212 cal");
#endif
  }

  // Line B4+: battery (if more slots requested)
  if (n < max_lines) {
    char bbuf[24]; prv_fmt_bat_bt(bbuf, sizeof(bbuf));
    snprintf(lines[n++], 32, "%s", bbuf);
  }

  return n;
}

static int build_info_lines_stacked(char lines[][32], int max_lines, struct tm *t) {
  int n = 0;
  if (n < max_lines && s_weather_temp[0]) snprintf(lines[n++], 32, "%s", s_weather_temp);
  else if (n < max_lines) snprintf(lines[n++], 32, "72 F");
  if (n < max_lines && t) snprintf(lines[n++], 32, "%s", s_day_names_full[t->tm_wday]);
  if (n < max_lines && t) snprintf(lines[n++], 32, "%s %d", s_month_names[t->tm_mon], t->tm_mday);
#if defined(PBL_HEALTH)
  if (n < max_lines) { char sbuf[16]; prv_fmt_steps_short(sbuf, sizeof(sbuf), s_steps); snprintf(lines[n++], 32, "%s steps", sbuf); }
  if (n < max_lines && s_steps_expected > 0) { char ebuf[16]; prv_fmt_steps_long(ebuf, sizeof(ebuf), s_steps_expected); snprintf(lines[n++], 32, "exp %s", ebuf); }
  if (n < max_lines && s_steps_expected > 0) snprintf(lines[n++], 32, "%d%% exp", (s_steps * 100) / s_steps_expected);
  if (n < max_lines && s_steps_expected > 0) {
    int diff = s_steps - s_steps_expected;
    if (diff == 0) snprintf(lines[n++], 32, "on pace");
    else { char dbuf[16]; prv_fmt_steps_long(dbuf, sizeof(dbuf), diff < 0 ? -diff : diff); snprintf(lines[n++], 32, "%s%s steps", diff > 0 ? "+" : "-", dbuf); }
  }
  if (n < max_lines && s_distance_m > 0) prv_fmt_distance(lines[n++], 32);
#endif
  if (n < max_lines) prv_fmt_bat_bt(lines[n++], 32);
  return n;
}

static GFont prv_info_font(void) { return fonts_get_system_font(INFO_FONT_KEY); }

static void draw_text_at_glyph_y(GContext *ctx, const char *text, int x, int glyph_y, int width) {
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
  char lines[INFO_LINES_MAX][32];
  int n = build_info_lines_stacked(lines, INFO_LINES_MAX, t);
  if (!n) return;
  int glyph_top = area.origin.y + MARGIN_OUTER - HALF_UNIT;
  int glyph_bot = area.origin.y + area.size.h - MARGIN_OUTER + INFO_GLYPH_H;
  int slot_h    = (glyph_bot - glyph_top) / n;
  for (int i = 0; i < n; i++)
    draw_text_at_glyph_y(ctx, lines[i], area.origin.x,
                         glyph_top + i * slot_h, area.size.w);
}

// Wide blocks: both anchor to screen edges.
// above: line 0 glyph at glyph_y_start, each subsequent +INFO_LINE_STEP down
// below: line n-1 glyph bottom at glyph_bot, each preceding line -INFO_LINE_STEP up
static void draw_info_block_down(GContext *ctx, char lines[][32], int n,
                                 int glyph_y_start, int width, int x) {
  for (int i = 0; i < n; i++)
    draw_text_at_glyph_y(ctx, lines[i], x, glyph_y_start + i * INFO_LINE_STEP, width);
}

static void draw_info_block_up(GContext *ctx, char lines[][32],
                               int first_idx, int n, int glyph_bot, int width, int x) {
  if (n <= 0) return;
  // last line: glyph bottom = glyph_bot, so glyph top = glyph_bot - INFO_GLYPH_H
  // each preceding line is INFO_LINE_STEP higher
  for (int i = 0; i < n; i++) {
    int glyph_y = glyph_bot - INFO_GLYPH_H - (n - 1 - i) * INFO_LINE_STEP;
    draw_text_at_glyph_y(ctx, lines[first_idx + i], x, glyph_y, width);
  }
}

static void draw_layer(Layer *layer, GContext *ctx) {
  Layer *root  = window_get_root_layer(s_window);
  GRect ub     = layer_get_unobstructed_bounds(root);
  int ub_top   = ub.origin.y, ub_h = ub.size.h;
  int ub_bot   = ub_top + ub_h;
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
  graphics_fill_rect(ctx, bounds, BORDER_RADIUS, GCornersAll);
#else
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
#endif

  int hr = s_hour % 12; if (!hr) hr = 12;
  int h_tens = hr/10, h_ones = hr%10, m_tens = s_minute/10, m_ones = s_minute%10;
  int bot_margin = BOTTOM_MARGIN(ub_h);

  if (s_phase == PHASE_COUNTDOWN) {
    draw_digits_vec(ctx, s_countdown_digit, s_countdown_digit,
                    s_countdown_digit, s_countdown_digit, s_h, ub_top + ub_h / 2);
    return;
  }

  time_t now_t = time(NULL);
  struct tm *tm_now = (s_phase == PHASE_DONE) ? localtime(&now_t) : NULL;

  if (s_layout == LAYOUT_FULL) {
    draw_digits_vec(ctx, h_tens, h_ones, m_tens, m_ones, s_h, ub_top + ub_h / 2);

  } else if (s_layout == LAYOUT_INFO_1 || s_layout == LAYOUT_INFO_2 || s_layout == LAYOUT_INFO_3) {
    int lines_above, lines_below;
    prv_layout_lines(&lines_above, &lines_below);

    // Above block: first glyph at ub_top + UNIT (1u from top edge)
    int above_glyph_start = ub_top + UNIT;
    int above_glyph_end   = above_glyph_start + prv_info_block_h(lines_above);

    // Below block: last glyph bottom at ub_bot - bot_margin
    // Anchor is the screen bottom edge, independent of digit position.
    int below_glyph_end   = ub_bot - bot_margin;
    int below_glyph_start = below_glyph_end - prv_info_block_h(lines_below);

    // Time fills between the two blocks with 1u gap each side
    int time_top = above_glyph_end + UNIT;
    int time_bot = below_glyph_start - UNIT;
    int time_cy  = (time_top + time_bot) / 2;

    char all_lines[INFO_LINES_MAX][32];
    // Always request full line count — build function guarantees all slots filled
    int total = build_info_lines_wide(all_lines, lines_above + lines_below,
                                      tm_now ? tm_now : NULL);

    int above_n = lines_above < total ? lines_above : total;
    draw_info_block_down(ctx, all_lines, above_n,
                         above_glyph_start, SCREEN_W - 2*SIDE_MARGIN, SIDE_MARGIN);

    int below_n = total - lines_above;
    if (below_n > lines_below) below_n = lines_below;
    if (below_n > 0)
      draw_info_block_up(ctx, all_lines, lines_above, below_n,
                         below_glyph_end, SCREEN_W - 2*SIDE_MARGIN, SIDE_MARGIN);

    draw_digits_vec(ctx, h_tens, h_ones, m_tens, m_ones, s_h, time_cy);

  } else {
    int dh    = prv_compute_stacked_h(ub_h);
    int h_cy  = ub_top + MARGIN_OUTER + dh / 2;
    int m_cy  = ub_bot - bot_margin - dh / 2;
    int tens_x, ones_x, info_x, info_w;

    if (s_layout == LAYOUT_STACK_R) {
      ones_x = SCREEN_W - SIDE_MARGIN - SLOT_W;
      tens_x = ones_x - SLOT_W;
      info_x = SIDE_MARGIN; info_w = tens_x - SIDE_MARGIN * 2;
    } else {
      tens_x = SIDE_MARGIN; ones_x = SIDE_MARGIN + SLOT_W;
      info_x = ones_x + SLOT_W + SIDE_MARGIN;
      info_w = SCREEN_W - info_x - SIDE_MARGIN;
    }

    if (tm_now && info_w > 20)
      draw_info_column(ctx, GRect(info_x, ub_top, info_w, ub_h), tm_now);
    draw_stacked_vec(ctx, h_tens, h_ones, m_tens, m_ones, dh,
                     tens_x, ones_x, h_cy, m_cy);
  }
}

static void timer_cb(void *data);
static void schedule(uint32_t ms) {
  if (s_timer) app_timer_cancel(s_timer);
  s_timer = app_timer_register(ms, timer_cb, NULL);
}

static void prv_start_blink(void) {
  s_h = s_target_h; s_going_down = true; s_anim_rep = 0; s_overshot = false;
  s_phase = PHASE_BLINK; layer_mark_dirty(s_canvas_layer); schedule(ANIM_STEP_MS);
}

static bool prv_expand_step(void) {
  if (s_overshot) { s_h = s_target_h; s_overshot = false; layer_mark_dirty(s_canvas_layer); return true; }
  s_h += ANIM_STEP_PX; layer_mark_dirty(s_canvas_layer);
  int peak = s_target_h + ANIM_OVERSHOOT;
  if (s_h >= peak) { s_h = peak; s_overshot = true; schedule(ANIM_SNAP_MS); return false; }
  schedule(ANIM_STEP_MS); return false;
}

static void timer_cb(void *data) {
  s_timer = NULL;
  switch (s_phase) {
    case PHASE_COUNTDOWN:
      switch (s_cd_sub) {
        case CD_SHRINK: s_h -= ANIM_STEP_PX; layer_mark_dirty(s_canvas_layer); if(s_h<=H_MIN){s_h=H_MIN;s_cd_sub=CD_HOLD_MIN;schedule(ANIM_SNAP_MS);}else schedule(ANIM_STEP_MS); break;
        case CD_HOLD_MIN: if(s_countdown_digit==0){prv_start_blink();break;} s_countdown_digit--; layer_mark_dirty(s_canvas_layer); s_cd_sub=CD_EXPAND; s_overshot=false; schedule(ANIM_STEP_MS); break;
        case CD_EXPAND: if(s_overshot){s_h=s_target_h;s_overshot=false;layer_mark_dirty(s_canvas_layer);s_cd_sub=CD_HOLD_MAX;schedule(COUNTDOWN_HOLD_MS);}else{s_h+=ANIM_STEP_PX;layer_mark_dirty(s_canvas_layer);int pk=s_target_h+ANIM_OVERSHOOT;if(s_h>=pk){s_h=pk;s_overshot=true;schedule(ANIM_SNAP_MS);}else schedule(ANIM_STEP_MS);} break;
        case CD_HOLD_MAX: s_cd_sub=CD_SHRINK; schedule(ANIM_STEP_MS); break;
      }
      break;
    case PHASE_BLINK:
      if (s_going_down) { s_h-=ANIM_STEP_PX; layer_mark_dirty(s_canvas_layer); if(s_h<=H_MIN){s_h=H_MIN;s_going_down=false;s_overshot=false;} schedule(ANIM_STEP_MS); }
      else { bool done=prv_expand_step(); if(done){if(++s_anim_rep<BLINK_REPS){s_going_down=true;s_overshot=false;schedule(ANIM_STEP_MS);}else{s_phase=PHASE_DONE;layer_mark_dirty(s_canvas_layer);}}}
      break;
    case PHASE_SQUISH:
      if (s_going_down) { s_h-=ANIM_STEP_PX; layer_mark_dirty(s_canvas_layer); if(s_h<=H_MIN){s_h=H_MIN;if(s_digit_pending){s_hour=s_pending_hour;s_minute=s_pending_minute;s_digit_pending=false;}s_going_down=false;s_overshot=false;} schedule(ANIM_STEP_MS); }
      else { bool done=prv_expand_step(); if(done){s_phase=PHASE_DONE;layer_mark_dirty(s_canvas_layer);} }
      break;
    case PHASE_SHAKE_CYCLE: {
      static const int OFF[]={0,-UNIT,-(UNIT*2),-(UNIT*3),-(UNIT*2),-UNIT,0,UNIT,0};
      #define SHAKE_LEN 9
      if(++s_anim_step<SHAKE_LEN){int h=s_target_h+OFF[s_anim_step];if(h<H_MIN)h=H_MIN;s_h=h;layer_mark_dirty(s_canvas_layer);schedule(s_anim_step==SHAKE_LEN-2?ANIM_SNAP_MS:ANIM_STEP_MS);}
      else if(++s_anim_rep<2){s_anim_step=0;s_h=s_target_h;layer_mark_dirty(s_canvas_layer);schedule(ANIM_STEP_MS);}
      else{s_phase=PHASE_DONE;s_h=s_target_h;layer_mark_dirty(s_canvas_layer);}
      break;
    }
    case PHASE_DONE: break;
  }
}

static void prv_update_targets(void) {
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  int above, below; prv_layout_lines(&above, &below);
  s_target_h = prv_compute_target_h(ub.size.h, above, below);
}

static void unobstructed_change(AnimationProgress progress, void *ctx) {
  prv_update_targets();
  if (s_phase == PHASE_DONE) { s_h = s_target_h; layer_mark_dirty(s_canvas_layer); }
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_phase == PHASE_COUNTDOWN) { prv_start_blink(); return; }
  if (s_phase != PHASE_DONE) return;
  s_layout = (s_layout + 1) % LAYOUT_COUNT;
  prv_update_targets();
  if (s_layout == LAYOUT_FULL) {
    s_phase = PHASE_SHAKE_CYCLE; s_anim_step = 0; s_anim_rep = 0;
    s_h = s_target_h; schedule(ANIM_STEP_MS);
  } else {
    s_phase = PHASE_SQUISH; s_going_down = true; s_overshot = false; schedule(ANIM_STEP_MS);
  }
  layer_mark_dirty(s_canvas_layer);
}

static void tick_handler(struct tm *t, TimeUnits units) {
  bool anim = (s_layout==LAYOUT_FULL||s_layout==LAYOUT_INFO_1||s_layout==LAYOUT_INFO_2||s_layout==LAYOUT_INFO_3);
  if (s_phase == PHASE_DONE && anim) {
    s_pending_hour=t->tm_hour; s_pending_minute=t->tm_min; s_digit_pending=true;
    s_phase=PHASE_SQUISH; s_going_down=true; s_overshot=false; schedule(ANIM_STEP_MS);
  } else if (s_phase == PHASE_SQUISH) {
    s_pending_hour=t->tm_hour; s_pending_minute=t->tm_min; s_digit_pending=true;
  } else {
    s_hour=t->tm_hour; s_minute=t->tm_min; layer_mark_dirty(s_canvas_layer);
  }
#if defined(PBL_HEALTH)
  prv_update_health();
#endif
}

static void battery_handler(BatteryChargeState state) { s_battery_pct=state.charge_percent; s_charging=state.is_charging; layer_mark_dirty(s_canvas_layer); }
static void bt_handler(bool connected) { s_bt_connected=connected; layer_mark_dirty(s_canvas_layer); }

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;
  t = dict_find(iter, MESSAGE_KEY_WeatherTempF);
  if (t) snprintf(s_weather_temp, sizeof(s_weather_temp), "%dF", (int)t->value->int32);
  t = dict_find(iter, MESSAGE_KEY_WeatherTempC);
  if (t && !s_weather_temp[0]) snprintf(s_weather_temp, sizeof(s_weather_temp), "%dC", (int)t->value->int32);
  t = dict_find(iter, MESSAGE_KEY_WeatherCode);
  if (t) {
    int c=(int)t->value->int32;
    const char *d=c==0?"clear":c<=3?"cloudy":c<=49?"fog":c<=69?"rain":c<=79?"snow":c<=99?"storm":"weather";
    snprintf(s_weather_desc, sizeof(s_weather_desc), "%s", d);
  }
  layer_mark_dirty(s_canvas_layer);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas_layer, draw_layer);
  layer_add_child(root, s_canvas_layer);
  GRect ub = layer_get_unobstructed_bounds(root);
  s_target_h = prv_compute_target_h(ub.size.h, 0, 0);
  s_h = s_target_h;
  UnobstructedAreaHandlers ua = {.change = unobstructed_change};
  unobstructed_area_service_subscribe(ua, NULL);
  accel_tap_service_subscribe(accel_tap_handler);
  s_phase = PHASE_COUNTDOWN; s_countdown_digit = 9; s_overshot = false; s_cd_sub = CD_HOLD_MAX;
  layer_mark_dirty(s_canvas_layer); schedule(COUNTDOWN_HOLD_MS);
}

static void window_unload(Window *window) {
  unobstructed_area_service_unsubscribe(); accel_tap_service_unsubscribe();
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  layer_destroy(s_canvas_layer);
}

static void init(void) {
  s_fg = GColorWhite; s_bg = GColorBlack;
  s_window = window_create(); window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){.load=window_load,.unload=window_unload});
  window_stack_push(s_window, true);
  time_t now = time(NULL); struct tm *t = localtime(&now);
  s_hour = t->tm_hour; s_minute = t->tm_min;
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler); battery_handler(battery_state_service_peek());
  bluetooth_connection_service_subscribe(bt_handler); s_bt_connected = bluetooth_connection_service_peek();
#if defined(PBL_HEALTH)
  prv_update_health();
#endif
  app_message_register_inbox_received(inbox_received); app_message_open(256, 64);
}

static void deinit(void) {
  tick_timer_service_unsubscribe(); battery_state_service_unsubscribe(); bluetooth_connection_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) { init(); app_event_loop(); deinit(); }
