#include <pebble.h>

// ============================================================
// TallBoy -- main.c  v3.28
//
// v3.28: four fixes
//   1. PictonBlue (light blue) → dark fg (was missing)
//   2. Animation 8x faster: ANIM_STEP_PX=8, ANIM_STEP_MS=16
//   3. Margin corrected: 2u each side (was 1u) → target_h = ub_h - 4*UNIT
//   4. Stacked layout dynamic sizing:
//        stacked_dh = (ub_h - 5*UNIT) / 2
//        (2u top margin + 1u gap + 2u bottom margin = 5u reserved)
//        h_cy and m_cy pinned to exact 2u from canvas edges
//        Retired: pick_stack_size, stack_digit_h, STACK_H_SZ1/2, STACK_SZ2_MIN
//
// MARGIN MODEL (consistent across both layouts):
//   Full-screen vector: 2u above + 2u below digits
//   Stacked:            2u above top row + 1u gap + 2u below bottom row
//   Quick look:         same 2u maintained as ub_h shrinks
// ============================================================

#define LAYOUT_VECTOR    0
#define LAYOUT_RIGHT     1
#define LAYOUT_COUNT     2
static int s_layout = LAYOUT_VECTOR;

#if defined(PBL_PLATFORM_EMERY)
  #define SCREEN_W      200
  #define SCREEN_H      228
  #define SLOT_W         40
  #define COLON_SLOT_X   80
  #define SLOT_H_TENS    12
  #define SLOT_H_ONES    52
  #define SLOT_M_TENS   108
  #define SLOT_M_ONES   148
  #define SIDE_MARGIN    12
  #define HALF_UNIT       4
  #define INFO_FONT_H    24
  #define INFO_LINE_H    28
  #define INFO_FONT_KEY  FONT_KEY_GOTHIC_24_BOLD
  #define UNIT            8
#else
  #define SCREEN_W      144
  #define SCREEN_H      168
  #define SLOT_W         30
  #define COLON_SLOT_X   57
  #define SLOT_H_TENS     6
  #define SLOT_H_ONES    36
  #define SLOT_M_TENS    78
  #define SLOT_M_ONES   108
  #define SIDE_MARGIN     6
  #define HALF_UNIT       3
  #define INFO_FONT_H    18
  #define INFO_LINE_H    20
  #define INFO_FONT_KEY  FONT_KEY_GOTHIC_18_BOLD
  #define UNIT            6
#endif

#define HALF_SLOT_PAD   (UNIT / 2)
#define GLYPH_W         (UNIT * 4)

// Minimum digit height: arcs just touching (bar=0, tail=0) = 11u
#define H_MIN           (UNIT * 11)

// Margin constants
#define MARGIN_OUTER    (UNIT * 2)   // 2u above/below digits — both layouts
#define MARGIN_GAP      UNIT         // 1u gap between stacked rows

// Animation timing — fast but still visually smooth
#define ANIM_STEP_PX    8     // pixels per animation step
#define ANIM_STEP_MS    16    // ~60fps equivalent
#define ANIM_SNAP_MS    120   // hold at overshoot before snap
#define ANIM_OVERSHOOT  UNIT  // 1u overshoot above target

#define COUNTDOWN_HOLD_MS   400
#define BLINK_REPS          2

#define STEPS_AVG_MAX_MIN 120

typedef enum { CD_SHRINK, CD_HOLD_MIN, CD_EXPAND, CD_HOLD_MAX } CdSubPhase;

// Full-screen target height: ub_h minus 2u top + 2u bottom margin
static int prv_compute_target_h(int ub_h) {
  int h = ub_h - 2 * MARGIN_OUTER;
  if (h < H_MIN) h = H_MIN;
  return h;
}

// Stacked row height: split available space equally between two rows,
// reserving 2u top margin + 1u gap + 2u bottom margin = 5u total.
static int prv_compute_stacked_h(int ub_h) {
  int h = (ub_h - 2 * MARGIN_OUTER - MARGIN_GAP) / 2;
  if (h < H_MIN) h = H_MIN;
  return h;
}

typedef enum { PHASE_COUNTDOWN, PHASE_BLINK, PHASE_DONE, PHASE_SQUISH, PHASE_SHAKE_CYCLE } Phase;

static Window    *s_window;
static Layer     *s_canvas_layer;
static int        s_hour = 0, s_minute = 0;
static int        s_h = 0;          // current animated digit height (pixels)
static int        s_target_h = 0;   // resting full-screen height
static GColor     s_fg, s_bg;
static Phase      s_phase = PHASE_COUNTDOWN;
static int        s_anim_step = 0, s_anim_rep = 0;
static bool       s_going_down = true;
static bool       s_overshot = false;
static int        s_countdown_digit = 9;
static CdSubPhase s_cd_sub = CD_HOLD_MAX;
static AppTimer  *s_timer = NULL;
static bool       s_digit_pending = false;
static int        s_pending_hour = 0, s_pending_minute = 0;
static int        s_battery_pct = 100;
static bool       s_charging = false, s_bt_connected = true;
static int        s_steps = 0, s_distance_m = 0;
static int        s_steps_avg = -1;
static char       s_weather_temp[8] = "", s_weather_desc[32] = "";

#if defined(PBL_COLOR)
// Step pace color spectrum — % of historical avg steps
// Light backgrounds (White, Yellow, PictonBlue, BabyBlueEyes) → black fg
static GColor prv_pace_color(int steps_today, int steps_avg) {
  if (steps_avg <= 0 || steps_today <= 0) return GColorBlack;
  int pct = (steps_today * 100) / steps_avg;
  if (pct <= 0)    return GColorBlack;
  if (pct <= 30)   return GColorRed;
  if (pct <= 60)   return GColorOrange;
  if (pct <= 90)   return GColorYellow;
  if (pct <= 200)  return GColorGreen;
  if (pct <= 300)  return GColorBlue;
  if (pct <= 400)  return GColorPictonBlue;
  if (pct <= 500)  return GColorWhite;
  if (pct <= 600)  return GColorBabyBlueEyes;
  if (pct <= 1000) return GColorPurple;
  return GColorBlack;  // 1001%+: you're unhinged
}

static bool prv_bg_needs_dark_fg(GColor bg) {
  return gcolor_equal(bg, GColorWhite)       ||
         gcolor_equal(bg, GColorYellow)      ||
         gcolor_equal(bg, GColorPictonBlue)  ||   // light blue → black fg
         gcolor_equal(bg, GColorBabyBlueEyes);
}
#endif

#if defined(PBL_HEALTH)
static HealthMinuteData s_minute_buf[STEPS_AVG_MAX_MIN];

static int prv_calc_steps_avg(void) {
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
#endif

static void fill_arc(GContext *ctx, int cx, int cy, int ro, int ri, int a0, int a1) {
  GRect b = GRect(cx - ro, cy - ro, ro * 2, ro * 2);
  graphics_fill_radial(ctx, b, GOvalScaleModeFitCircle, (uint16_t)(ro - ri),
                       DEG_TO_TRIGANGLE(a0), DEG_TO_TRIGANGLE(a1));
}

// Draw a single vector digit parameterized by pixel height h.
// No discrete size index — spans stretch continuously.
//   bar  = (h - 7*UNIT) / 2    ring gap
//   tail = max(0, (h - H_MIN) / 4)  stub extension
static void draw_digit_vec(GContext *ctx, int digit, int slot_x, int cy, int h) {
  graphics_context_set_fill_color(ctx, s_fg);

  const int ro = UNIT * 2;
  const int ri = UNIT * 1;
  const int sw = UNIT;

  int gx     = slot_x + HALF_SLOT_PAD;
  int gx_r   = gx + GLYPH_W - sw;
  int cap_cx = gx + ro;

  int top_y = cy - h / 2;
  int bot_y = cy + h / 2;

  int bar  = (h - 7 * UNIT) / 2;
  if (bar < 0) bar = 0;
  int t_bc = cy - (ro - HALF_UNIT);
  int t_tc = t_bc - bar;
  int b_tc = cy + (ro - HALF_UNIT);
  int b_bc = b_tc + bar;
  int tail = (h > H_MIN) ? (h - H_MIN) / 4 : 0;

  #define top_cy (cy - (h - ro*2) / 2)
  #define bot_cy (cy + (h - ro*2) / 2)
  #define VBAR(x,y0,y1) if((y1)>(y0)) graphics_fill_rect(ctx,GRect((x),(y0),sw,(y1)-(y0)),0,GCornerNone)
  #define HBAR(y) graphics_fill_rect(ctx,GRect(gx,(y),GLYPH_W,sw),0,GCornerNone)
  #define NUB(x,y) graphics_fill_rect(ctx,GRect((x),(y),sw,sw),0,GCornerNone)

  switch (digit) {
    case 0:
      fill_arc(ctx, cap_cx, top_cy, ro, ri, 270, 450);
      fill_arc(ctx, cap_cx, bot_cy, ro, ri, 90, 270);
      VBAR(gx,   top_cy, bot_cy);
      VBAR(gx_r, top_cy, bot_cy);
      break;
    case 1: {
      HBAR(bot_y - sw);
      int stem_x   = gx + GLYPH_W / 2 - sw / 2;
      VBAR(stem_x, top_y, bot_y - sw);
      int cap_right = stem_x + sw;
      int diag_h    = cap_right - gx;
      if (diag_h > 0) {
        GPoint pts[5] = {
          {cap_right - 1, top_y},
          {cap_right - 1, top_y + sw - HALF_UNIT},
          {gx        - 1, top_y + sw + diag_h - HALF_UNIT},
          {gx        - 1, top_y + diag_h - sw},
          {stem_x    - 1, top_y},
        };
        GPathInfo info = { .num_points = 5, .points = pts };
        GPath *path = gpath_create(&info);
        gpath_draw_filled(ctx, path);
        gpath_destroy(path);
      }
      break;
    }
    case 2: {
      fill_arc(ctx, cap_cx, top_cy, ro, ri, 270, 450);
      VBAR(gx,   top_cy, top_cy + tail);
      VBAR(gx_r, top_cy, top_cy + tail);
      int dy = (bot_y - sw) - (top_cy + tail);
      if (dy > 0) {
        GPoint pts[4] = {
          {gx_r - 1,         top_cy + tail},
          {gx_r + sw,        top_cy + tail},
          {gx  - 1 + sw + 1, bot_y - sw},
          {gx  - 1,          bot_y - sw},
        };
        GPathInfo info = { .num_points = 4, .points = pts };
        GPath *path = gpath_create(&info);
        gpath_draw_filled(ctx, path);
        gpath_destroy(path);
      }
      HBAR(bot_y - sw);
      break;
    }
    case 3: {
      fill_arc(ctx, cap_cx, t_tc, ro, ri, 270, 450);
      VBAR(gx,   t_tc, t_tc + tail);
      VBAR(gx_r, t_tc, t_bc);
      fill_arc(ctx, cap_cx, t_bc, ro, ri, 90, 180);
      NUB(gx + sw, t_bc + sw);
      fill_arc(ctx, cap_cx, b_tc, ro, ri, 360, 450);
      VBAR(gx,   b_bc - tail, b_bc);
      VBAR(gx_r, b_tc, b_bc);
      fill_arc(ctx, cap_cx, b_bc, ro, ri, 90, 270);
      break;
    }
    case 4:
      VBAR(gx,   top_y, cy - HALF_UNIT + sw);
      VBAR(gx_r, top_y, bot_y);
      graphics_fill_rect(ctx, GRect(gx, cy - HALF_UNIT, GLYPH_W, sw), 0, GCornerNone);
      break;
    case 5:
      HBAR(top_y);
      VBAR(gx,   top_y + sw, b_tc);
      fill_arc(ctx, cap_cx, b_tc, ro, ri, 270, 450);
      fill_arc(ctx, cap_cx, b_bc, ro, ri, 90, 270);
      VBAR(gx_r, b_tc, b_bc);
      if (tail > 0) VBAR(gx, b_bc - tail, b_bc);
      break;
    case 6:
      fill_arc(ctx, cap_cx, top_cy, ro, ri, 270, 450);
      VBAR(gx_r, top_cy, top_cy + tail);
      VBAR(gx,   top_cy, b_bc);
      fill_arc(ctx, cap_cx, b_tc, ro, ri, 270, 450);
      fill_arc(ctx, cap_cx, b_bc, ro, ri, 90, 270);
      VBAR(gx_r, b_tc, b_bc);
      break;
    case 7: {
      HBAR(top_y);
      GPoint pts[4] = {
        {gx_r - 1,  top_y + sw},
        {gx_r + sw, top_y + sw},
        {gx + sw,   bot_y - 1},
        {gx - 1,    bot_y - 1},
      };
      GPathInfo info = { .num_points = 4, .points = pts };
      GPath *path = gpath_create(&info);
      gpath_draw_filled(ctx, path);
      gpath_destroy(path);
      break;
    }
    case 8:
      fill_arc(ctx, cap_cx, t_tc, ro, ri, 270, 450);
      fill_arc(ctx, cap_cx, t_bc, ro, ri, 90, 270);
      VBAR(gx,   t_tc, t_bc);
      VBAR(gx_r, t_tc, t_bc);
      fill_arc(ctx, cap_cx, b_tc, ro, ri, 270, 450);
      fill_arc(ctx, cap_cx, b_bc, ro, ri, 90, 270);
      VBAR(gx,   b_tc, b_bc);
      VBAR(gx_r, b_tc, b_bc);
      break;
    case 9:
      fill_arc(ctx, cap_cx, t_tc, ro, ri, 270, 450);
      fill_arc(ctx, cap_cx, t_bc, ro, ri, 90, 270);
      VBAR(gx,   t_tc, t_bc);
      VBAR(gx,   bot_cy - tail, bot_cy);
      VBAR(gx_r, t_tc, bot_cy);
      fill_arc(ctx, cap_cx, bot_cy, ro, ri, 90, 270);
      break;
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
  GRect b1 = GRect(dx-r, cy-h/4-r+2, r*2, r*2);
  GRect b2 = GRect(dx-r, cy+h/4-r-2, r*2, r*2);
  graphics_fill_radial(ctx, b1, GOvalScaleModeFitCircle, (uint16_t)r, 0, DEG_TO_TRIGANGLE(360));
  graphics_fill_radial(ctx, b2, GOvalScaleModeFitCircle, (uint16_t)r, 0, DEG_TO_TRIGANGLE(360));
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

static const char *s_day_names_full[] = {
  "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};
static const char *s_month_abbrs[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};

static int build_info_lines(char lines[][32], int max_lines, struct tm *t) {
  int n = 0;
  if (n < max_lines && t) snprintf(lines[n++], 32, "%s", s_day_names_full[t->tm_wday]);
  if (n < max_lines && t) snprintf(lines[n++], 32, "%s %d", s_month_abbrs[t->tm_mon], t->tm_mday);
#if defined(PBL_HEALTH)
  if (n < max_lines) {
    if (s_steps >= 1000) snprintf(lines[n++], 32, "%dk steps", s_steps/1000);
    else snprintf(lines[n++], 32, "%d steps", s_steps);
  }
  if (n < max_lines && s_distance_m > 0) {
    if (strcmp(i18n_get_system_locale(),"en_US")==0) { int mx=(s_distance_m*10)/1609; snprintf(lines[n++],32,"%d.%d mi",mx/10,mx%10); }
    else { int kx=(s_distance_m*10)/1000; snprintf(lines[n++],32,"%d.%d km",kx/10,kx%10); }
  }
  if (n < max_lines && s_steps_avg > 0) snprintf(lines[n++], 32, "avg %d", s_steps_avg);
#endif
  if (n < max_lines) snprintf(lines[n++], 32, "bat %d%%", s_battery_pct);
  return n;
}

static GFont prv_info_font(void) { return fonts_get_system_font(INFO_FONT_KEY); }

static void draw_info_block(GContext *ctx, int x, int y, int width, char lines[][32], int n) {
  if (!n) return;
  GFont font = prv_info_font();
  graphics_context_set_text_color(ctx, s_fg);
  for (int i = 0; i < n; i++) {
    GRect r = GRect(x, y + i * INFO_LINE_H, width, INFO_FONT_H + 4);
    graphics_draw_text(ctx, lines[i], font, r, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void draw_info_column(GContext *ctx, GRect area, struct tm *t) {
  char lines[6][32];
  int n = build_info_lines(lines, area.size.h / INFO_LINE_H < 6 ? area.size.h / INFO_LINE_H : 6, t);
  if (!n) return;
  int start_y = area.origin.y + (area.size.h - n * INFO_LINE_H) / 2;
  draw_info_block(ctx, area.origin.x, start_y, area.size.w, lines, n);
}

static void draw_layer(Layer *layer, GContext *ctx) {
  Layer *root  = window_get_root_layer(s_window);
  GRect ub     = layer_get_unobstructed_bounds(root);
  int ub_top   = ub.origin.y, ub_h = ub.size.h;
  int center_y = ub_top + ub_h / 2;

#if defined(PBL_COLOR) && defined(PBL_HEALTH)
  GColor bg = prv_pace_color(s_steps, s_steps_avg);
  s_fg = prv_bg_needs_dark_fg(bg) ? GColorBlack : GColorWhite;
#else
  GColor bg = s_bg;
  s_fg = GColorWhite;
#endif
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  int hr = s_hour % 12; if (!hr) hr = 12;
  int h_tens = hr/10, h_ones = hr%10, m_tens = s_minute/10, m_ones = s_minute%10;

  if (s_phase == PHASE_COUNTDOWN) {
    int d = s_countdown_digit;
    draw_digits_vec(ctx, d, d, d, d, s_h, center_y);
    return;
  }

  time_t now_t = time(NULL);
  struct tm *tm = (s_phase == PHASE_DONE) ? localtime(&now_t) : NULL;

  if (s_layout == LAYOUT_VECTOR) {
    draw_digits_vec(ctx, h_tens, h_ones, m_tens, m_ones, s_h, center_y);
  } else {
    // LAYOUT_RIGHT: stacked digits pinned to exact 2u margins top & bottom,
    // 1u gap between hour and minute rows. Dynamic digit height from ub_h.
    int dh    = prv_compute_stacked_h(ub_h);
    int h_cy  = ub_top + MARGIN_OUTER + dh / 2;
    int m_cy  = ub_top + ub_h - MARGIN_OUTER - dh / 2;
    int ones_x = SCREEN_W - SIDE_MARGIN - SLOT_W;
    int tens_x = ones_x - SLOT_W;
    int info_x = SIDE_MARGIN;
    int info_w = tens_x - SIDE_MARGIN * 2;
    draw_stacked_vec(ctx, h_tens, h_ones, m_tens, m_ones, dh,
                     tens_x, ones_x, h_cy, m_cy);
    if (tm && info_w > 20) draw_info_column(ctx, GRect(info_x, ub_top, info_w, ub_h), tm);
  }
}

static void timer_cb(void *data);
static void schedule(uint32_t ms) { if (s_timer) app_timer_cancel(s_timer); s_timer = app_timer_register(ms, timer_cb, NULL); }

static void prv_start_blink(void) {
  s_h = s_target_h; s_going_down = true; s_anim_rep = 0; s_overshot = false;
  s_phase = PHASE_BLINK;
  layer_mark_dirty(s_canvas_layer); schedule(ANIM_STEP_MS);
}

static bool prv_expand_step(void) {
  if (s_overshot) {
    s_h = s_target_h; s_overshot = false;
    layer_mark_dirty(s_canvas_layer);
    return true;
  }
  s_h += ANIM_STEP_PX;
  layer_mark_dirty(s_canvas_layer);
  int peak = s_target_h + ANIM_OVERSHOOT;
  if (s_h >= peak) {
    s_h = peak; s_overshot = true;
    schedule(ANIM_SNAP_MS);
    return false;
  }
  schedule(ANIM_STEP_MS);
  return false;
}

static void timer_cb(void *data) {
  s_timer = NULL;
  switch (s_phase) {

    case PHASE_COUNTDOWN:
      switch (s_cd_sub) {
        case CD_SHRINK:
          s_h -= ANIM_STEP_PX;
          layer_mark_dirty(s_canvas_layer);
          if (s_h <= H_MIN) { s_h = H_MIN; s_cd_sub = CD_HOLD_MIN; schedule(ANIM_SNAP_MS); }
          else { schedule(ANIM_STEP_MS); }
          break;
        case CD_HOLD_MIN:
          if (s_countdown_digit == 0) { prv_start_blink(); break; }
          s_countdown_digit--;
          layer_mark_dirty(s_canvas_layer);
          s_cd_sub = CD_EXPAND; s_overshot = false;
          schedule(ANIM_STEP_MS);
          break;
        case CD_EXPAND: {
          if (s_overshot) {
            s_h = s_target_h; s_overshot = false;
            layer_mark_dirty(s_canvas_layer);
            s_cd_sub = CD_HOLD_MAX; schedule(COUNTDOWN_HOLD_MS);
          } else {
            s_h += ANIM_STEP_PX;
            layer_mark_dirty(s_canvas_layer);
            int peak = s_target_h + ANIM_OVERSHOOT;
            if (s_h >= peak) { s_h = peak; s_overshot = true; schedule(ANIM_SNAP_MS); }
            else { schedule(ANIM_STEP_MS); }
          }
          break;
        }
        case CD_HOLD_MAX:
          s_cd_sub = CD_SHRINK; schedule(ANIM_STEP_MS);
          break;
      }
      break;

    case PHASE_BLINK:
      if (s_going_down) {
        s_h -= ANIM_STEP_PX; layer_mark_dirty(s_canvas_layer);
        if (s_h <= H_MIN) { s_h = H_MIN; s_going_down = false; s_overshot = false; }
        schedule(ANIM_STEP_MS);
      } else {
        bool done = prv_expand_step();
        if (done) {
          if (++s_anim_rep < BLINK_REPS) {
            s_going_down = true; s_overshot = false; schedule(ANIM_STEP_MS);
          } else {
            s_phase = PHASE_DONE; layer_mark_dirty(s_canvas_layer);
          }
        }
      }
      break;

    case PHASE_SQUISH:
      if (s_going_down) {
        s_h -= ANIM_STEP_PX; layer_mark_dirty(s_canvas_layer);
        if (s_h <= H_MIN) {
          s_h = H_MIN;
          if (s_digit_pending) { s_hour = s_pending_hour; s_minute = s_pending_minute; s_digit_pending = false; }
          s_going_down = false; s_overshot = false;
        }
        schedule(ANIM_STEP_MS);
      } else {
        bool done = prv_expand_step();
        if (done) { s_phase = PHASE_DONE; layer_mark_dirty(s_canvas_layer); }
      }
      break;

    case PHASE_SHAKE_CYCLE: {
      // Pixel offsets from s_target_h: down 3u then back up with 1u overshoot
      static const int SHAKE_OFFSETS_PX[] = {
        0, -UNIT, -(UNIT*2), -(UNIT*3),
        -(UNIT*2), -UNIT, 0, UNIT, 0
      };
      #define SHAKE_LEN 9
      if (++s_anim_step < SHAKE_LEN) {
        int h = s_target_h + SHAKE_OFFSETS_PX[s_anim_step];
        if (h < H_MIN) h = H_MIN;
        s_h = h;
        layer_mark_dirty(s_canvas_layer);
        bool at_snap = (s_anim_step == SHAKE_LEN - 2);
        schedule(at_snap ? ANIM_SNAP_MS : ANIM_STEP_MS);
      } else if (++s_anim_rep < 2) {
        s_anim_step = 0; s_h = s_target_h;
        layer_mark_dirty(s_canvas_layer); schedule(ANIM_STEP_MS);
      } else {
        s_phase = PHASE_DONE; s_h = s_target_h; layer_mark_dirty(s_canvas_layer);
      }
      break;
    }

    case PHASE_DONE: break;
  }
}

static void prv_update_targets(void) {
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  if (s_layout == LAYOUT_VECTOR) {
    s_target_h = prv_compute_target_h(ub.size.h);
  }
}

static void unobstructed_change(AnimationProgress progress, void *ctx) {
  prv_update_targets();
  if (s_phase == PHASE_DONE && s_layout == LAYOUT_VECTOR) {
    s_h = s_target_h;
    layer_mark_dirty(s_canvas_layer);
  }
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_phase == PHASE_COUNTDOWN) { prv_start_blink(); return; }
  if (s_phase != PHASE_DONE) return;
  s_layout = (s_layout + 1) % LAYOUT_COUNT;
  prv_update_targets();
  if (s_layout == LAYOUT_VECTOR) {
    s_phase = PHASE_SHAKE_CYCLE; s_anim_step = 0; s_anim_rep = 0;
    s_h = s_target_h; schedule(ANIM_STEP_MS);
  } else {
    s_h = s_target_h;
  }
  layer_mark_dirty(s_canvas_layer);
}

static void tick_handler(struct tm *t, TimeUnits units) {
  if (s_phase == PHASE_DONE && s_layout == LAYOUT_VECTOR) {
    s_pending_hour = t->tm_hour; s_pending_minute = t->tm_min;
    s_digit_pending = true; s_phase = PHASE_SQUISH;
    s_going_down = true; s_overshot = false;
    schedule(ANIM_STEP_MS);
  } else if (s_phase == PHASE_SQUISH) {
    s_pending_hour = t->tm_hour; s_pending_minute = t->tm_min; s_digit_pending = true;
  } else {
    s_hour = t->tm_hour; s_minute = t->tm_min; layer_mark_dirty(s_canvas_layer);
  }
#if defined(PBL_HEALTH)
  s_steps_avg = prv_calc_steps_avg();
#endif
}

static void battery_handler(BatteryChargeState state){s_battery_pct=state.charge_percent;s_charging=state.is_charging;layer_mark_dirty(s_canvas_layer);}
static void bt_handler(bool connected){s_bt_connected=connected;layer_mark_dirty(s_canvas_layer);}

#if defined(PBL_HEALTH)
static void health_handler(HealthEventType event, void *context) {
  if(event==HealthEventMovementUpdate||event==HealthEventSignificantUpdate){
    HealthServiceAccessibilityMask mask;
    mask=health_service_metric_accessible(HealthMetricStepCount,time_start_of_today(),time(NULL));
    s_steps=(mask&HealthServiceAccessibilityMaskAvailable)?(int)health_service_sum_today(HealthMetricStepCount):0;
    mask=health_service_metric_accessible(HealthMetricWalkedDistanceMeters,time_start_of_today(),time(NULL));
    s_distance_m=(mask&HealthServiceAccessibilityMaskAvailable)?(int)health_service_sum_today(HealthMetricWalkedDistanceMeters):0;
  }
  layer_mark_dirty(s_canvas_layer);
}
#endif

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;
  t=dict_find(iter,MESSAGE_KEY_WeatherTempF);if(t)snprintf(s_weather_temp,sizeof(s_weather_temp),"%dF",(int)t->value->int32);
  t=dict_find(iter,MESSAGE_KEY_WeatherTempC);if(t&&!s_weather_temp[0])snprintf(s_weather_temp,sizeof(s_weather_temp),"%dC",(int)t->value->int32);
  t=dict_find(iter,MESSAGE_KEY_WeatherCode);
  if(t){int c=(int)t->value->int32;const char*d="WEATHER";if(c==0)d="CLEAR";else if(c<=3)d="CLOUDY";else if(c<=49)d="FOG";else if(c<=69)d="RAIN";else if(c<=79)d="SNOW";else if(c<=99)d="STORM";snprintf(s_weather_desc,sizeof(s_weather_desc),"%s",d);}
  layer_mark_dirty(s_canvas_layer);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas_layer, draw_layer);
  layer_add_child(root, s_canvas_layer);
  GRect ub = layer_get_unobstructed_bounds(root);
  s_target_h = prv_compute_target_h(ub.size.h);
  s_h = s_target_h;
  UnobstructedAreaHandlers ua = {.change = unobstructed_change};
  unobstructed_area_service_subscribe(ua, NULL);
  accel_tap_service_subscribe(accel_tap_handler);
  s_phase = PHASE_COUNTDOWN; s_countdown_digit = 9;
  s_overshot = false; s_cd_sub = CD_HOLD_MAX;
  layer_mark_dirty(s_canvas_layer); schedule(COUNTDOWN_HOLD_MS);
}

static void window_unload(Window *window) {
  unobstructed_area_service_unsubscribe(); accel_tap_service_unsubscribe();
  if(s_timer){app_timer_cancel(s_timer);s_timer=NULL;}
  layer_destroy(s_canvas_layer);
}

static void init(void) {
  s_fg = GColorWhite; s_bg = GColorBlack;
  s_window = window_create(); window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window,(WindowHandlers){.load=window_load,.unload=window_unload});
  window_stack_push(s_window, true);
  time_t now = time(NULL); struct tm *t = localtime(&now);
  s_hour = t->tm_hour; s_minute = t->tm_min;
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler); battery_handler(battery_state_service_peek());
  bluetooth_connection_service_subscribe(bt_handler); s_bt_connected = bluetooth_connection_service_peek();
#if defined(PBL_HEALTH)
  health_service_events_subscribe(health_handler, NULL); health_handler(HealthEventSignificantUpdate, NULL);
  s_steps_avg = prv_calc_steps_avg();
#endif
  app_message_register_inbox_received(inbox_received); app_message_open(256, 64);
}

static void deinit(void) {
  tick_timer_service_unsubscribe(); battery_state_service_unsubscribe(); bluetooth_connection_service_unsubscribe();
#if defined(PBL_HEALTH)
  health_service_events_unsubscribe();
#endif
  window_destroy(s_window);
}

int main(void){init();app_event_loop();deinit();}
