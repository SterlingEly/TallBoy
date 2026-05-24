#include <pebble.h>

// ============================================================
// TallBoy — main.c  v3.41
// Design: Sterling Ely. Code: Sterling Ely + Claude. 2026.
//
// Full-vector Pebble watchface. Digits are drawn from geometric
// primitives (fill_arc + graphics_fill_rect) — no bitmaps, no
// system fonts for digits. All layout is pixel-exact using a
// UNIT-based grid (8px on emery, 6px on flint).
//
// LAYOUT CYCLE (shake wrist):
//   FULL     → full-screen time, squish animation on minute change
//   INFO_1   → 1 info line above + 1 below, time fills middle
//   INFO_2   → 2 above + 2 below
//   INFO_3   → 3 above + 3 below
//   STACK_R  → stacked digits right, info column left
//   STACK_L  → stacked digits left, info column right
//
// TOUCH (emery only, PBL_TOUCH):
//   Tap screen to cycle bg corner radius: 2u → 3u → 4u → wrap.
//   TouchEvent struct fields TBD; fires on any touch for now.
//   TODO: filter to touch-down only once enum name is confirmed.
//
// FONT METRICS (emery, Gothic 24 Bold — measured on device):
//   INFO_LINE_H         = 28px  full cell height
//   INFO_TOP_PAD        = 10px  dead space above glyph top
//   INFO_GLYPH_H        = 18px  cap-top to descender-bottom
//   INFO_DESCENDER      =  4px  descenders (g, y, comma)
//   INFO_CAP_OFFSET     =  3px  caps above x-height
//   INFO_BOT_PAD        =  0px  glyph touches cell bottom
//   INFO_LINE_STEP      = 26px  1u glyph gap (stacked layouts)
//   INFO_LINE_STEP_WIDE = 22px  ½u glyph gap (wide info overlay layouts)
//   INFO_BOT_ADJUST     =  2px  half-descender (documented, not yet applied)
//
// MARGIN MODEL:
//   MARGIN_CANVAS = 1u   screen edge → bg rounded-rect boundary
//   MARGIN_DIGIT  = 1u   canvas boundary → digit/content area
//   MARGIN_OUTER  = 2u   = MARGIN_CANVAS + MARGIN_DIGIT (convenience alias)
//   Quick-look: bar reduces ub_h from bottom; BOTTOM_MARGIN() macro
//   returns MARGIN_DIGIT (1u) when bar active, MARGIN_OUTER (2u) otherwise.
//
// WIDE INFO OVERLAY MARGINS (v3.41):
//   Top glyph:    ½u from screen top  (ub_top + HALF_UNIT)
//   Bottom glyph: ½u from screen bottom (ub_bot - HALF_UNIT)
//   Line spacing: ½u between glyphs   (INFO_LINE_STEP_WIDE = 22px)
//   Time gap:     ½u each side of time digit block
//
// ANIMATION:
//   All phases use AppTimer at 16ms (≈60fps), ANIM_STEP_PX=8px/step.
//   Overshoot: digits animate to target+UNIT then snap back (ANIM_SNAP_MS).
//   Minute change: PHASE_SQUISH squishes to H_MIN, swaps time, re-expands.
//   Shake: PHASE_SHAKE_CYCLE when returning to LAYOUT_FULL.
//
// STEP PACE COLOR (emery + PBL_COLOR + PBL_HEALTH):
//   Compares today's steps to 7-day minute-level avg at current time.
//   0/no data→Black, 1-30%→Red, 31-60%→Orange, 61-90%→Yellow,
//   91-200%→Green, 201-400%→Blue, 401-700%→Indigo, 701-1000%→VividViolet,
//   1001%+→Black (easter egg).
//   Yellow and Green use dark (black) foreground; all others use white.
// ============================================================

// Uncomment to show magenta fill behind every text rect (layout debug):
// #define DEBUG_TEXT_BOXES

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------
#define LAYOUT_FULL      0
#define LAYOUT_INFO_1    1
#define LAYOUT_INFO_2    2
#define LAYOUT_INFO_3    3
#define LAYOUT_STACK_R   4
#define LAYOUT_STACK_L   5
#define LAYOUT_COUNT     6

// Number of shake animation steps (9-point sine-ish pulse)
#define SHAKE_LEN        9

static int s_layout = LAYOUT_FULL;

// ---------------------------------------------------------------------------
// Platform-specific geometry — everything in multiples of UNIT
// ---------------------------------------------------------------------------
#if defined(PBL_PLATFORM_EMERY)
  #define SCREEN_W          200   // px
  #define SCREEN_H          228   // px
  #define SLOT_W             40   // 5u digit slot width
  #define COLON_SLOT_X       80   // x of colon slot
  #define SLOT_H_TENS        12   // x of hour-tens slot
  #define SLOT_H_ONES        52   // x of hour-ones slot
  #define SLOT_M_TENS       108   // x of minute-tens slot
  #define SLOT_M_ONES       148   // x of minute-ones slot
  #define SIDE_MARGIN        12   // 1.5u — info text horizontal inset
  #define HALF_UNIT           4   // px
  #define INFO_LINE_H        28   // full font cell height (measured)
  #define INFO_GLYPH_H       18   // rendered glyph height (measured)
  #define INFO_TOP_PAD       10   // dead space above glyph in cell (measured)
  #define INFO_DESCENDER      4   // descender depth below baseline (measured)
  #define INFO_CAP_OFFSET     3   // cap height above x-height (measured)
  #define INFO_BOT_ADJUST     2   // half-descender — documented, not yet applied
  #define INFO_LINE_STEP     26   // = INFO_GLYPH_H + UNIT — 1u gap (stacked layouts)
  #define INFO_LINE_STEP_WIDE 22  // = INFO_GLYPH_H + HALF_UNIT — ½u gap (wide layouts)
  #define INFO_FONT_KEY    FONT_KEY_GOTHIC_24_BOLD
  #define UNIT                8   // px — base grid unit

  // Corner radius options for bg rounded rect, cycled by touch
  static const uint16_t s_radius_opts[] = { UNIT*2, UNIT*3, UNIT*4 };
  #define RADIUS_COUNT 3
  static int s_radius_idx = 0;

#else  // flint, basalt, diorite, aplite
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
  #define INFO_LINE_H        20   // estimated — not yet measured on device
  #define INFO_GLYPH_H       14
  #define INFO_TOP_PAD        6
  #define INFO_DESCENDER      3
  #define INFO_CAP_OFFSET     2
  #define INFO_BOT_ADJUST     1
  #define INFO_LINE_STEP     20   // = INFO_GLYPH_H + UNIT (stacked)
  #define INFO_LINE_STEP_WIDE 17  // = INFO_GLYPH_H + HALF_UNIT (wide)
  #define INFO_FONT_KEY    FONT_KEY_GOTHIC_18_BOLD
  #define UNIT                6
#endif

// Margin model (see file header for explanation)
#define MARGIN_CANVAS   UNIT
#define MARGIN_DIGIT    UNIT
#define MARGIN_OUTER    (MARGIN_CANVAS + MARGIN_DIGIT)

// Digit geometry constants
#define HALF_SLOT_PAD   (UNIT / 2)    // padding each side of glyph within slot
#define GLYPH_W         (UNIT * 4)    // 4u glyph content width
#define H_MIN           (UNIT * 11)   // minimum digit height (88px emery, 66px flint)

// Info line buffer — wider than max on-screen to suppress format-truncation warnings.
// GTextOverflowModeTrailingEllipsis clips to display width anyway.
#define INFO_LINE_BUF   40
#define INFO_LINES_MAX  8

// Animation timing
#define ANIM_STEP_PX     8    // px per timer tick during animation
#define ANIM_STEP_MS    16    // ~60fps
#define ANIM_SNAP_MS   120    // hold at overshoot peak before snap
#define ANIM_OVERSHOOT  UNIT  // 1u overshoot above target height

// Launch countdown / blink
#define COUNTDOWN_HOLD_MS  150  // pause between countdown digits
#define BLINK_REPS           2  // squish blinks after countdown

// Health history window (minutes per day, 7 days)
#define STEPS_AVG_MAX_MIN  120

// UTF-8 middle dot separator for paired info values
#define DOT " \xc2\xb7 "

// Quick-look bar detection: bar is active when ub_h < full screen height by more than 1u
#define QUICK_LOOK_ACTIVE(ub_h)  ((ub_h) < (SCREEN_H - UNIT))
#define BOTTOM_MARGIN(ub_h)      (QUICK_LOOK_ACTIVE(ub_h) ? MARGIN_DIGIT : MARGIN_OUTER)

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
typedef enum { CD_SHRINK, CD_HOLD_MIN, CD_EXPAND, CD_HOLD_MAX } CdSubPhase;
typedef enum { PHASE_COUNTDOWN, PHASE_BLINK, PHASE_DONE, PHASE_SQUISH, PHASE_SHAKE_CYCLE } Phase;

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
static Window    *s_window;
static Layer     *s_canvas_layer;
static int        s_hour = 0, s_minute = 0;
static int        s_h = 0, s_target_h = 0;  // current and target digit height (px)
static GColor     s_fg, s_bg;
static Phase      s_phase = PHASE_COUNTDOWN;
static int        s_anim_step = 0, s_anim_rep = 0;
static bool       s_going_down = true, s_overshot = false;
static int        s_countdown_digit = 9;
static CdSubPhase s_cd_sub = CD_HOLD_MAX;
static AppTimer  *s_timer = NULL;
static bool       s_digit_pending = false;  // time swap pending at H_MIN
static int        s_pending_hour = 0, s_pending_minute = 0;
static int        s_battery_pct = 100;
static bool       s_charging = false, s_bt_connected = true;
static int        s_steps = 0, s_distance_m = 0, s_calories = 0, s_heart_rate = 0;
static int        s_steps_expected = -1;  // -1 = insufficient history
static int        s_sunrise_min = -1, s_sunset_min = -1;  // minutes since midnight, -1 = unknown
static char       s_weather_temp[8] = "";   // e.g. "72F" or "22C"
static char       s_weather_desc[32] = "";  // e.g. "sunny", "rain"

// ---------------------------------------------------------------------------
// Step pace background color (emery + color + health only)
// ---------------------------------------------------------------------------
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
  return GColorBlack;  // 1001%+ easter egg: back to black
}

// Yellow and Green are light enough to require dark (black) foreground
static bool prv_bg_needs_dark_fg(GColor bg) {
  return gcolor_equal(bg, GColorYellow) || gcolor_equal(bg, GColorGreen);
}
#endif  // PBL_COLOR

// ---------------------------------------------------------------------------
// Health data — refreshed once per minute in tick_handler
// ---------------------------------------------------------------------------
#if defined(PBL_HEALTH)
static HealthMinuteData s_minute_buf[STEPS_AVG_MAX_MIN];  // 480 bytes, static allocation

// Compute expected cumulative steps at current time of day based on 7-day history.
// Uses minute-level data for the trailing window (up to STEPS_AVG_MAX_MIN minutes).
// Returns -1 if < 2 minutes have elapsed today or no history available.
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
      // Scale up proportionally if window is shorter than elapsed time
      if (elapsed_min > window) day_steps = (day_steps * elapsed_min) / window;
      total += day_steps;
      day_count++;
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
#endif  // PBL_HEALTH

// ---------------------------------------------------------------------------
// Drawing primitives
// ---------------------------------------------------------------------------

// Filled annular arc segment. ro/ri in px, a0/a1 in degrees.
static void fill_arc(GContext *ctx, int cx, int cy, int ro, int ri, int a0, int a1) {
  GRect b = GRect(cx - ro, cy - ro, ro * 2, ro * 2);
  graphics_fill_radial(ctx, b, GOvalScaleModeFitCircle, (uint16_t)(ro - ri),
                       DEG_TO_TRIGANGLE(a0), DEG_TO_TRIGANGLE(a1));
}

// Draw one vector digit. slot_x is left edge of 5u slot; cy is vertical center; h is height.
// Digit geometry is fully parameterized by h — only vertical bars change length.
// Arcs (ro=2u, ri=1u, sw=1u) are invariant — never change with h.
static void draw_digit_vec(GContext *ctx, int digit, int slot_x, int cy, int h) {
  graphics_context_set_fill_color(ctx, s_fg);

  // Geometry constants — NEVER change with h
  const int ro = UNIT * 2;   // arc outer radius
  const int ri = UNIT;       // arc inner radius
  const int sw = UNIT;       // stroke width

  // Horizontal positions (derived from slot_x)
  int gx     = slot_x + HALF_SLOT_PAD;    // glyph left edge
  int gx_r   = gx + GLYPH_W - sw;         // glyph right stroke left edge
  int cap_cx = gx + ro;                   // arc center x (same for all arcs)

  // Vertical positions (derived from h)
  int top_y  = cy - h / 2;
  int bot_y  = cy + h / 2;
  int bar    = (h - 7 * UNIT) / 2;  // gap between inner cap centers; 0 at H_MIN
  if (bar < 0) bar = 0;
  int t_bc   = cy - (ro - HALF_UNIT);     // top bottom-cap center y
  int t_tc   = t_bc - bar;               // top top-cap center y
  int b_tc   = cy + (ro - HALF_UNIT);     // bottom top-cap center y
  int b_bc   = b_tc + bar;               // bottom bottom-cap center y
  int tail   = (h > H_MIN) ? (h - H_MIN) / 4 : 0;  // tail stub length; 0 at H_MIN

  // Convenience macros for readability — undefined at end of function
  #define top_cy  (cy - (h - ro*2) / 2)   // single-arc top cap center y
  #define bot_cy  (cy + (h - ro*2) / 2)   // single-arc bottom cap center y
  #define VBAR(x, y0, y1)  if ((y1) > (y0)) graphics_fill_rect(ctx, GRect((x),(y0),sw,(y1)-(y0)), 0, GCornerNone)
  #define HBAR(y)  graphics_fill_rect(ctx, GRect(gx,(y),GLYPH_W,sw), 0, GCornerNone)
  #define NUB(x, y)  graphics_fill_rect(ctx, GRect((x),(y),sw,sw), 0, GCornerNone)

  switch (digit) {
    case 0:
      fill_arc(ctx, cap_cx, top_cy, ro, ri, 270, 450);
      fill_arc(ctx, cap_cx, bot_cy, ro, ri,  90, 270);
      VBAR(gx,   top_cy, bot_cy);
      VBAR(gx_r, top_cy, bot_cy);
      break;

    case 1: {
      HBAR(bot_y - sw);
      int stem_x = gx + GLYPH_W / 2 - sw / 2;
      VBAR(stem_x, top_y, bot_y - sw);
      // Diagonal chamfer serif at top-left
      int cap_r = stem_x + sw, diag_h = cap_r - gx;
      if (diag_h > 0) {
        GPoint pts[5] = {
          { cap_r - 1, top_y },
          { cap_r - 1, top_y + sw - HALF_UNIT },
          { gx    - 1, top_y + sw + diag_h - HALF_UNIT },
          { gx    - 1, top_y + diag_h - sw },
          { stem_x- 1, top_y },
        };
        GPathInfo pi = { .num_points = 5, .points = pts };
        GPath *path = gpath_create(&pi);
        if (path) { gpath_draw_filled(ctx, path); gpath_destroy(path); }
      }
      break;
    }

    case 2: {
      fill_arc(ctx, cap_cx, top_cy, ro, ri, 270, 450);
      VBAR(gx,   top_cy, top_cy + tail);
      VBAR(gx_r, top_cy, top_cy + tail);
      int dy = (bot_y - sw) - (top_cy + tail);
      if (dy > 0) {
        // Diagonal stroke from top-right to bottom-left
        GPoint pts[4] = {
          { gx_r - 1,         top_cy + tail },
          { gx_r + sw,        top_cy + tail },
          { gx   - 1 + sw + 1, bot_y  - sw  },
          { gx   - 1,          bot_y  - sw  },
        };
        GPathInfo pi = { .num_points = 4, .points = pts };
        GPath *path = gpath_create(&pi);
        if (path) { gpath_draw_filled(ctx, path); gpath_destroy(path); }
      }
      HBAR(bot_y - sw);
      break;
    }

    case 3:
      fill_arc(ctx, cap_cx, t_tc, ro, ri, 270, 450);
      VBAR(gx,   t_tc, t_tc + tail);
      VBAR(gx_r, t_tc, t_bc);
      fill_arc(ctx, cap_cx, t_bc, ro, ri,  90, 180);
      NUB(gx + sw, t_bc + sw);  // inner nub connecting the two ring halves
      fill_arc(ctx, cap_cx, b_tc, ro, ri, 360, 450);
      VBAR(gx,   b_bc - tail, b_bc);
      VBAR(gx_r, b_tc, b_bc);
      fill_arc(ctx, cap_cx, b_bc, ro, ri,  90, 270);
      break;

    case 4:
      VBAR(gx,   top_y, cy - HALF_UNIT + sw);
      VBAR(gx_r, top_y, bot_y);
      graphics_fill_rect(ctx, GRect(gx, cy - HALF_UNIT, GLYPH_W, sw), 0, GCornerNone);
      break;

    case 5:
      HBAR(top_y);
      VBAR(gx,   top_y + sw, b_tc);
      fill_arc(ctx, cap_cx, b_tc, ro, ri, 270, 450);
      fill_arc(ctx, cap_cx, b_bc, ro, ri,  90, 270);
      VBAR(gx_r, b_tc, b_bc);
      if (tail > 0) VBAR(gx, b_bc - tail, b_bc);
      break;

    case 6:
      fill_arc(ctx, cap_cx, top_cy, ro, ri, 270, 450);
      VBAR(gx_r, top_cy, top_cy + tail);
      VBAR(gx,   top_cy, b_bc);
      fill_arc(ctx, cap_cx, b_tc, ro, ri, 270, 450);
      fill_arc(ctx, cap_cx, b_bc, ro, ri,  90, 270);
      VBAR(gx_r, b_tc, b_bc);
      break;

    case 7: {
      HBAR(top_y);
      GPoint pts[4] = {
        { gx_r - 1,  top_y + sw },
        { gx_r + sw, top_y + sw },
        { gx   + sw, bot_y  - 1 },
        { gx   - 1,  bot_y  - 1 },
      };
      GPathInfo pi = { .num_points = 4, .points = pts };
      GPath *path = gpath_create(&pi);
      if (path) { gpath_draw_filled(ctx, path); gpath_destroy(path); }
      break;
    }

    case 8:
      fill_arc(ctx, cap_cx, t_tc, ro, ri, 270, 450);
      fill_arc(ctx, cap_cx, t_bc, ro, ri,  90, 270);
      VBAR(gx,   t_tc, t_bc);
      VBAR(gx_r, t_tc, t_bc);
      fill_arc(ctx, cap_cx, b_tc, ro, ri, 270, 450);
      fill_arc(ctx, cap_cx, b_bc, ro, ri,  90, 270);
      VBAR(gx,   b_tc, b_bc);
      VBAR(gx_r, b_tc, b_bc);
      break;

    case 9:
      fill_arc(ctx, cap_cx, t_tc, ro, ri, 270, 450);
      fill_arc(ctx, cap_cx, t_bc, ro, ri,  90, 270);
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

// Two filled circles for the time colon, positioned at ±h/4 from center
static void draw_colon_vec(GContext *ctx, int slot_x, int cy, int h) {
  graphics_context_set_fill_color(ctx, s_fg);
  int r = UNIT / 2, dx = slot_x + SLOT_W / 2;
  graphics_fill_radial(ctx, GRect(dx-r, cy-h/4-r+2, r*2, r*2),
                       GOvalScaleModeFitCircle, (uint16_t)r, 0, DEG_TO_TRIGANGLE(360));
  graphics_fill_radial(ctx, GRect(dx-r, cy+h/4-r-2, r*2, r*2),
                       GOvalScaleModeFitCircle, (uint16_t)r, 0, DEG_TO_TRIGANGLE(360));
}

// Draw all four digits + colon at a given center y and height
static void draw_digits_vec(GContext *ctx, int h_tens, int h_ones,
                             int m_tens, int m_ones, int h, int cy) {
  draw_digit_vec(ctx, h_tens, SLOT_H_TENS, cy, h);
  draw_digit_vec(ctx, h_ones, SLOT_H_ONES, cy, h);
  draw_colon_vec(ctx, COLON_SLOT_X, cy, h);
  draw_digit_vec(ctx, m_tens, SLOT_M_TENS, cy, h);
  draw_digit_vec(ctx, m_ones, SLOT_M_ONES, cy, h);
}

// Draw four digits (no colon) for stacked layout — two rows, same digit size
static void draw_stacked_vec(GContext *ctx, int h_tens, int h_ones,
                              int m_tens, int m_ones, int dh,
                              int tens_x, int ones_x, int h_cy, int m_cy) {
  draw_digit_vec(ctx, h_tens, tens_x, h_cy, dh);
  draw_digit_vec(ctx, h_ones, ones_x, h_cy, dh);
  draw_digit_vec(ctx, m_tens, tens_x, m_cy, dh);
  draw_digit_vec(ctx, m_ones, ones_x, m_cy, dh);
}

// ---------------------------------------------------------------------------
// Layout geometry helpers
// ---------------------------------------------------------------------------

// Height of a visual block of n info lines (top of first glyph to bottom of last).
// step = INFO_LINE_STEP (stacked, 1u spacing) or INFO_LINE_STEP_WIDE (wide, ½u spacing)
static int prv_info_block_h(int n, int step) {
  if (n <= 0) return 0;
  return INFO_GLYPH_H + (n - 1) * step;
}

// Target digit height for current layout and unobstructed bounds height.
// lines_above / lines_below are the info line counts for info overlay layouts.
static int prv_compute_target_h(int ub_h, int lines_above, int lines_below) {
  if (lines_above == 0 && lines_below == 0) return ub_h - 2 * MARGIN_OUTER;
  int reserved = 0;
  // Wide layout: ½u outer gap + glyph block + ½u inner gap to time, each side
  reserved += HALF_UNIT + prv_info_block_h(lines_above, INFO_LINE_STEP_WIDE) + HALF_UNIT;
  reserved += HALF_UNIT + prv_info_block_h(lines_below, INFO_LINE_STEP_WIDE) + HALF_UNIT;
  int h = ub_h - reserved;
  return h < H_MIN ? H_MIN : h;
}

// Returns above/below line counts for the current layout.
// Return value is total line count (above + below).
static int prv_layout_lines(int *above, int *below) {
  switch (s_layout) {
    case LAYOUT_INFO_1: *above = 1; *below = 1; return 2;
    case LAYOUT_INFO_2: *above = 2; *below = 2; return 4;
    case LAYOUT_INFO_3: *above = 3; *below = 3; return 6;
    default:            *above = 0; *below = 0; return 0;
  }
}

// Digit height for stacked layouts: two equal rows with 1u gap, 2u margins
static int prv_compute_stacked_h(int ub_h) {
  int h = (ub_h - 2 * MARGIN_OUTER - UNIT) / 2;
  return h < H_MIN ? H_MIN : h;
}

// Recompute s_target_h from current layout and live unobstructed bounds
static void prv_update_targets(void) {
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  int above, below;
  prv_layout_lines(&above, &below);
  s_target_h = prv_compute_target_h(ub.size.h, above, below);
}

// ---------------------------------------------------------------------------
// String formatting helpers
// ---------------------------------------------------------------------------
static const char *s_day_names[] = {
  "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};
static const char *s_month_names[] = {
  "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

static void prv_fmt_distance(char *buf, int len) {
  if (strcmp(i18n_get_system_locale(), "en_US") == 0) {
    int mx = (s_distance_m * 10) / 1609;
    snprintf(buf, len, "%d.%d mi", mx/10, mx%10);
  } else {
    int kx = (s_distance_m * 10) / 1000;
    snprintf(buf, len, "%d.%d km", kx/10, kx%10);
  }
}

// Short form: raw under 1k, "1.5k" up to 9.9k, "10,000" for 10k+
static void prv_fmt_steps_short(char *buf, int len, int steps) {
  if (steps >= 10000) snprintf(buf, len, "%d,%03d", steps/1000, steps%1000);
  else if (steps >= 1000) snprintf(buf, len, "%d.%dk", steps/1000, (steps%1000)/100);
  else snprintf(buf, len, "%d", steps);
}

static void prv_fmt_steps_long(char *buf, int len, int steps) {
  if (steps >= 1000) snprintf(buf, len, "%d,%03d", steps/1000, steps%1000);
  else snprintf(buf, len, "%d", steps);
}

// Format minutes-since-midnight as "12:34am" / "12:34pm"
static void prv_fmt_time_min(char *buf, int len, int total_min) {
  if (total_min < 0) { snprintf(buf, len, "--"); return; }
  int h = (total_min / 60) % 12;
  if (!h) h = 12;
  snprintf(buf, len, "%d:%02d%s", h, total_min % 60, total_min < 720 ? "am" : "pm");
}

static void prv_fmt_bat_bt(char *buf, int len) {
  if (!s_bt_connected)  snprintf(buf, len, "no phone");
  else if (s_charging)  snprintf(buf, len, "%d%% bat +", s_battery_pct);
  else                  snprintf(buf, len, "%d%% bat", s_battery_pct);
}

// ---------------------------------------------------------------------------
// Info line builders
// Wide layout: always fills exactly max_lines slots; debug fallbacks ensure
// all slots are visible even without real data, for layout tuning.
// ---------------------------------------------------------------------------
static int build_info_lines_wide(char lines[][INFO_LINE_BUF], int max_lines, struct tm *t) {
  int n = 0;

  // A1: day & date (always available from RTC)
  if (n < max_lines) {
    if (t) snprintf(lines[n++], INFO_LINE_BUF, "%s, %s %d",
                    s_day_names[t->tm_wday], s_month_names[t->tm_mon], t->tm_mday);
    else   snprintf(lines[n++], INFO_LINE_BUF, "Mon, Jan 1");
  }

  // A2: weather — "%.7s & %.29s" guarantees max 7+4+29=40 chars <= INFO_LINE_BUF
  if (n < max_lines) {
    if (s_weather_temp[0] && s_weather_desc[0])
      snprintf(lines[n++], INFO_LINE_BUF, "%.7s & %.29s", s_weather_temp, s_weather_desc);
    else if (s_weather_temp[0])
      snprintf(lines[n++], INFO_LINE_BUF, "%s", s_weather_temp);
    else
      snprintf(lines[n++], INFO_LINE_BUF, "72 F & sunny");  // debug fallback
  }

  // A3: sunrise · sunset
  if (n < max_lines) {
    if (s_sunrise_min >= 0 || s_sunset_min >= 0) {
      char rise[12], set[12];
      prv_fmt_time_min(rise, sizeof(rise), s_sunrise_min);
      prv_fmt_time_min(set,  sizeof(set),  s_sunset_min);
      snprintf(lines[n++], INFO_LINE_BUF, "%s" DOT "%s", rise, set);
    } else {
      snprintf(lines[n++], INFO_LINE_BUF, "6:02am" DOT "8:14pm");  // debug fallback
    }
  }

  // B1: steps · distance
  if (n < max_lines) {
#if defined(PBL_HEALTH)
    char sbuf[16]; prv_fmt_steps_long(sbuf, sizeof(sbuf), s_steps);
    if (s_distance_m > 0) {
      char dbuf[12]; prv_fmt_distance(dbuf, sizeof(dbuf));
      snprintf(lines[n++], INFO_LINE_BUF, "%s steps" DOT "%s", sbuf, dbuf);
    } else {
      snprintf(lines[n++], INFO_LINE_BUF, "%s steps", sbuf);
    }
#else
    snprintf(lines[n++], INFO_LINE_BUF, "3,500 steps" DOT "2.1 mi");
#endif
  }

  // B2: expected steps · pace %
  if (n < max_lines) {
#if defined(PBL_HEALTH)
    if (s_steps_expected > 0) {
      char ebuf[16]; prv_fmt_steps_long(ebuf, sizeof(ebuf), s_steps_expected);
      snprintf(lines[n++], INFO_LINE_BUF, "exp %s" DOT "%d%%",
               ebuf, (s_steps * 100) / s_steps_expected);
    } else {
      snprintf(lines[n++], INFO_LINE_BUF, "exp -- " DOT " --");
    }
#else
    snprintf(lines[n++], INFO_LINE_BUF, "exp 3,500" DOT "100%%");
#endif
  }

  // B3: heart rate · calories (always shows something for debug)
  if (n < max_lines) {
#if defined(PBL_HEALTH)
    bool has_hr = s_heart_rate > 0, has_cal = s_calories > 0;
    if      (has_hr && has_cal) snprintf(lines[n++], INFO_LINE_BUF, "%d bpm" DOT "%d cal", s_heart_rate, s_calories);
    else if (has_hr)            snprintf(lines[n++], INFO_LINE_BUF, "%d bpm", s_heart_rate);
    else if (has_cal)           snprintf(lines[n++], INFO_LINE_BUF, "%d cal", s_calories);
    else                        snprintf(lines[n++], INFO_LINE_BUF, "-- bpm" DOT "-- cal");
#else
    snprintf(lines[n++], INFO_LINE_BUF, "72 bpm" DOT "212 cal");
#endif
  }

  // B4+: battery / BT status
  if (n < max_lines) {
    char bbuf[24]; prv_fmt_bat_bt(bbuf, sizeof(bbuf));
    snprintf(lines[n++], INFO_LINE_BUF, "%s", bbuf);
  }

  return n;
}

// Stacked layout info column — shorter strings to fit narrow column
static int build_info_lines_stacked(char lines[][INFO_LINE_BUF], int max_lines, struct tm *t) {
  int n = 0;

  if (n < max_lines) {
    if (s_weather_temp[0]) snprintf(lines[n++], INFO_LINE_BUF, "%s", s_weather_temp);
    else                   snprintf(lines[n++], INFO_LINE_BUF, "72 F");
  }
  if (n < max_lines && t) snprintf(lines[n++], INFO_LINE_BUF, "%s", s_day_names[t->tm_wday]);
  if (n < max_lines && t) snprintf(lines[n++], INFO_LINE_BUF, "%s %d", s_month_names[t->tm_mon], t->tm_mday);

#if defined(PBL_HEALTH)
  if (n < max_lines) {
    char sbuf[16]; prv_fmt_steps_short(sbuf, sizeof(sbuf), s_steps);
    snprintf(lines[n++], INFO_LINE_BUF, "%s steps", sbuf);
  }
  if (n < max_lines && s_steps_expected > 0) {
    char ebuf[16]; prv_fmt_steps_long(ebuf, sizeof(ebuf), s_steps_expected);
    snprintf(lines[n++], INFO_LINE_BUF, "exp %s", ebuf);
  }
  if (n < max_lines && s_steps_expected > 0) {
    snprintf(lines[n++], INFO_LINE_BUF, "%d%% exp", (s_steps * 100) / s_steps_expected);
  }
  if (n < max_lines && s_steps_expected > 0) {
    int diff = s_steps - s_steps_expected;
    if (diff == 0) {
      snprintf(lines[n++], INFO_LINE_BUF, "on pace");
    } else {
      char dbuf[16]; prv_fmt_steps_long(dbuf, sizeof(dbuf), diff < 0 ? -diff : diff);
      snprintf(lines[n++], INFO_LINE_BUF, "%s%s steps", diff > 0 ? "+" : "-", dbuf);
    }
  }
  if (n < max_lines && s_distance_m > 0) prv_fmt_distance(lines[n++], INFO_LINE_BUF);
#endif

  if (n < max_lines) prv_fmt_bat_bt(lines[n++], INFO_LINE_BUF);
  return n;
}

// ---------------------------------------------------------------------------
// Text drawing — glyph-y positioning compensates for INFO_TOP_PAD
// ---------------------------------------------------------------------------
static GFont prv_info_font(void) { return fonts_get_system_font(INFO_FONT_KEY); }

// Draw text so the visual glyph top appears at `glyph_y`.
// The rect origin is shifted up by INFO_TOP_PAD to account for the font's
// internal dead space above the glyph.
static void draw_text_at_glyph_y(GContext *ctx, const char *text,
                                  int x, int glyph_y, int width) {
  GRect r = GRect(x, glyph_y - INFO_TOP_PAD, width, INFO_LINE_H);
#if defined(DEBUG_TEXT_BOXES)
  // Magenta fill shows exact rect boundaries for margin calibration
  graphics_context_set_fill_color(ctx, GColorMagenta);
  graphics_fill_rect(ctx, r, 0, GCornerNone);
#endif
  graphics_context_set_text_color(ctx, s_fg);
  graphics_draw_text(ctx, text, prv_info_font(), r,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// Stacked info column: n lines distributed evenly between adjusted margins.
// Uses INFO_LINE_STEP (1u spacing) via equal slot distribution.
static void draw_info_column(GContext *ctx, GRect area, struct tm *t) {
  char lines[INFO_LINES_MAX][INFO_LINE_BUF];
  int n = build_info_lines_stacked(lines, INFO_LINES_MAX, t);
  if (!n) return;

  // Glyph range: top at MARGIN_OUTER-HALF_UNIT, bottom extends INFO_GLYPH_H past 2u margin
  int glyph_top = area.origin.y + MARGIN_OUTER - HALF_UNIT;
  int glyph_bot = area.origin.y + area.size.h - MARGIN_OUTER + INFO_GLYPH_H;
  int slot_h    = (glyph_bot - glyph_top) / n;

  for (int i = 0; i < n; i++)
    draw_text_at_glyph_y(ctx, lines[i], area.origin.x,
                         glyph_top + i * slot_h, area.size.w);
}

// Wide layout: lines anchored to top edge, growing downward.
// Uses INFO_LINE_STEP_WIDE (½u spacing between glyphs).
static void draw_info_block_down(GContext *ctx, char lines[][INFO_LINE_BUF], int n,
                                  int glyph_y_start, int width, int x) {
  for (int i = 0; i < n; i++)
    draw_text_at_glyph_y(ctx, lines[i], x,
                         glyph_y_start + i * INFO_LINE_STEP_WIDE, width);
}

// Wide layout: lines anchored to bottom edge, growing upward.
// glyph_bot is the pixel where the bottom of the last glyph should sit.
// Uses INFO_LINE_STEP_WIDE (½u spacing between glyphs).
static void draw_info_block_up(GContext *ctx, char lines[][INFO_LINE_BUF],
                                int first_idx, int n, int glyph_bot, int width, int x) {
  if (n <= 0) return;
  for (int i = 0; i < n; i++) {
    // Last line (i = n-1): glyph top = glyph_bot - INFO_GLYPH_H
    // Each preceding line is INFO_LINE_STEP_WIDE higher
    int glyph_y = glyph_bot - INFO_GLYPH_H - (n - 1 - i) * INFO_LINE_STEP_WIDE;
    draw_text_at_glyph_y(ctx, lines[first_idx + i], x, glyph_y, width);
  }
}

// ---------------------------------------------------------------------------
// Main draw callback
// ---------------------------------------------------------------------------
static void draw_layer(Layer *layer, GContext *ctx) {
  Layer *root = window_get_root_layer(s_window);
  GRect ub    = layer_get_unobstructed_bounds(root);
  int ub_top  = ub.origin.y;
  int ub_h    = ub.size.h;
  int ub_bot  = ub_top + ub_h;
  GRect bounds = layer_get_bounds(layer);

  // Determine background color and foreground from step pace (emery+color+health)
  // or fall back to black bg / white fg
#if defined(PBL_COLOR) && defined(PBL_HEALTH)
  GColor bg = prv_pace_color(s_steps, s_steps_expected);
  s_fg = prv_bg_needs_dark_fg(bg) ? GColorBlack : GColorWhite;
#else
  GColor bg = s_bg;
  s_fg = GColorWhite;
#endif

  // 1. Black fill — corners outside rounded rect are always black
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // 2. Bg color as rounded rect (radius cycles via touch on emery)
#if defined(PBL_PLATFORM_EMERY)
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, s_radius_opts[s_radius_idx], GCornersAll);
#else
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
#endif

  // Precompute digit values
  int hr     = s_hour % 12; if (!hr) hr = 12;
  int h_tens = hr / 10, h_ones = hr % 10;
  int m_tens = s_minute / 10, m_ones = s_minute % 10;
  int bot_margin = BOTTOM_MARGIN(ub_h);

  // Countdown phase: draw only animated countdown digits, no info
  if (s_phase == PHASE_COUNTDOWN) {
    draw_digits_vec(ctx, s_countdown_digit, s_countdown_digit,
                    s_countdown_digit, s_countdown_digit,
                    s_h, ub_top + ub_h / 2);
    return;
  }

  // Get current time for info lines (NULL during animation to avoid unnecessary work)
  time_t now_t = time(NULL);
  struct tm *tm_now = (s_phase == PHASE_DONE) ? localtime(&now_t) : NULL;

  if (s_layout == LAYOUT_FULL) {
    draw_digits_vec(ctx, h_tens, h_ones, m_tens, m_ones, s_h, ub_top + ub_h / 2);

  } else if (s_layout == LAYOUT_INFO_1 || s_layout == LAYOUT_INFO_2 || s_layout == LAYOUT_INFO_3) {
    int lines_above, lines_below;
    prv_layout_lines(&lines_above, &lines_below);

    // Above block: first glyph at ½u from screen top
    int above_start = ub_top + HALF_UNIT;
    int above_end   = above_start + prv_info_block_h(lines_above, INFO_LINE_STEP_WIDE);

    // Below block: last glyph bottom at ½u from unobstructed bottom
    int below_end   = ub_bot - HALF_UNIT;
    int below_start = below_end - prv_info_block_h(lines_below, INFO_LINE_STEP_WIDE);

    // Time fills whatever is left between the two blocks (½u gap each side)
    int time_cy = (above_end + HALF_UNIT + below_start - HALF_UNIT) / 2;

    // Build and draw info lines (info draws first, digits on top)
    char all_lines[INFO_LINES_MAX][INFO_LINE_BUF];
    int total = build_info_lines_wide(all_lines, lines_above + lines_below, tm_now);

    int above_n = lines_above < total ? lines_above : total;
    draw_info_block_down(ctx, all_lines, above_n,
                         above_start, SCREEN_W - 2*SIDE_MARGIN, SIDE_MARGIN);

    int below_n = total - lines_above;
    if (below_n > lines_below) below_n = lines_below;
    if (below_n > 0)
      draw_info_block_up(ctx, all_lines, lines_above, below_n,
                         below_end, SCREEN_W - 2*SIDE_MARGIN, SIDE_MARGIN);

    // Digits last = drawn on top of info lines
    draw_digits_vec(ctx, h_tens, h_ones, m_tens, m_ones, s_h, time_cy);

  } else {
    // Stacked layouts: fixed digit size, info column opposite the digits
    int dh   = prv_compute_stacked_h(ub_h);
    int h_cy = ub_top + MARGIN_OUTER + dh / 2;          // top digit center
    int m_cy = ub_bot - bot_margin - dh / 2;             // bottom digit center
    int tens_x, ones_x, info_x, info_w;

    if (s_layout == LAYOUT_STACK_R) {
      ones_x = SCREEN_W - SIDE_MARGIN - SLOT_W;
      tens_x = ones_x - SLOT_W;
      info_x = SIDE_MARGIN;
      info_w = tens_x - SIDE_MARGIN * 2;
    } else {  // LAYOUT_STACK_L
      tens_x = SIDE_MARGIN;
      ones_x = SIDE_MARGIN + SLOT_W;
      info_x = ones_x + SLOT_W + SIDE_MARGIN;
      info_w = SCREEN_W - info_x - SIDE_MARGIN;
    }

    if (tm_now && info_w > 20)
      draw_info_column(ctx, GRect(info_x, ub_top, info_w, ub_h), tm_now);
    draw_stacked_vec(ctx, h_tens, h_ones, m_tens, m_ones, dh,
                     tens_x, ones_x, h_cy, m_cy);
  }
}

// ---------------------------------------------------------------------------
// Animation engine
// ---------------------------------------------------------------------------
static void timer_cb(void *data);

static void schedule(uint32_t ms) {
  if (s_timer) app_timer_cancel(s_timer);
  s_timer = app_timer_register(ms, timer_cb, NULL);
}

// Begin blink phase: start from target height, squish down, expand back (BLINK_REPS times)
static void prv_start_blink(void) {
  s_h = s_target_h;
  s_going_down = true;
  s_anim_rep = 0;
  s_overshot = false;
  s_phase = PHASE_BLINK;
  layer_mark_dirty(s_canvas_layer);
  schedule(ANIM_STEP_MS);
}

// One step of expand animation. Returns true when target height is reached.
// Overshoots by ANIM_OVERSHOOT then snaps back after ANIM_SNAP_MS.
static bool prv_expand_step(void) {
  if (s_overshot) {
    s_h = s_target_h;
    s_overshot = false;
    layer_mark_dirty(s_canvas_layer);
    return true;
  }
  s_h += ANIM_STEP_PX;
  layer_mark_dirty(s_canvas_layer);
  int peak = s_target_h + ANIM_OVERSHOOT;
  if (s_h >= peak) {
    s_h = peak;
    s_overshot = true;
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
          else schedule(ANIM_STEP_MS);
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
            s_cd_sub = CD_HOLD_MAX;
            schedule(COUNTDOWN_HOLD_MS);
          } else {
            s_h += ANIM_STEP_PX;
            layer_mark_dirty(s_canvas_layer);
            int pk = s_target_h + ANIM_OVERSHOOT;
            if (s_h >= pk) { s_h = pk; s_overshot = true; schedule(ANIM_SNAP_MS); }
            else schedule(ANIM_STEP_MS);
          }
          break;
        }

        case CD_HOLD_MAX:
          s_cd_sub = CD_SHRINK;
          schedule(ANIM_STEP_MS);
          break;
      }
      break;

    case PHASE_BLINK:
      if (s_going_down) {
        s_h -= ANIM_STEP_PX;
        layer_mark_dirty(s_canvas_layer);
        if (s_h <= H_MIN) { s_h = H_MIN; s_going_down = false; s_overshot = false; }
        schedule(ANIM_STEP_MS);
      } else {
        bool done = prv_expand_step();
        if (done) {
          if (++s_anim_rep < BLINK_REPS) {
            s_going_down = true; s_overshot = false; schedule(ANIM_STEP_MS);
          } else {
            s_phase = PHASE_DONE;
            layer_mark_dirty(s_canvas_layer);
          }
        }
      }
      break;

    case PHASE_SQUISH:
      if (s_going_down) {
        s_h -= ANIM_STEP_PX;
        layer_mark_dirty(s_canvas_layer);
        if (s_h <= H_MIN) {
          s_h = H_MIN;
          // Swap to new time exactly at minimum height
          if (s_digit_pending) {
            s_hour = s_pending_hour; s_minute = s_pending_minute;
            s_digit_pending = false;
          }
          s_going_down = false; s_overshot = false;
        }
        schedule(ANIM_STEP_MS);
      } else {
        bool done = prv_expand_step();
        if (done) { s_phase = PHASE_DONE; layer_mark_dirty(s_canvas_layer); }
      }
      break;

    case PHASE_SHAKE_CYCLE: {
      // 9-point pulse wave: 0, -1u, -2u, -3u, -2u, -1u, 0, +1u, 0
      static const int OFF[SHAKE_LEN] = {
        0, -UNIT, -(UNIT*2), -(UNIT*3), -(UNIT*2), -UNIT, 0, UNIT, 0
      };
      if (++s_anim_step < SHAKE_LEN) {
        int h = s_target_h + OFF[s_anim_step];
        s_h = h < H_MIN ? H_MIN : h;
        layer_mark_dirty(s_canvas_layer);
        schedule(s_anim_step == SHAKE_LEN - 2 ? ANIM_SNAP_MS : ANIM_STEP_MS);
      } else if (++s_anim_rep < 2) {
        s_anim_step = 0; s_h = s_target_h;
        layer_mark_dirty(s_canvas_layer); schedule(ANIM_STEP_MS);
      } else {
        s_phase = PHASE_DONE; s_h = s_target_h;
        layer_mark_dirty(s_canvas_layer);
      }
      break;
    }

    case PHASE_DONE: break;
  }
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

static void unobstructed_change(AnimationProgress progress, void *ctx) {
  prv_update_targets();
  if (s_phase == PHASE_DONE) { s_h = s_target_h; layer_mark_dirty(s_canvas_layer); }
}

// Shake (accel tap): advance to next layout, animate transition
static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_phase == PHASE_COUNTDOWN) { prv_start_blink(); return; }
  if (s_phase != PHASE_DONE) return;

  s_layout = (s_layout + 1) % LAYOUT_COUNT;
  prv_update_targets();

  if (s_layout == LAYOUT_FULL) {
    s_phase = PHASE_SHAKE_CYCLE; s_anim_step = 0; s_anim_rep = 0;
    s_h = s_target_h; schedule(ANIM_STEP_MS);
  } else {
    s_phase = PHASE_SQUISH; s_going_down = true; s_overshot = false;
    schedule(ANIM_STEP_MS);
  }
  layer_mark_dirty(s_canvas_layer);
}

// Touch (emery only): cycle bg corner radius 2u → 3u → 4u → 2u
// API confirmed from pebble.h:913:
//   void touch_service_subscribe(TouchServiceHandler handler, void *context);
//   typedef void (*TouchServiceHandler)(const TouchEvent *, void *);
// TouchEvent struct fields (type, x/y coords) TBD — fires on any touch for now.
// TODO: filter to TOUCH_EVENT_TYPE_DOWN once enum name is confirmed.
// Future: use event->touch.x / event->touch.y for per-digit squish easter egg.
#if defined(PBL_TOUCH)
static void touch_handler(const TouchEvent *event, void *context) {
  (void)event;    // x/y/type unused until TouchEvent fields are confirmed
  (void)context;
#if defined(PBL_PLATFORM_EMERY)
  s_radius_idx = (s_radius_idx + 1) % RADIUS_COUNT;
  layer_mark_dirty(s_canvas_layer);
#endif
}
#endif  // PBL_TOUCH

// Minute tick: trigger squish animation (wide/full layouts), or direct update (stacked)
static void tick_handler(struct tm *t, TimeUnits units) {
  bool animated_layout = (s_layout == LAYOUT_FULL   ||
                          s_layout == LAYOUT_INFO_1 ||
                          s_layout == LAYOUT_INFO_2 ||
                          s_layout == LAYOUT_INFO_3);

  if (s_phase == PHASE_DONE && animated_layout) {
    s_pending_hour = t->tm_hour; s_pending_minute = t->tm_min;
    s_digit_pending = true;
    s_phase = PHASE_SQUISH; s_going_down = true; s_overshot = false;
    schedule(ANIM_STEP_MS);
  } else if (s_phase == PHASE_SQUISH) {
    // Already squishing — queue the new time to swap at H_MIN
    s_pending_hour = t->tm_hour; s_pending_minute = t->tm_min;
    s_digit_pending = true;
  } else {
    // Stacked layouts or mid-animation: update directly, no squish
    s_hour = t->tm_hour; s_minute = t->tm_min;
    layer_mark_dirty(s_canvas_layer);
  }

#if defined(PBL_HEALTH)
  prv_update_health();
#endif
}

static void battery_handler(BatteryChargeState state) {
  s_battery_pct = state.charge_percent;
  s_charging    = state.is_charging;
  layer_mark_dirty(s_canvas_layer);
}

static void bt_handler(bool connected) {
  s_bt_connected = connected;
  layer_mark_dirty(s_canvas_layer);
}

// Weather and solar data from JS pebble-js-app
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;

  t = dict_find(iter, MESSAGE_KEY_WeatherTempF);
  if (t) snprintf(s_weather_temp, sizeof(s_weather_temp), "%dF", (int)t->value->int32);

  t = dict_find(iter, MESSAGE_KEY_WeatherTempC);
  if (t && !s_weather_temp[0])
    snprintf(s_weather_temp, sizeof(s_weather_temp), "%dC", (int)t->value->int32);

  t = dict_find(iter, MESSAGE_KEY_WeatherCode);
  if (t) {
    int c = (int)t->value->int32;
    const char *d = (c == 0)  ? "clear"  :
                    (c <= 3)  ? "cloudy" :
                    (c <= 49) ? "fog"    :
                    (c <= 69) ? "rain"   :
                    (c <= 79) ? "snow"   :
                    (c <= 99) ? "storm"  : "weather";
    snprintf(s_weather_desc, sizeof(s_weather_desc), "%s", d);
  }

  // TODO: MESSAGE_KEY_SunriseMin / MESSAGE_KEY_SunsetMin handling (stub)
  // t = dict_find(iter, MESSAGE_KEY_SunriseMin);
  // if (t) s_sunrise_min = (int)t->value->int32;

  layer_mark_dirty(s_canvas_layer);
}

// ---------------------------------------------------------------------------
// Window lifecycle
// ---------------------------------------------------------------------------
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas_layer, draw_layer);
  layer_add_child(root, s_canvas_layer);

  GRect ub = layer_get_unobstructed_bounds(root);
  s_target_h = prv_compute_target_h(ub.size.h, 0, 0);
  s_h = s_target_h;

  UnobstructedAreaHandlers ua = { .change = unobstructed_change };
  unobstructed_area_service_subscribe(ua, NULL);
  accel_tap_service_subscribe(accel_tap_handler);
#if defined(PBL_TOUCH)
  touch_service_subscribe(touch_handler, NULL);
#endif

  // Start launch countdown from 9 -> 0
  s_phase = PHASE_COUNTDOWN;
  s_countdown_digit = 9;
  s_overshot = false;
  s_cd_sub = CD_HOLD_MAX;
  layer_mark_dirty(s_canvas_layer);
  schedule(COUNTDOWN_HOLD_MS);
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

// ---------------------------------------------------------------------------
// App lifecycle
// ---------------------------------------------------------------------------
static void init(void) {
  s_fg = GColorWhite;
  s_bg = GColorBlack;

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  // Seed initial time so first frame shows correct digits before first tick
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  s_hour   = t->tm_hour;
  s_minute = t->tm_min;

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  battery_handler(battery_state_service_peek());  // prime immediately
  bluetooth_connection_service_subscribe(bt_handler);
  s_bt_connected = bluetooth_connection_service_peek();

#if defined(PBL_HEALTH)
  prv_update_health();
#endif

  // 256B inbox is sufficient for weather + solar payload (~40 bytes)
  app_message_register_inbox_received(inbox_received);
  app_message_open(256, 64);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
