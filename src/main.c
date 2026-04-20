#include <pebble.h>

// ============================================================
// TallBoy — main.c  v1.2
//
// Fixes from v1.1:
//   - Leading zero + colon: always drawn (no toggle), colon fallback
//     searches adjacent sizes so a missing resource never blanks it
//   - Boot: s_target_size protected from underflow; blink always ends at 6
//   - Left stack: info column rect corrected (was painting full-screen)
//   - Info font: GOTHIC_18_BOLD on all platforms for stacked column too
// ============================================================

#define LAYOUT_WIDE        0
#define LAYOUT_WIDE_COMPS  1
#define LAYOUT_LEFT        2
#define LAYOUT_RIGHT       3
#define LAYOUT_COUNT       4
static int s_layout = LAYOUT_WIDE;

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
  #define SIDE_MARGIN    12
  #define HALF_UNIT       4
  #define STACK_H_SZ1    56
  #define STACK_H_SZ2    88
  #define STACK_SZ2_MIN 190
  #define INFO_FONT_H    18
  #define INFO_LINE_H    20
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
  #define STACK_H_SZ1    42
  #define STACK_H_SZ2    66
  #define STACK_SZ2_MIN 146
  #define INFO_FONT_H    18
  #define INFO_LINE_H    20
#endif

#define WIDE_COMP_LINES 3
#define WIDE_COMP_SIZE  2

static int s_stack_size = 2;

static int pick_stack_size(int ub_h) {
  return (ub_h >= STACK_SZ2_MIN) ? 2 : 1;
}

static int stack_digit_h(int sz) {
  return (sz == 2) ? STACK_H_SZ2 : STACK_H_SZ1;
}

static int pick_size(int available_h) {
#if defined(PBL_PLATFORM_EMERY)
  for (int s = 6; s >= 1; s--)
    if ((24 + 32 * s) <= available_h - 6) return s;
#else
  for (int s = 6; s >= 1; s--)
    if ((18 + 24 * s) <= available_h - 6) return s;
#endif
  return 1;
}

static int digit_outer_h(int sz) {
#if defined(PBL_PLATFORM_EMERY)
  return 24 + 32 * sz;
#else
  return 18 + 24 * sz;
#endif
}

// ---- Animation ----
static const int GROW[]   = { 1, 2, 3, 4, 5, 6 };
static const int SHRINK[] = { 5, 4, 3, 2, 1 };
#define GROW_LEN    6
#define SHRINK_LEN  5
#define COUNTDOWN_FRAMES (GROW_LEN + SHRINK_LEN)
#define COUNTDOWN_MS  55
#define BLINK_REPS    2
#define ANIM_FAST_MS  80
#define WIDE_FULL_SIZE 6  // size LAYOUT_WIDE settles at after animations

static const int SIZE_CYCLE[] = { 6, 5, 4, 3, 2, 1, 2, 3, 4, 5, 6 };
#define SIZE_CYCLE_LEN 11

typedef enum {
  PHASE_COUNTDOWN,
  PHASE_BLINK,
  PHASE_DONE,
  PHASE_SQUISH,
  PHASE_SHAKE_CYCLE,
} Phase;

// ---- State ----
static Window    *s_window;
static Layer     *s_canvas_layer;
static int        s_hour        = 0;
static int        s_minute      = 0;
static int        s_size        = 6;
static int        s_target_size = 6;
static GColor     s_fg, s_bg;
static Phase      s_phase           = PHASE_COUNTDOWN;
static int        s_anim_step       = 0;
static int        s_anim_rep        = 0;
static bool       s_going_down      = true;
static int        s_countdown_digit = 9;
static AppTimer  *s_timer           = NULL;

static bool  s_demo_override = false;
static int   s_demo_digit    = 9;
static int   s_demo_size     = 1;

static bool  s_digit_pending  = false;
static int   s_pending_hour   = 0;
static int   s_pending_minute = 0;

// ---- Data state ----
static int   s_battery_pct      = 100;
static bool  s_charging         = false;
static bool  s_bt_connected     = true;
static int   s_steps            = 0;
static int   s_distance_m       = 0;
static int   s_heart_rate       = 0;
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

// get_colon: tries requested size, then falls back to adjacent sizes so a
// single missing resource never blanks the colon entirely.
static GBitmap *get_colon(int size) {
  int si = size - 1;
  if (!s_colon_bm[si]) {
    // Try requested size first
    if (s_colon_res[si])
      s_colon_bm[si] = gbitmap_create_with_resource(s_colon_res[si]);
    // If still null, search for any loaded/loadable colon
    if (!s_colon_bm[si]) {
      for (int i = 0; i < 6; i++) {
        if (i == si) continue;
        if (!s_colon_bm[i] && s_colon_res[i])
          s_colon_bm[i] = gbitmap_create_with_resource(s_colon_res[i]);
        if (s_colon_bm[i]) { return s_colon_bm[i]; }
      }
    }
  }
  return s_colon_bm[si];
}

static void free_bitmaps(void) {
  for (int d = 0; d < 10; d++)
    for (int s = 0; s < 6; s++)
      if (s_bitmaps[d][s]) { gbitmap_destroy(s_bitmaps[d][s]); s_bitmaps[d][s] = NULL; }
  for (int s = 0; s < 6; s++)
    if (s_colon_bm[s]) { gbitmap_destroy(s_colon_bm[s]); s_colon_bm[s] = NULL; }
}

static void blit(GContext *ctx, GBitmap *bm, int x, int y) {
  if (!bm) return;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, bm, GRect(x, y, SLOT_W, SCREEN_H));
}

// ============================================================
// INFO LINES
// ============================================================
static const char *s_day_names[]   = { "SUN","MON","TUE","WED","THU","FRI","SAT" };
static const char *s_month_abbrs[] = {
  "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"
};

static int build_info_lines(char lines[][32], int max_lines, struct tm *t) {
  int n = 0;
  if (n < max_lines && t)
    snprintf(lines[n++], 32, "%s %s %d",
             s_day_names[t->tm_wday], s_month_abbrs[t->tm_mon], t->tm_mday);
  if (n < max_lines && s_weather_temp[0] && s_weather_desc[0])
    snprintf(lines[n++], 32, "%s %s", s_weather_temp, s_weather_desc);
  else if (n < max_lines && s_weather_temp[0])
    snprintf(lines[n++], 32, "%s", s_weather_temp);
#if defined(PBL_HEALTH)
  if (n < max_lines) {
    if (s_steps >= 1000)
      snprintf(lines[n++], 32, "%d,%03d steps", s_steps / 1000, s_steps % 1000);
    else
      snprintf(lines[n++], 32, "%d steps", s_steps);
  }
  if (n < max_lines && s_distance_m > 0) {
    if (strcmp(i18n_get_system_locale(), "en_US") == 0) {
      int miles_x10 = (s_distance_m * 10) / 1609;
      snprintf(lines[n++], 32, "%d.%d mi", miles_x10 / 10, miles_x10 % 10);
    } else {
      int km_x10 = (s_distance_m * 10) / 1000;
      snprintf(lines[n++], 32, "%d.%d km", km_x10 / 10, km_x10 % 10);
    }
  }
  if (n < max_lines && s_heart_rate > 0)
    snprintf(lines[n++], 32, "%d bpm", s_heart_rate);
#endif
  if (n < max_lines)
    snprintf(lines[n++], 32, "bat %d%%%s", s_battery_pct, s_charging ? " chg" : "");
  if (n < max_lines && !s_bt_connected)
    snprintf(lines[n++], 32, "bt off");
  return n;
}

// GOTHIC_18_BOLD on both platforms (looks better in stacked column too)
static GFont prv_info_font(void) {
  return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
}

// Draw n lines from a lines array, centered horizontally in [x, x+width],
// starting at start_y, spaced by INFO_LINE_H.
static void draw_info_block(GContext *ctx,
                            int x, int start_y, int width,
                            char lines[][32], int n) {
  if (n == 0) return;
  GFont font = prv_info_font();
  graphics_context_set_text_color(ctx, s_fg);
  for (int i = 0; i < n; i++) {
    GRect r = GRect(x, start_y + i * INFO_LINE_H, width, INFO_FONT_H + 4);
    graphics_draw_text(ctx, lines[i], font, r,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

// Draw info lines into a side column, vertically centered within area.
static void draw_info_column(GContext *ctx, GRect area, struct tm *t) {
  char lines[8][32];
  int max_fit = area.size.h / INFO_LINE_H;
  int n = build_info_lines(lines, max_fit < 8 ? max_fit : 8, t);
  if (n == 0) return;
  int total_h = n * INFO_LINE_H;
  int start_y = area.origin.y + (area.size.h - total_h) / 2;
  // x and width from area — keeps text inside the column, not full-screen
  draw_info_block(ctx, area.origin.x, start_y, area.size.w, lines, n);
}

// ============================================================
// DRAW LAYER
// ============================================================
static void draw_layer(Layer *layer, GContext *ctx) {
  Layer *root  = window_get_root_layer(s_window);
  GRect ub     = layer_get_unobstructed_bounds(root);
  int ub_top   = ub.origin.y;
  int ub_h     = ub.size.h;
  int center_y = ub_top + ub_h / 2;

  graphics_context_set_fill_color(ctx, s_bg);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  int h_tens, h_ones, m_tens, m_ones, size;
  if (s_demo_override) {
    h_tens = h_ones = m_tens = m_ones = s_demo_digit;
    size = s_demo_size;
  } else {
    int h  = s_hour % 12;
    if (h == 0) h = 12;
    h_tens = h / 10;
    h_ones = h % 10;
    m_tens = s_minute / 10;
    m_ones = s_minute % 10;
    size   = s_size;
  }

  // Always draw leading zero; suppress colon only during countdown
  bool draw_colon = (s_phase != PHASE_COUNTDOWN) && !s_demo_override;

  time_t now_t  = time(NULL);
  struct tm *tm = (s_phase == PHASE_DONE) ? localtime(&now_t) : NULL;

  if (s_layout == LAYOUT_WIDE) {
    int y = center_y - SCREEN_H / 2;
    blit(ctx, get_bitmap(h_tens, size), SLOT_H_TENS, y);
    blit(ctx, get_bitmap(h_ones, size), SLOT_H_ONES, y);
    if (draw_colon) blit(ctx, get_colon(size), COLON_SLOT_X, y);
    blit(ctx, get_bitmap(m_tens, size), SLOT_M_TENS, y);
    blit(ctx, get_bitmap(m_ones, size), SLOT_M_ONES, y);

  } else if (s_layout == LAYOUT_WIDE_COMPS) {
    int digit_h = digit_outer_h(size);
    int y = center_y - SCREEN_H / 2;

    blit(ctx, get_bitmap(h_tens, size), SLOT_H_TENS, y);
    blit(ctx, get_bitmap(h_ones, size), SLOT_H_ONES, y);
    if (draw_colon) blit(ctx, get_colon(size), COLON_SLOT_X, y);
    blit(ctx, get_bitmap(m_tens, size), SLOT_M_TENS, y);
    blit(ctx, get_bitmap(m_ones, size), SLOT_M_ONES, y);

    if (tm) {
      char lines[8][32];
      int n = build_info_lines(lines, 8, tm);
      int above = (n < WIDE_COMP_LINES) ? n : WIDE_COMP_LINES;
      int below = (n - above < WIDE_COMP_LINES) ? n - above : WIDE_COMP_LINES;

      int digit_top = center_y - digit_h / 2;
      int digit_bot = center_y + digit_h / 2;

      if (above > 0) {
        int block_h   = above * INFO_LINE_H;
        int start_above = digit_top - block_h - 4;
        draw_info_block(ctx, 0, start_above, SCREEN_W, lines, above);
      }
      if (below > 0) {
        draw_info_block(ctx, 0, digit_bot + 4, SCREEN_W, lines + above, below);
      }
    }

  } else {
    // LAYOUT_LEFT / LAYOUT_RIGHT
    int sdh = stack_digit_h(s_stack_size);
    int h_y = (center_y - sdh / 2 - HALF_UNIT) - SCREEN_H / 2;
    int m_y = (center_y + sdh / 2 + HALF_UNIT) - SCREEN_H / 2;

    int tens_x, ones_x, info_x, info_w;
    if (s_layout == LAYOUT_LEFT) {
      tens_x = SIDE_MARGIN;
      ones_x = SIDE_MARGIN + SLOT_W;
      info_x = SIDE_MARGIN + SLOT_W * 2 + SIDE_MARGIN;
      info_w = SCREEN_W - info_x - SIDE_MARGIN;
    } else {
      ones_x = SCREEN_W - SIDE_MARGIN - SLOT_W;
      tens_x = ones_x - SLOT_W;
      info_x = SIDE_MARGIN;
      info_w = tens_x - SIDE_MARGIN * 2;
    }

    blit(ctx, get_bitmap(h_tens, s_stack_size), tens_x, h_y);
    blit(ctx, get_bitmap(h_ones, s_stack_size), ones_x, h_y);
    blit(ctx, get_bitmap(m_tens, s_stack_size), tens_x, m_y);
    blit(ctx, get_bitmap(m_ones, s_stack_size), ones_x, m_y);

    if (tm && info_w > 20)
      draw_info_column(ctx, GRect(info_x, ub_top, info_w, ub_h), tm);
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

    case PHASE_COUNTDOWN: {
      int frame = s_anim_step % COUNTDOWN_FRAMES;
      s_demo_size     = (frame < GROW_LEN) ? GROW[frame] : SHRINK[frame - GROW_LEN];
      s_demo_digit    = s_countdown_digit;
      s_demo_override = true;
      layer_mark_dirty(s_canvas_layer);
      s_anim_step++;
      if (s_anim_step >= COUNTDOWN_FRAMES) {
        s_anim_step = 0;
        if (s_countdown_digit > 0) {
          s_countdown_digit--;
          schedule(COUNTDOWN_MS);
        } else {
          s_demo_override = false;
          s_size       = 6;
          s_going_down = true;
          s_anim_rep   = 0;
          s_phase      = PHASE_BLINK;
          layer_mark_dirty(s_canvas_layer);
          schedule(ANIM_FAST_MS);
        }
      } else {
        schedule(COUNTDOWN_MS);
      }
      break;
    }

    case PHASE_BLINK:
      if (s_going_down) {
        s_size--;
        layer_mark_dirty(s_canvas_layer);
        if (s_size <= 1) { s_size = 1; s_going_down = false; }
        schedule(ANIM_FAST_MS);
      } else {
        s_size++;
        layer_mark_dirty(s_canvas_layer);
        if (s_size >= WIDE_FULL_SIZE) {
          s_size = WIDE_FULL_SIZE;
          s_anim_rep++;
          if (s_anim_rep < BLINK_REPS) {
            s_going_down = true;
            schedule(ANIM_FAST_MS);
          } else {
            // Boot complete: settle at full size, set target for future squish
            s_target_size = WIDE_FULL_SIZE;
            s_phase = PHASE_DONE;
            layer_mark_dirty(s_canvas_layer);
          }
        } else {
          schedule(ANIM_FAST_MS);
        }
      }
      break;

    case PHASE_SQUISH:
      if (s_going_down) {
        s_size--;
        layer_mark_dirty(s_canvas_layer);
        if (s_size <= 1) {
          s_size = 1;
          if (s_digit_pending) {
            s_hour = s_pending_hour; s_minute = s_pending_minute;
            s_digit_pending = false;
          }
          s_going_down = false;
        }
        schedule(ANIM_FAST_MS);
      } else {
        s_size++;
        layer_mark_dirty(s_canvas_layer);
        if (s_size >= s_target_size) {
          s_size = s_target_size; s_phase = PHASE_DONE;
          layer_mark_dirty(s_canvas_layer);
        } else {
          schedule(ANIM_FAST_MS);
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
          s_anim_step = 0; s_size = SIZE_CYCLE[0];
          layer_mark_dirty(s_canvas_layer);
          schedule(ANIM_FAST_MS);
        } else {
          s_phase = PHASE_DONE; s_size = s_target_size;
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
static void unobstructed_change(AnimationProgress progress, void *ctx) {
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  // Only update target_size for non-comps wide layouts
  if (s_layout == LAYOUT_WIDE)
    s_target_size = pick_size(ub.size.h);
  s_stack_size = pick_stack_size(ub.size.h);
  if (s_phase == PHASE_DONE && s_layout == LAYOUT_WIDE) {
    s_size = s_target_size;
    layer_mark_dirty(s_canvas_layer);
  }
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_phase != PHASE_DONE) return;
  s_layout = (s_layout + 1) % LAYOUT_COUNT;
  if (s_layout == LAYOUT_WIDE_COMPS) {
    s_target_size = WIDE_COMP_SIZE;
    s_size        = WIDE_COMP_SIZE;
  } else if (s_layout == LAYOUT_WIDE) {
    s_target_size = WIDE_FULL_SIZE;
    s_phase = PHASE_SHAKE_CYCLE;
    s_anim_step = 0; s_anim_rep = 0;
    s_size = SIZE_CYCLE[0];
    schedule(ANIM_FAST_MS);
  } else {
    // LEFT / RIGHT: use stack size, target_size not relevant for wide anim
    s_target_size = WIDE_FULL_SIZE;
  }
  layer_mark_dirty(s_canvas_layer);
}

static void tick_handler(struct tm *t, TimeUnits units) {
  bool do_squish = (s_layout == LAYOUT_WIDE || s_layout == LAYOUT_WIDE_COMPS);
  if (s_phase == PHASE_DONE && do_squish) {
    s_pending_hour   = t->tm_hour;
    s_pending_minute = t->tm_min;
    s_digit_pending  = true;
    s_phase      = PHASE_SQUISH;
    s_going_down = true;
    schedule(ANIM_FAST_MS);
  } else if (s_phase == PHASE_SQUISH) {
    s_pending_hour   = t->tm_hour;
    s_pending_minute = t->tm_min;
    s_digit_pending  = true;
  } else {
    s_hour   = t->tm_hour;
    s_minute = t->tm_min;
    layer_mark_dirty(s_canvas_layer);
  }
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

#if defined(PBL_HEALTH)
static void health_handler(HealthEventType event, void *context) {
  if (event == HealthEventMovementUpdate || event == HealthEventSignificantUpdate) {
    HealthServiceAccessibilityMask mask;
    mask = health_service_metric_accessible(
      HealthMetricStepCount, time_start_of_today(), time(NULL));
    s_steps = (mask & HealthServiceAccessibilityMaskAvailable)
      ? (int)health_service_sum_today(HealthMetricStepCount) : 0;
    mask = health_service_metric_accessible(
      HealthMetricWalkedDistanceMeters, time_start_of_today(), time(NULL));
    s_distance_m = (mask & HealthServiceAccessibilityMaskAvailable)
      ? (int)health_service_sum_today(HealthMetricWalkedDistanceMeters) : 0;
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
    const char *desc = "WEATHER";
    if (code == 0)       desc = "CLEAR";
    else if (code <= 3)  desc = "CLOUDY";
    else if (code <= 49) desc = "FOG";
    else if (code <= 69) desc = "RAIN";
    else if (code <= 79) desc = "SNOW";
    else if (code <= 99) desc = "STORM";
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
  s_target_size = WIDE_FULL_SIZE;
  s_stack_size  = pick_stack_size(ub.size.h);
  s_size = 6;
  UnobstructedAreaHandlers ua = { .change = unobstructed_change };
  unobstructed_area_service_subscribe(ua, NULL);
  accel_tap_service_subscribe(accel_tap_handler);
  s_phase = PHASE_COUNTDOWN;
  s_countdown_digit = 9;
  s_anim_step = 0;
  s_demo_override = true;
  s_demo_digit = 9;
  s_demo_size  = GROW[0];
  layer_mark_dirty(s_canvas_layer);
  schedule(COUNTDOWN_MS);
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
    .load = window_load, .unload = window_unload,
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
