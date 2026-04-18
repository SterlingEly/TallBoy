#include <pebble.h>

// ============================================================
// TallBoy — main.c  v0.9
//
// THREE LAYOUTS:
//   LAYOUT_WIDE  — HH:MM horizontal, full width (current)
//   LAYOUT_LEFT  — HH stacked over MM, left-aligned, info lines right
//   LAYOUT_RIGHT — HH stacked over MM, right-aligned, info lines left
//
// INFO LINES: date, day, steps, battery, weather, BT, charge, HR
// QUICK VIEW: vertically centered on unobstructed bounds
// OPTIONS: leading zero (default on), show colon (default on)
// ============================================================

// ---- Temporary: cycle layouts for testing ----
// 0=WIDE, 1=LEFT, 2=RIGHT. Will become a setting later.
#define LAYOUT_WIDE   0
#define LAYOUT_LEFT   1
#define LAYOUT_RIGHT  2
static int s_layout = LAYOUT_WIDE;

// ---- Options (will become settings) ----
static bool s_leading_zero = true;
static bool s_show_colon   = true;

// ---- Platform geometry ----
#if defined(PBL_PLATFORM_EMERY)
  #define SCREEN_W      200
  #define SCREEN_H      228
  #define SLOT_W         40
  #define COLON_SLOT_X   80
  #define SLOT_H_TENS    12
  #define SLOT_H_ONES    52
  #define SLOT_M_TENS   108
  #define SLOT_M_ONES   148
  #define SIDE_MARGIN    12   // outer margin = SLOT_H_TENS
  #define INFO_FONT_H    18   // GOTHIC_18 line height approx
  #define INFO_SMALL_H   14   // GOTHIC_14 line height
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
  #define INFO_FONT_H    14
  #define INFO_SMALL_H   14
#endif

// Stacked layout: size 2 digits, two rows
// outer_h at size 2: emery=88, low-res=66
#if defined(PBL_PLATFORM_EMERY)
  #define STACK_DIGIT_H   88   // outer_h for size 2
  #define STACK_UNIT       8
#else
  #define STACK_DIGIT_H   66   // 18+24*2
  #define STACK_UNIT       6
#endif
#define STACK_SIZE         2
#define STACK_TOTAL_H     (STACK_DIGIT_H * 2)

// ---- Size selection ----
static int pick_size(int available_h) {
#if defined(PBL_PLATFORM_EMERY)
  for (int s = 6; s >= 1; s--) {
    if ((24 + 32 * s) <= available_h - 6) return s;
  }
#else
  for (int s = 6; s >= 1; s--) {
    if ((18 + 24 * s) <= available_h - 6) return s;
  }
#endif
  return 1;
}

// ---- Animation sequences ----
static const int SQUISH_DOWN[] = { 5, 4, 3, 2, 1 };
static const int SQUISH_UP[]   = { 2, 3, 4, 5, 6 };
#define SQUISH_LEN 5

static const int SIZE_CYCLE[]  = { 6, 5, 4, 3, 2, 1, 2, 3, 4, 5, 6 };
#define SIZE_CYCLE_LEN 11

#define ANIM_SLOW_MS  1000
#define ANIM_FAST_MS    80
#define DEMO_TIME_MS  5000
#define DEMO_DIGIT_MS 1000

typedef enum {
  PHASE_TIME_1,
  PHASE_DIGIT_CYCLE,
  PHASE_TIME_2,
  PHASE_SIZE_SLOW,
  PHASE_SIZE_FAST,
  PHASE_DONE,
  PHASE_SQUISH,
  PHASE_SHAKE_CYCLE,
} DemoPhase;

// ---- State ----
static Window    *s_window;
static Layer     *s_canvas_layer;
static int        s_hour        = 0;
static int        s_minute      = 0;
static int        s_size        = 6;
static int        s_target_size = 6;
static GColor     s_fg, s_bg;
static DemoPhase  s_phase       = PHASE_TIME_1;
static int        s_demo_digit  = 0;
static int        s_anim_step   = 0;
static int        s_anim_rep    = 0;
static AppTimer  *s_timer       = NULL;

// ---- Data state ----
static int   s_battery_pct  = 100;
static bool  s_charging      = false;
static bool  s_bt_connected  = true;
static int   s_steps         = 0;
static int   s_heart_rate    = 0;
static char  s_weather_temp[8]  = "";
static char  s_weather_desc[32] = "";

// ---- Bitmaps ----
static GBitmap *s_bitmaps[10][6];
static GBitmap *s_colon_bm[6];

#if defined(PBL_PLATFORM_EMERY)
static const uint32_t s_res[10][6] = {
  { RESOURCE_ID_TALLBOY_01, RESOURCE_ID_TALLBOY_02, RESOURCE_ID_TALLBOY_03,
    RESOURCE_ID_TALLBOY_04, RESOURCE_ID_TALLBOY_05, RESOURCE_ID_TALLBOY_06 },
  { RESOURCE_ID_TALLBOY_11, RESOURCE_ID_TALLBOY_12, RESOURCE_ID_TALLBOY_13,
    RESOURCE_ID_TALLBOY_14, RESOURCE_ID_TALLBOY_15, RESOURCE_ID_TALLBOY_16 },
  { RESOURCE_ID_TALLBOY_21, RESOURCE_ID_TALLBOY_22, RESOURCE_ID_TALLBOY_23,
    RESOURCE_ID_TALLBOY_24, RESOURCE_ID_TALLBOY_25, RESOURCE_ID_TALLBOY_26 },
  { RESOURCE_ID_TALLBOY_31, RESOURCE_ID_TALLBOY_32, RESOURCE_ID_TALLBOY_33,
    RESOURCE_ID_TALLBOY_34, RESOURCE_ID_TALLBOY_35, RESOURCE_ID_TALLBOY_36 },
  { RESOURCE_ID_TALLBOY_41, RESOURCE_ID_TALLBOY_42, RESOURCE_ID_TALLBOY_43,
    RESOURCE_ID_TALLBOY_44, RESOURCE_ID_TALLBOY_45, RESOURCE_ID_TALLBOY_46 },
  { RESOURCE_ID_TALLBOY_51, RESOURCE_ID_TALLBOY_52, RESOURCE_ID_TALLBOY_53,
    RESOURCE_ID_TALLBOY_54, RESOURCE_ID_TALLBOY_55, RESOURCE_ID_TALLBOY_56 },
  { RESOURCE_ID_TALLBOY_61, RESOURCE_ID_TALLBOY_62, RESOURCE_ID_TALLBOY_63,
    RESOURCE_ID_TALLBOY_64, RESOURCE_ID_TALLBOY_65, RESOURCE_ID_TALLBOY_66 },
  { RESOURCE_ID_TALLBOY_71, RESOURCE_ID_TALLBOY_72, RESOURCE_ID_TALLBOY_73,
    RESOURCE_ID_TALLBOY_74, RESOURCE_ID_TALLBOY_75, RESOURCE_ID_TALLBOY_76 },
  { RESOURCE_ID_TALLBOY_81, RESOURCE_ID_TALLBOY_82, RESOURCE_ID_TALLBOY_83,
    RESOURCE_ID_TALLBOY_84, RESOURCE_ID_TALLBOY_85, RESOURCE_ID_TALLBOY_86 },
  { RESOURCE_ID_TALLBOY_91, RESOURCE_ID_TALLBOY_92, RESOURCE_ID_TALLBOY_93,
    RESOURCE_ID_TALLBOY_94, RESOURCE_ID_TALLBOY_95, RESOURCE_ID_TALLBOY_96 },
};
static const uint32_t s_colon_res[6] = {
  RESOURCE_ID_TALLBOY_COLON1, RESOURCE_ID_TALLBOY_COLON2, RESOURCE_ID_TALLBOY_COLON3,
  RESOURCE_ID_TALLBOY_COLON4, RESOURCE_ID_TALLBOY_COLON5, RESOURCE_ID_TALLBOY_COLON6,
};
#else
static const uint32_t s_res[10][6] = {
  { RESOURCE_ID_TALLBOY_L01, RESOURCE_ID_TALLBOY_L02, RESOURCE_ID_TALLBOY_L03,
    RESOURCE_ID_TALLBOY_L04, RESOURCE_ID_TALLBOY_L05, RESOURCE_ID_TALLBOY_L06 },
  { RESOURCE_ID_TALLBOY_L11, RESOURCE_ID_TALLBOY_L12, RESOURCE_ID_TALLBOY_L13,
    RESOURCE_ID_TALLBOY_L14, RESOURCE_ID_TALLBOY_L15, RESOURCE_ID_TALLBOY_L16 },
  { RESOURCE_ID_TALLBOY_L21, RESOURCE_ID_TALLBOY_L22, RESOURCE_ID_TALLBOY_L23,
    RESOURCE_ID_TALLBOY_L24, RESOURCE_ID_TALLBOY_L25, RESOURCE_ID_TALLBOY_L26 },
  { RESOURCE_ID_TALLBOY_L31, RESOURCE_ID_TALLBOY_L32, RESOURCE_ID_TALLBOY_L33,
    RESOURCE_ID_TALLBOY_L34, RESOURCE_ID_TALLBOY_L35, RESOURCE_ID_TALLBOY_L36 },
  { RESOURCE_ID_TALLBOY_L41, RESOURCE_ID_TALLBOY_L42, RESOURCE_ID_TALLBOY_L43,
    RESOURCE_ID_TALLBOY_L44, RESOURCE_ID_TALLBOY_L45, RESOURCE_ID_TALLBOY_L46 },
  { RESOURCE_ID_TALLBOY_L51, RESOURCE_ID_TALLBOY_L52, RESOURCE_ID_TALLBOY_L53,
    RESOURCE_ID_TALLBOY_L54, RESOURCE_ID_TALLBOY_L55, RESOURCE_ID_TALLBOY_L56 },
  { RESOURCE_ID_TALLBOY_L61, RESOURCE_ID_TALLBOY_L62, RESOURCE_ID_TALLBOY_L63,
    RESOURCE_ID_TALLBOY_L64, RESOURCE_ID_TALLBOY_L65, RESOURCE_ID_TALLBOY_L66 },
  { RESOURCE_ID_TALLBOY_L71, RESOURCE_ID_TALLBOY_L72, RESOURCE_ID_TALLBOY_L73,
    RESOURCE_ID_TALLBOY_L74, RESOURCE_ID_TALLBOY_L75, RESOURCE_ID_TALLBOY_L76 },
  { RESOURCE_ID_TALLBOY_L81, RESOURCE_ID_TALLBOY_L82, RESOURCE_ID_TALLBOY_L83,
    RESOURCE_ID_TALLBOY_L84, RESOURCE_ID_TALLBOY_L85, RESOURCE_ID_TALLBOY_L86 },
  { RESOURCE_ID_TALLBOY_L91, RESOURCE_ID_TALLBOY_L92, RESOURCE_ID_TALLBOY_L93,
    RESOURCE_ID_TALLBOY_L94, RESOURCE_ID_TALLBOY_L95, RESOURCE_ID_TALLBOY_L96 },
};
static const uint32_t s_colon_res[6] = {
  RESOURCE_ID_TALLBOY_LCOLON1, RESOURCE_ID_TALLBOY_LCOLON2, RESOURCE_ID_TALLBOY_LCOLON3,
  RESOURCE_ID_TALLBOY_LCOLON4, RESOURCE_ID_TALLBOY_LCOLON5, RESOURCE_ID_TALLBOY_LCOLON6,
};
#endif

static uint32_t find_res(int digit, int size) {
  int si = size - 1;
  if (s_res[digit][si]) return s_res[digit][si];
  for (int i = si + 1; i < 6; i++) if (s_res[digit][i]) return s_res[digit][i];
  for (int i = si - 1; i >= 0; i--) if (s_res[digit][i]) return s_res[digit][i];
  return 0;
}

static GBitmap *get_bitmap(int digit, int size) {
  int si = size - 1;
  if (!s_bitmaps[digit][si]) {
    uint32_t res = find_res(digit, size);
    if (res) s_bitmaps[digit][si] = gbitmap_create_with_resource(res);
  }
  return s_bitmaps[digit][si];
}

static GBitmap *get_colon(int size) {
  int si = size - 1;
  if (!s_colon_bm[si])
    s_colon_bm[si] = gbitmap_create_with_resource(s_colon_res[si]);
  return s_colon_bm[si];
}

static void free_bitmaps(void) {
  for (int d = 0; d < 10; d++)
    for (int s = 0; s < 6; s++)
      if (s_bitmaps[d][s]) { gbitmap_destroy(s_bitmaps[d][s]); s_bitmaps[d][s] = NULL; }
  for (int s = 0; s < 6; s++)
    if (s_colon_bm[s]) { gbitmap_destroy(s_colon_bm[s]); s_colon_bm[s] = NULL; }
}

// ---- Bitmap draw helpers ----
// Blit a digit at (slot_x, y_offset). y_offset lets stacked layout shift rows.
// In LAYOUT_WIDE, files are full SCREEN_H tall and self-centered — y_offset=0,
// height=SCREEN_H. In stacked layouts, we blit just the STACK_DIGIT_H portion
// centered vertically within the file, at the correct y position.
static void draw_digit_at(GContext *ctx, int digit, int size, int slot_x, int y, int h) {
  GBitmap *bm = get_bitmap(digit, size);
  if (!bm) return;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, bm, GRect(slot_x, y, SLOT_W, h));
}

static void draw_digit(GContext *ctx, int digit, int size, int slot_x) {
  draw_digit_at(ctx, digit, size, slot_x, 0, SCREEN_H);
}

static void draw_colon_bm(GContext *ctx, int size) {
  GBitmap *bm = get_colon(size);
  if (!bm) return;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, bm, GRect(COLON_SLOT_X, 0, SLOT_W, SCREEN_H));
}

// ============================================================
// INFO LINES
// Build a list of strings from live data, render into a rect.
// ============================================================
static const char *s_day_names[] = {
  "SUN","MON","TUE","WED","THU","FRI","SAT"
};
static const char *s_month_abbrs[] = {
  "JAN","FEB","MAR","APR","MAY","JUN",
  "JUL","AUG","SEP","OCT","NOV","DEC"
};

// Fill up to max_lines info strings into lines[]. Returns count filled.
static int build_info_lines(char lines[][32], int max_lines, struct tm *t) {
  int n = 0;
  if (n < max_lines && t) {
    snprintf(lines[n++], 32, "%s %s %d",
             s_day_names[t->tm_wday],
             s_month_abbrs[t->tm_mon],
             t->tm_mday);
  }
  if (n < max_lines) {
    snprintf(lines[n++], 32, "BAT %d%%%s",
             s_battery_pct,
             s_charging ? " CHG" : "");
  }
#if defined(PBL_HEALTH)
  if (n < max_lines) {
    snprintf(lines[n++], 32, "%d STEPS", s_steps);
  }
  if (n < max_lines && s_heart_rate > 0) {
    snprintf(lines[n++], 32, "%d BPM", s_heart_rate);
  }
#endif
  if (n < max_lines && !s_bt_connected) {
    snprintf(lines[n++], 32, "BT OFF");
  }
  if (n < max_lines && s_weather_temp[0]) {
    snprintf(lines[n++], 32, "%s", s_weather_temp);
  }
  if (n < max_lines && s_weather_desc[0]) {
    snprintf(lines[n++], 32, "%s", s_weather_desc);
  }
  return n;
}

static void draw_info_lines(GContext *ctx, GRect area, int max_lines, struct tm *t) {
  char lines[8][32];
  int n = build_info_lines(lines, max_lines < 8 ? max_lines : 8, t);
  if (n == 0) return;

  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  int line_h = INFO_SMALL_H + 2;
  int total_h = n * line_h;
  int start_y = area.origin.y + (area.size.h - total_h) / 2;

  graphics_context_set_text_color(ctx, s_fg);
  for (int i = 0; i < n; i++) {
    GRect r = GRect(area.origin.x, start_y + i * line_h,
                    area.size.w, line_h + 2);
    graphics_draw_text(ctx, lines[i], font, r,
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
  }
}

// ============================================================
// DRAW LAYER
// ============================================================
static void draw_layer(Layer *layer, GContext *ctx) {
  // Use unobstructed bounds for all centering calculations
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  int ub_top = ub.origin.y;
  int ub_h   = ub.size.h;

  graphics_context_set_fill_color(ctx, s_bg);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  // Determine digits to show
  int h_tens, h_ones, m_tens, m_ones;
  if (s_phase == PHASE_DIGIT_CYCLE) {
    h_tens = h_ones = m_tens = m_ones = s_demo_digit;
  } else {
    int h = s_hour % 12;
    if (h == 0) h = 12;
    h_tens = h / 10;
    h_ones = h % 10;
    m_tens = s_minute / 10;
    m_ones = s_minute % 10;
  }

  // For leading zero suppression: if h_tens == 0, skip it
  bool draw_h_tens = s_leading_zero || (h_tens != 0);

  // Get current time struct for info lines (only needed after demo)
  time_t now_t = time(NULL);
  struct tm *now_tm = (s_phase == PHASE_DONE) ? localtime(&now_t) : NULL;

  if (s_layout == LAYOUT_WIDE) {
    // ---- LAYOUT_WIDE: horizontal HH:MM centered on unobstructed area ----
    int size = s_size;

    // Vertical offset: shift all bitmaps so the content reads as centered in ub.
    // The bitmaps are 228px tall and self-center within that height at each size.
    // outer_h(size) is the visible digit height. We want its center at ub center.
    // Content center in file: SCREEN_H/2. Desired center: ub_top + ub_h/2.
    // So we blit at y_offset = (ub_top + ub_h/2) - SCREEN_H/2.
    int y_off = ub_top + ub_h / 2 - SCREEN_H / 2;

    if (draw_h_tens) {
      GBitmap *bm = get_bitmap(h_tens, size);
      if (bm) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, bm,
          GRect(SLOT_H_TENS, y_off, SLOT_W, SCREEN_H));
      }
    }
    {
      GBitmap *bm = get_bitmap(h_ones, size);
      if (bm) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, bm,
          GRect(SLOT_H_ONES, y_off, SLOT_W, SCREEN_H));
      }
    }
    if (s_show_colon) {
      GBitmap *bm = get_colon(size);
      if (bm) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, bm,
          GRect(COLON_SLOT_X, y_off, SLOT_W, SCREEN_H));
      }
    }
    {
      GBitmap *bm = get_bitmap(m_tens, size);
      if (bm) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, bm,
          GRect(SLOT_M_TENS, y_off, SLOT_W, SCREEN_H));
      }
    }
    {
      GBitmap *bm = get_bitmap(m_ones, size);
      if (bm) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, bm,
          GRect(SLOT_M_ONES, y_off, SLOT_W, SCREEN_H));
      }
    }

  } else {
    // ---- LAYOUT_LEFT / LAYOUT_RIGHT: stacked HH over MM at size 2 ----
    // Two digit slots side-by-side, stacked vertically.
    // Total height = STACK_TOTAL_H = 2 * STACK_DIGIT_H.
    // Vertically centered within unobstructed area.
    //
    // Each digit file is SCREEN_H tall with content centered.
    // At size 2, outer_h = STACK_DIGIT_H. Content center in file = SCREEN_H/2.
    // For top row: we want its center at (ub_top + ub_h/2 - STACK_DIGIT_H/2).
    // For bottom row: center at (ub_top + ub_h/2 + STACK_DIGIT_H/2).
    // Blit y = desired_center - SCREEN_H/2.

    int center_y = ub_top + ub_h / 2;
    // Top digit (hours) blit offset
    int top_y = center_y - STACK_DIGIT_H / 2 - STACK_DIGIT_H - SCREEN_H / 2;
    // Wait — think again. File height = SCREEN_H. Digit is centered at SCREEN_H/2 within file.
    // We want digit center at: center_y - STACK_DIGIT_H/2 (top half center)
    // So: file_top + SCREEN_H/2 = center_y - STACK_DIGIT_H/2
    //     file_top = center_y - STACK_DIGIT_H/2 - SCREEN_H/2
    int h_file_y = center_y - STACK_DIGIT_H / 2 - SCREEN_H / 2;
    // Bottom digit (minutes) center at: center_y + STACK_DIGIT_H/2
    int m_file_y = center_y + STACK_DIGIT_H / 2 - SCREEN_H / 2;
    (void)top_y;  // suppress warning

    // Digit x positions depend on left/right alignment
    int tens_x, ones_x;
    if (s_layout == LAYOUT_LEFT) {
      tens_x = SIDE_MARGIN;
      ones_x = SIDE_MARGIN + SLOT_W;
    } else {
      // LAYOUT_RIGHT: right-align. ones at right, tens to left of ones.
      ones_x = SCREEN_W - SIDE_MARGIN - SLOT_W;
      tens_x = ones_x - SLOT_W;
    }

    // Info area: the other side
    int info_x, info_w;
    if (s_layout == LAYOUT_LEFT) {
      info_x = SIDE_MARGIN + SLOT_W * 2 + SIDE_MARGIN;
      info_w = SCREEN_W - info_x - SIDE_MARGIN;
    } else {
      info_x = SIDE_MARGIN;
      info_w = tens_x - SIDE_MARGIN * 2;
    }

    // Draw hours (top row)
    if (draw_h_tens) {
      GBitmap *bm = get_bitmap(h_tens, STACK_SIZE);
      if (bm) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, bm, GRect(tens_x, h_file_y, SLOT_W, SCREEN_H));
      }
    }
    {
      GBitmap *bm = get_bitmap(h_ones, STACK_SIZE);
      if (bm) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, bm, GRect(ones_x, h_file_y, SLOT_W, SCREEN_H));
      }
    }

    // Draw minutes (bottom row)
    {
      GBitmap *bm = get_bitmap(m_tens, STACK_SIZE);
      if (bm) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, bm, GRect(tens_x, m_file_y, SLOT_W, SCREEN_H));
      }
    }
    {
      GBitmap *bm = get_bitmap(m_ones, STACK_SIZE);
      if (bm) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, bm, GRect(ones_x, m_file_y, SLOT_W, SCREEN_H));
      }
    }

    // Draw info lines in the opposite zone
    if (now_tm && info_w > 20) {
      GRect info_area = GRect(info_x, ub_top, info_w, ub_h);
      // Fit as many lines as space allows
      int max_lines = ub_h / (INFO_SMALL_H + 2);
      draw_info_lines(ctx, info_area, max_lines, now_tm);
    }
  }
}

// ============================================================
// TIMER
// ============================================================
static void timer_cb(void *data);

static void schedule(uint32_t ms) {
  if (s_timer) app_timer_cancel(s_timer);
  s_timer = app_timer_register(ms, timer_cb, NULL);
}

static void timer_cb(void *data) {
  s_timer = NULL;
  switch (s_phase) {

    case PHASE_TIME_1:
      s_phase = PHASE_DIGIT_CYCLE;
      s_demo_digit = 0;
      layer_mark_dirty(s_canvas_layer);
      schedule(DEMO_DIGIT_MS);
      break;

    case PHASE_DIGIT_CYCLE:
      s_demo_digit++;
      if (s_demo_digit < 10) {
        layer_mark_dirty(s_canvas_layer);
        schedule(DEMO_DIGIT_MS);
      } else {
        s_phase = PHASE_TIME_2;
        s_size = 6;
        layer_mark_dirty(s_canvas_layer);
        schedule(DEMO_TIME_MS);
      }
      break;

    case PHASE_TIME_2:
      s_phase = PHASE_SIZE_SLOW;
      s_anim_step = 0; s_anim_rep = 0;
      s_size = SIZE_CYCLE[0];
      layer_mark_dirty(s_canvas_layer);
      schedule(ANIM_SLOW_MS);
      break;

    case PHASE_SIZE_SLOW:
      s_anim_step++;
      if (s_anim_step < SIZE_CYCLE_LEN) {
        s_size = SIZE_CYCLE[s_anim_step];
        layer_mark_dirty(s_canvas_layer);
        schedule(ANIM_SLOW_MS);
      } else {
        s_anim_rep++;
        if (s_anim_rep < 2) {
          s_anim_step = 0;
          s_size = SIZE_CYCLE[0];
          layer_mark_dirty(s_canvas_layer);
          schedule(ANIM_SLOW_MS);
        } else {
          s_phase = PHASE_SIZE_FAST;
          s_anim_step = 0; s_anim_rep = 0;
          s_size = SIZE_CYCLE[0];
          layer_mark_dirty(s_canvas_layer);
          schedule(ANIM_FAST_MS);
        }
      }
      break;

    case PHASE_SIZE_FAST:
      s_anim_step++;
      if (s_anim_step < SIZE_CYCLE_LEN) {
        s_size = SIZE_CYCLE[s_anim_step];
        layer_mark_dirty(s_canvas_layer);
        schedule(ANIM_FAST_MS);
      } else {
        s_anim_rep++;
        if (s_anim_rep < 2) {
          s_anim_step = 0;
          s_size = SIZE_CYCLE[0];
          layer_mark_dirty(s_canvas_layer);
          schedule(ANIM_FAST_MS);
        } else {
          s_phase = PHASE_DONE;
          s_size = s_target_size;
          layer_mark_dirty(s_canvas_layer);
        }
      }
      break;

    case PHASE_SQUISH:
      s_anim_step++;
      if (s_anim_step < SQUISH_LEN) {
        s_size = SQUISH_DOWN[s_anim_step];
        layer_mark_dirty(s_canvas_layer);
        schedule(ANIM_FAST_MS);
      } else {
        int up_step = s_anim_step - SQUISH_LEN;
        if (up_step < SQUISH_LEN) {
          int proposed = SQUISH_UP[up_step];
          s_size = (proposed > s_target_size) ? s_target_size : proposed;
          layer_mark_dirty(s_canvas_layer);
          schedule(ANIM_FAST_MS);
        } else {
          s_phase = PHASE_DONE;
          s_size = s_target_size;
          layer_mark_dirty(s_canvas_layer);
        }
      }
      break;

    case PHASE_SHAKE_CYCLE:
      s_anim_step++;
      if (s_anim_step < SIZE_CYCLE_LEN) {
        s_size = SIZE_CYCLE[s_anim_step];
        layer_mark_dirty(s_canvas_layer);
        schedule(ANIM_FAST_MS);
      } else {
        s_anim_rep++;
        if (s_anim_rep < 2) {
          s_anim_step = 0;
          s_size = SIZE_CYCLE[0];
          layer_mark_dirty(s_canvas_layer);
          schedule(ANIM_FAST_MS);
        } else {
          s_phase = PHASE_DONE;
          s_size = s_target_size;
          layer_mark_dirty(s_canvas_layer);
        }
      }
      break;

    case PHASE_DONE:
      break;
  }
}

// ============================================================
// EVENT HANDLERS
// ============================================================

// Quick View
static void unobstructed_change(AnimationProgress progress, void *ctx) {
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  s_target_size = pick_size(ub.size.h);
  if (s_phase == PHASE_DONE) {
    s_size = s_target_size;
    layer_mark_dirty(s_canvas_layer);
  }
}

// Shake: cycle layouts after demo, or trigger animation
static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_phase != PHASE_DONE) return;
  // Advance layout on shake
  s_layout = (s_layout + 1) % 3;
  // Also do the shake animation on LAYOUT_WIDE
  if (s_layout == LAYOUT_WIDE) {
    s_phase = PHASE_SHAKE_CYCLE;
    s_anim_step = 0; s_anim_rep = 0;
    s_size = SIZE_CYCLE[0];
    schedule(ANIM_FAST_MS);
  }
  layer_mark_dirty(s_canvas_layer);
}

// Tick
static void tick_handler(struct tm *t, TimeUnits units) {
  s_hour   = t->tm_hour;
  s_minute = t->tm_min;
  if (s_phase == PHASE_DONE && s_layout == LAYOUT_WIDE) {
    s_phase = PHASE_SQUISH;
    s_anim_step = 0;
    s_size = SQUISH_DOWN[0];
    schedule(ANIM_FAST_MS);
  }
  layer_mark_dirty(s_canvas_layer);
}

// Battery
static void battery_handler(BatteryChargeState state) {
  s_battery_pct = state.charge_percent;
  s_charging    = state.is_charging;
  layer_mark_dirty(s_canvas_layer);
}

// Bluetooth
static void bt_handler(bool connected) {
  s_bt_connected = connected;
  layer_mark_dirty(s_canvas_layer);
}

// Health
#if defined(PBL_HEALTH)
static void health_handler(HealthEventType event, void *context) {
  if (event == HealthEventMovementUpdate || event == HealthEventSignificantUpdate) {
    HealthServiceAccessibilityMask mask = health_service_metric_accessible(
      HealthMetricStepCount, time_start_of_today(), time(NULL));
    s_steps = (mask & HealthServiceAccessibilityMaskAvailable)
      ? (int)health_service_sum_today(HealthMetricStepCount) : 0;
  }
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_DIORITE)
  if (event == HealthEventHeartRateUpdate) {
    HealthServiceAccessibilityMask mask = health_service_metric_accessible(
      HealthMetricHeartRateBPM, time(NULL), time(NULL) + 1);
    s_heart_rate = (mask & HealthServiceAccessibilityMaskAvailable)
      ? (int)health_service_peek_current_value(HealthMetricHeartRateBPM) : 0;
  }
#endif
  layer_mark_dirty(s_canvas_layer);
}
#endif

// App message (weather from phone)
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;
  t = dict_find(iter, MESSAGE_KEY_WeatherTempF);
  if (t) snprintf(s_weather_temp, sizeof(s_weather_temp), "%dF", (int)t->value->int32);
  t = dict_find(iter, MESSAGE_KEY_WeatherTempC);
  if (t && !s_weather_temp[0])
    snprintf(s_weather_temp, sizeof(s_weather_temp), "%dC", (int)t->value->int32);
  t = dict_find(iter, MESSAGE_KEY_WeatherCode);
  if (t) {
    int code = (int)t->value->int32;
    // Simple weather description from WMO code ranges
    const char *desc = "WEATHER";
    if (code == 0)                       desc = "CLEAR";
    else if (code <= 3)                  desc = "CLOUDY";
    else if (code <= 49)                 desc = "FOG";
    else if (code <= 69)                 desc = "RAIN";
    else if (code <= 79)                 desc = "SNOW";
    else if (code <= 99)                 desc = "STORM";
    snprintf(s_weather_desc, sizeof(s_weather_desc), "%s", desc);
  }
  layer_mark_dirty(s_canvas_layer);
}

// ============================================================
// WINDOW / APP LIFECYCLE
// ============================================================
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas_layer, draw_layer);
  layer_add_child(root, s_canvas_layer);

  GRect ub = layer_get_unobstructed_bounds(root);
  s_target_size = pick_size(ub.size.h);
  s_size = 6;

  UnobstructedAreaHandlers ua_handlers = { .change = unobstructed_change };
  unobstructed_area_service_subscribe(ua_handlers, NULL);
  accel_tap_service_subscribe(accel_tap_handler);

  schedule(DEMO_TIME_MS);
}

static void window_unload(Window *window) {
  unobstructed_area_service_unsubscribe();
  accel_tap_service_unsubscribe();
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  free_bitmaps();
  layer_destroy(s_canvas_layer);
}

static void init(void) {
  s_fg = GColorWhite;
  s_bg = GColorBlack;
  memset(s_bitmaps,  0, sizeof(s_bitmaps));
  memset(s_colon_bm, 0, sizeof(s_colon_bm));

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  s_hour   = t->tm_hour;
  s_minute = t->tm_min;

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  battery_handler(battery_state_service_peek());
  bluetooth_connection_service_subscribe(bt_handler);
  s_bt_connected = bluetooth_connection_service_peek();
#if defined(PBL_HEALTH)
  health_service_events_subscribe(health_handler, NULL);
  health_handler(HealthEventSignificantUpdate, NULL);
#endif
  app_message_register_inbox_received(inbox_received);
  app_message_open(256, 64);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
#if defined(PBL_HEALTH)
  health_service_events_unsubscribe();
#endif
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
