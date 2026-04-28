#include <pebble.h>

// ============================================================
// TallBoy — main.c  v2.7
//
// RULE: circles ALWAYS fixed: ro=2u, ri=1u. Only straight spans stretch.
//
// 8 geometry (correct):
//   Two 0-rings, each occupying h/2 of the digit height.
//   Top ring:    top cap at (top_y + ro),  bottom cap at (cy - ro)
//   Bottom ring: top cap at (cy + ro),     bottom cap at (bot_y - ro)
//   VBARs connect each ring's cap centers (auto-absent at size 1 since y0>y1).
//   At size 1: just two overlapping circles, exactly 1u (stroke width) overlap.
// ============================================================

#define LAYOUT_WIDE        0
#define LAYOUT_WIDE_COMPS  1
#define LAYOUT_LEFT        2
#define LAYOUT_RIGHT       3
#define LAYOUT_VECTOR      4
#define LAYOUT_COUNT       5
static int s_layout = LAYOUT_WIDE;

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
  #define STACK_H_SZ1    42
  #define STACK_H_SZ2    66
  #define STACK_SZ2_MIN 146
  #define INFO_FONT_H    18
  #define INFO_LINE_H    20
  #define UNIT            6
#endif

#define HALF_SLOT_PAD  (UNIT / 2)
#define GLYPH_W        (UNIT * 4)
#define COMP_GAP  4
#define COMP_LINES_ABOVE  2
#define COMP_LINES_BELOW  2

#define COUNTDOWN_HOLD_MS  1500
#define BLINK_REPS    2
#define ANIM_FAST_MS  80
#define WIDE_FULL_SIZE 6

static const int SIZE_CYCLE[] = { 6, 5, 4, 3, 2, 1, 2, 3, 4, 5, 6 };
#define SIZE_CYCLE_LEN 11

static int s_stack_size = 2;
static int pick_stack_size(int ub_h) { return (ub_h >= STACK_SZ2_MIN) ? 2 : 1; }
static int stack_digit_h(int sz)     { return (sz == 2) ? STACK_H_SZ2 : STACK_H_SZ1; }

static int pick_size(int available_h) {
  for (int s = 6; s >= 1; s--)
    if ((UNIT * (3 + 4 * s)) <= available_h - UNIT) return s;
  return 1;
}

static int digit_outer_h(int sz) { return UNIT * (3 + 4 * sz); }

static int comp_time_space(int ub_h) {
  int avail = ub_h - COMP_LINES_ABOVE * INFO_LINE_H - COMP_GAP
                   - COMP_LINES_BELOW * INFO_LINE_H - COMP_GAP;
  return avail > 0 ? avail : 0;
}

typedef enum { PHASE_COUNTDOWN, PHASE_BLINK, PHASE_DONE, PHASE_SQUISH, PHASE_SHAKE_CYCLE } Phase;

static Window    *s_window;
static Layer     *s_canvas_layer;
static int        s_hour = 0, s_minute = 0, s_size = 6, s_target_size = 6;
static GColor     s_fg, s_bg;
static Phase      s_phase = PHASE_COUNTDOWN;
static int        s_anim_step = 0, s_anim_rep = 0;
static bool       s_going_down = true;
static int        s_countdown_digit = 9;
static AppTimer  *s_timer = NULL;
static bool       s_demo_override = false;
static int        s_demo_digit = 9, s_demo_size = 6;
static bool       s_digit_pending = false;
static int        s_pending_hour = 0, s_pending_minute = 0;
static int        s_battery_pct = 100;
static bool       s_charging = false, s_bt_connected = true;
static int        s_steps = 0, s_distance_m = 0, s_heart_rate = 0;
static char       s_weather_temp[8] = "", s_weather_desc[32] = "";
static GBitmap   *s_bitmaps[10][6];
static GBitmap   *s_colon_bm[6];

#if defined(PBL_PLATFORM_EMERY)
static const uint32_t s_res[10][6] = {
  {RESOURCE_ID_TALLBOY_01,RESOURCE_ID_TALLBOY_02,RESOURCE_ID_TALLBOY_03,RESOURCE_ID_TALLBOY_04,RESOURCE_ID_TALLBOY_05,RESOURCE_ID_TALLBOY_06},
  {RESOURCE_ID_TALLBOY_11,RESOURCE_ID_TALLBOY_12,RESOURCE_ID_TALLBOY_13,RESOURCE_ID_TALLBOY_14,RESOURCE_ID_TALLBOY_15,RESOURCE_ID_TALLBOY_16},
  {RESOURCE_ID_TALLBOY_21,RESOURCE_ID_TALLBOY_22,RESOURCE_ID_TALLBOY_23,RESOURCE_ID_TALLBOY_24,RESOURCE_ID_TALLBOY_25,RESOURCE_ID_TALLBOY_26},
  {RESOURCE_ID_TALLBOY_31,RESOURCE_ID_TALLBOY_32,RESOURCE_ID_TALLBOY_33,RESOURCE_ID_TALLBOY_34,RESOURCE_ID_TALLBOY_35,RESOURCE_ID_TALLBOY_36},
  {RESOURCE_ID_TALLBOY_41,RESOURCE_ID_TALLBOY_42,RESOURCE_ID_TALLBOY_43,RESOURCE_ID_TALLBOY_44,RESOURCE_ID_TALLBOY_45,RESOURCE_ID_TALLBOY_46},
  {RESOURCE_ID_TALLBOY_51,RESOURCE_ID_TALLBOY_52,RESOURCE_ID_TALLBOY_53,RESOURCE_ID_TALLBOY_54,RESOURCE_ID_TALLBOY_55,RESOURCE_ID_TALLBOY_56},
  {RESOURCE_ID_TALLBOY_61,RESOURCE_ID_TALLBOY_62,RESOURCE_ID_TALLBOY_63,RESOURCE_ID_TALLBOY_64,RESOURCE_ID_TALLBOY_65,RESOURCE_ID_TALLBOY_66},
  {RESOURCE_ID_TALLBOY_71,RESOURCE_ID_TALLBOY_72,RESOURCE_ID_TALLBOY_73,RESOURCE_ID_TALLBOY_74,RESOURCE_ID_TALLBOY_75,RESOURCE_ID_TALLBOY_76},
  {RESOURCE_ID_TALLBOY_81,RESOURCE_ID_TALLBOY_82,RESOURCE_ID_TALLBOY_83,RESOURCE_ID_TALLBOY_84,RESOURCE_ID_TALLBOY_85,RESOURCE_ID_TALLBOY_86},
  {RESOURCE_ID_TALLBOY_91,RESOURCE_ID_TALLBOY_92,RESOURCE_ID_TALLBOY_93,RESOURCE_ID_TALLBOY_94,RESOURCE_ID_TALLBOY_95,RESOURCE_ID_TALLBOY_96},
};
static const uint32_t s_colon_res[6] = {RESOURCE_ID_TALLBOY_COLON1,RESOURCE_ID_TALLBOY_COLON2,RESOURCE_ID_TALLBOY_COLON3,RESOURCE_ID_TALLBOY_COLON4,RESOURCE_ID_TALLBOY_COLON5,RESOURCE_ID_TALLBOY_COLON6};
#else
static const uint32_t s_res[10][6] = {
  {RESOURCE_ID_TALLBOY_L01,RESOURCE_ID_TALLBOY_L02,RESOURCE_ID_TALLBOY_L03,RESOURCE_ID_TALLBOY_L04,RESOURCE_ID_TALLBOY_L05,RESOURCE_ID_TALLBOY_L06},
  {RESOURCE_ID_TALLBOY_L11,RESOURCE_ID_TALLBOY_L12,RESOURCE_ID_TALLBOY_L13,RESOURCE_ID_TALLBOY_L14,RESOURCE_ID_TALLBOY_L15,RESOURCE_ID_TALLBOY_L16},
  {RESOURCE_ID_TALLBOY_L21,RESOURCE_ID_TALLBOY_L22,RESOURCE_ID_TALLBOY_L23,RESOURCE_ID_TALLBOY_L24,RESOURCE_ID_TALLBOY_L25,RESOURCE_ID_TALLBOY_L26},
  {RESOURCE_ID_TALLBOY_L31,RESOURCE_ID_TALLBOY_L32,RESOURCE_ID_TALLBOY_L33,RESOURCE_ID_TALLBOY_L34,RESOURCE_ID_TALLBOY_L35,RESOURCE_ID_TALLBOY_L36},
  {RESOURCE_ID_TALLBOY_L41,RESOURCE_ID_TALLBOY_L42,RESOURCE_ID_TALLBOY_L43,RESOURCE_ID_TALLBOY_L44,RESOURCE_ID_TALLBOY_L45,RESOURCE_ID_TALLBOY_L46},
  {RESOURCE_ID_TALLBOY_L51,RESOURCE_ID_TALLBOY_L52,RESOURCE_ID_TALLBOY_L53,RESOURCE_ID_TALLBOY_L54,RESOURCE_ID_TALLBOY_L55,RESOURCE_ID_TALLBOY_L56},
  {RESOURCE_ID_TALLBOY_L61,RESOURCE_ID_TALLBOY_L62,RESOURCE_ID_TALLBOY_L63,RESOURCE_ID_TALLBOY_L64,RESOURCE_ID_TALLBOY_L65,RESOURCE_ID_TALLBOY_L66},
  {RESOURCE_ID_TALLBOY_L71,RESOURCE_ID_TALLBOY_L72,RESOURCE_ID_TALLBOY_L73,RESOURCE_ID_TALLBOY_L74,RESOURCE_ID_TALLBOY_L75,RESOURCE_ID_TALLBOY_L76},
  {RESOURCE_ID_TALLBOY_L81,RESOURCE_ID_TALLBOY_L82,RESOURCE_ID_TALLBOY_L83,RESOURCE_ID_TALLBOY_L84,RESOURCE_ID_TALLBOY_L85,RESOURCE_ID_TALLBOY_L86},
  {RESOURCE_ID_TALLBOY_L91,RESOURCE_ID_TALLBOY_L92,RESOURCE_ID_TALLBOY_L93,RESOURCE_ID_TALLBOY_L94,RESOURCE_ID_TALLBOY_L95,RESOURCE_ID_TALLBOY_L96},
};
static const uint32_t s_colon_res[6] = {RESOURCE_ID_TALLBOY_LCOLON1,RESOURCE_ID_TALLBOY_LCOLON2,RESOURCE_ID_TALLBOY_LCOLON3,RESOURCE_ID_TALLBOY_LCOLON4,RESOURCE_ID_TALLBOY_LCOLON5,RESOURCE_ID_TALLBOY_LCOLON6};
#endif

static uint32_t find_res(int digit, int size) {
  int si = size - 1;
  if (s_res[digit][si]) return s_res[digit][si];
  for (int i = si+1; i < 6; i++) if (s_res[digit][i]) return s_res[digit][i];
  for (int i = si-1; i >= 0; i--) if (s_res[digit][i]) return s_res[digit][i];
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
  if (!s_colon_bm[si]) {
    if (s_colon_res[si]) s_colon_bm[si] = gbitmap_create_with_resource(s_colon_res[si]);
    if (!s_colon_bm[si]) {
      for (int i = 0; i < 6; i++) {
        if (i == si) continue;
        if (!s_colon_bm[i] && s_colon_res[i]) s_colon_bm[i] = gbitmap_create_with_resource(s_colon_res[i]);
        if (s_colon_bm[i]) return s_colon_bm[i];
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

static void free_digit_bitmaps(int digit) {
  for (int s = 0; s < 6; s++)
    if (s_bitmaps[digit][s]) { gbitmap_destroy(s_bitmaps[digit][s]); s_bitmaps[digit][s] = NULL; }
}

static void blit(GContext *ctx, GBitmap *bm, int x, int y) {
  if (!bm) return;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, bm, GRect(x, y, SLOT_W, SCREEN_H));
}

static void draw_digits(GContext *ctx, int h_tens, int h_ones, int m_tens, int m_ones,
                        int size, int y, bool colon) {
  blit(ctx, get_bitmap(h_tens, size), SLOT_H_TENS, y);
  blit(ctx, get_bitmap(h_ones, size), SLOT_H_ONES, y);
  if (colon) blit(ctx, get_colon(size), COLON_SLOT_X, y);
  blit(ctx, get_bitmap(m_tens, size), SLOT_M_TENS, y);
  blit(ctx, get_bitmap(m_ones, size), SLOT_M_ONES, y);
}

// ============================================================
// VECTOR DIGIT DRAWING
// ============================================================

static void fill_arc(GContext *ctx, int cx, int cy, int ro, int ri, int a0, int a1) {
  GRect b = GRect(cx - ro, cy - ro, ro * 2, ro * 2);
  graphics_fill_radial(ctx, b, GOvalScaleModeFitCircle, (uint16_t)(ro - ri),
                       DEG_TO_TRIGANGLE(a0), DEG_TO_TRIGANGLE(a1));
}

static void draw_digit_vec(GContext *ctx, int digit, int slot_x, int cy, int size) {
  graphics_context_set_fill_color(ctx, s_fg);

  const int ro = UNIT * 2;
  const int ri = UNIT * 1;
  const int sw = UNIT;

  int h      = digit_outer_h(size);
  int body_h = h - ro * 2;
  int gx     = slot_x + HALF_SLOT_PAD;
  int gx_r   = gx + GLYPH_W - sw;
  int cap_cx = gx + ro;
  int top_cy = cy - body_h / 2;
  int bot_cy = cy + body_h / 2;
  int top_y  = cy - h / 2;
  int bot_y  = cy + h / 2;

#define VBAR(x,y0,y1) if((y1)>(y0)) graphics_fill_rect(ctx,GRect((x),(y0),sw,(y1)-(y0)),0,GCornerNone)
#define HBAR(y) graphics_fill_rect(ctx,GRect(gx,(y),GLYPH_W,sw),0,GCornerNone)
#define RING(ccx,ccy) do { \
  fill_arc(ctx,(ccx),(ccy),ro,ri,270,450); \
  fill_arc(ctx,(ccx),(ccy),ro,ri,90,270); } while(0)

  switch (digit) {

    case 0:
      fill_arc(ctx, cap_cx, top_cy, ro, ri, 270, 450);
      fill_arc(ctx, cap_cx, bot_cy, ro, ri, 90, 270);
      VBAR(gx,   top_cy, bot_cy);
      VBAR(gx_r, top_cy, bot_cy);
      break;

    case 1: {
      HBAR(bot_y - sw);
      int stem_x = gx + GLYPH_W / 2 - sw / 2;
      VBAR(stem_x, top_y, bot_y - sw);
      for (int i = 0; i < stem_x - gx; i++)
        graphics_fill_rect(ctx, GRect(stem_x-i-1, top_y+i, sw, sw), 0, GCornerNone);
      break;
    }

    case 2:
      fill_arc(ctx, cap_cx, top_cy, ro, ri, 270, 450);
      {
        int dh = bot_cy - top_cy, dw = GLYPH_W - sw;
        if (dh > 0)
          for (int i = 0; i < dh; i++)
            graphics_fill_rect(ctx, GRect(gx_r-(i*dw/dh), top_cy+i, sw, sw), 0, GCornerNone);
      }
      VBAR(gx, bot_cy - ri, bot_cy);
      fill_arc(ctx, cap_cx, bot_cy, ro, ri, 180, 270);
      HBAR(bot_y - sw);
      break;

    case 3:
      fill_arc(ctx, cap_cx, top_cy, ro, ri, 0, 180);
      fill_arc(ctx, cap_cx, bot_cy, ro, ri, 0, 180);
      VBAR(gx_r, top_cy, bot_cy);
      break;

    case 4:
      VBAR(gx,   top_y, cy + sw);
      VBAR(gx_r, top_y, bot_y);
      graphics_fill_rect(ctx, GRect(gx, cy, GLYPH_W, sw), 0, GCornerNone);
      break;

    case 5:
      HBAR(top_y);
      VBAR(gx, top_y + sw, cy);
      RING(cap_cx, bot_cy);
      VBAR(gx_r, bot_cy - ro, bot_cy);
      break;

    case 6:
      fill_arc(ctx, cap_cx, top_cy, ro, ri, 180, 360);
      VBAR(gx, top_cy, bot_cy);
      RING(cap_cx, bot_cy);
      VBAR(gx_r, cy, bot_cy);
      break;

    case 7:
      HBAR(top_y);
      {
        int x0 = gx_r, y0 = top_y + sw, x1 = gx, y1 = bot_y;
        int dh = y1 - y0, dw = x0 - x1;
        if (dh > 0 && dw > 0)
          for (int i = 0; i <= dh; i++)
            graphics_fill_rect(ctx, GRect(x0-(i*dw/dh), y0+i, sw, sw), 0, GCornerNone);
      }
      break;

    case 8: {
      // Two 0-rings each occupying h/2 of digit height.
      // Top ring:    top cap at (top_y+ro),  bottom cap at (cy-ro)
      // Bottom ring: top cap at (cy+ro),     bottom cap at (bot_y-ro)
      // VBARs auto-absent at size 1 (y0 >= y1 condition in VBAR macro).
      int t_tc = top_y + ro;   // top ring, top cap center
      int t_bc = cy - ro;      // top ring, bottom cap center
      int b_tc = cy + ro;      // bottom ring, top cap center
      int b_bc = bot_y - ro;   // bottom ring, bottom cap center
      fill_arc(ctx, cap_cx, t_tc, ro, ri, 270, 450); // top ring top cap
      fill_arc(ctx, cap_cx, t_bc, ro, ri, 90, 270);  // top ring bottom cap
      VBAR(gx,   t_tc, t_bc);                         // top ring left bar
      VBAR(gx_r, t_tc, t_bc);                         // top ring right bar
      fill_arc(ctx, cap_cx, b_tc, ro, ri, 270, 450); // bottom ring top cap
      fill_arc(ctx, cap_cx, b_bc, ro, ri, 90, 270);  // bottom ring bottom cap
      VBAR(gx,   b_tc, b_bc);                         // bottom ring left bar
      VBAR(gx_r, b_tc, b_bc);                         // bottom ring right bar
      break;
    }

    case 9:
      RING(cap_cx, top_cy);
      VBAR(gx_r, top_cy, bot_cy);
      VBAR(gx,   top_cy, cy);
      fill_arc(ctx, cap_cx, bot_cy, ro, ri, 0, 180);
      break;

    default: break;
  }
#undef VBAR
#undef HBAR
#undef RING
}

static void draw_colon_vec(GContext *ctx, int slot_x, int cy, int size) {
  graphics_context_set_fill_color(ctx, s_fg);
  int h = digit_outer_h(size), r = UNIT / 2, dx = slot_x + SLOT_W / 2;
  GRect b1 = GRect(dx-r, cy-h/4-r, r*2, r*2);
  GRect b2 = GRect(dx-r, cy+h/4-r, r*2, r*2);
  graphics_fill_radial(ctx, b1, GOvalScaleModeFitCircle, (uint16_t)r, 0, DEG_TO_TRIGANGLE(360));
  graphics_fill_radial(ctx, b2, GOvalScaleModeFitCircle, (uint16_t)r, 0, DEG_TO_TRIGANGLE(360));
}

static void draw_digits_vec(GContext *ctx, int h_tens, int h_ones,
                            int m_tens, int m_ones, int size, int cy) {
  draw_digit_vec(ctx, h_tens, SLOT_H_TENS, cy, size);
  draw_digit_vec(ctx, h_ones, SLOT_H_ONES, cy, size);
  draw_colon_vec(ctx, COLON_SLOT_X, cy, size);
  draw_digit_vec(ctx, m_tens, SLOT_M_TENS, cy, size);
  draw_digit_vec(ctx, m_ones, SLOT_M_ONES, cy, size);
}

static void draw_digits_countdown_split(GContext *ctx, int digit, int size, int cy) {
  int y = cy - SCREEN_H / 2;
  blit(ctx, get_bitmap(digit, size), SLOT_H_TENS, y);
  blit(ctx, get_bitmap(digit, size), SLOT_M_ONES, y);
  draw_digit_vec(ctx, digit, SLOT_H_ONES, cy, size);
  draw_digit_vec(ctx, digit, SLOT_M_TENS, cy, size);
  draw_colon_vec(ctx, COLON_SLOT_X, cy, size);
}

// ============================================================
// INFO LINES
// ============================================================
static const char *s_day_names[]   = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
static const char *s_day_full[]    = {"SUNDAY","MONDAY","TUESDAY","WEDNESDAY","THURSDAY","FRIDAY","SATURDAY"};
static const char *s_month_abbrs[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};

static int build_comp_above(char lines[][32], int max_lines, struct tm *t) {
  int n = 0;
  if (n < max_lines && t)
    snprintf(lines[n++], 32, "%s  %s %d", s_day_full[t->tm_wday], s_month_abbrs[t->tm_mon], t->tm_mday);
  return n;
}

static int build_comp_below(char lines[][32], int max_lines, struct tm *t) {
  int n = 0;
#if defined(PBL_HEALTH)
  if (n < max_lines) {
    if (s_steps >= 1000) snprintf(lines[n++], 32, "%d,%03d steps", s_steps/1000, s_steps%1000);
    else snprintf(lines[n++], 32, "%d steps", s_steps);
  }
  if (n < max_lines && s_distance_m > 0) {
    if (strcmp(i18n_get_system_locale(), "en_US") == 0) { int mx=(s_distance_m*10)/1609; snprintf(lines[n++],32,"%d.%d mi",mx/10,mx%10); }
    else { int kx=(s_distance_m*10)/1000; snprintf(lines[n++],32,"%d.%d km",kx/10,kx%10); }
  }
#endif
  if (n == 0 && n < max_lines) snprintf(lines[n++], 32, "bat %d%%%s", s_battery_pct, s_charging?"+":"");
  return n;
}

static int build_info_lines_short(char lines[][32], int max_lines, struct tm *t) {
  int n = 0;
  if (n < max_lines && t) snprintf(lines[n++], 32, "%s", s_day_names[t->tm_wday]);
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
#endif
  if (n < max_lines) snprintf(lines[n++], 32, "bat %d%%", s_battery_pct);
  return n;
}

static GFont prv_info_font(void) { return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD); }

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
  int n = build_info_lines_short(lines, area.size.h / INFO_LINE_H < 6 ? area.size.h / INFO_LINE_H : 6, t);
  if (!n) return;
  int start_y = area.origin.y + (area.size.h - n * INFO_LINE_H) / 2;
  draw_info_block(ctx, area.origin.x, start_y, area.size.w, lines, n);
}

// ============================================================
// DRAW LAYER
// ============================================================
static void draw_layer(Layer *layer, GContext *ctx) {
  Layer *root  = window_get_root_layer(s_window);
  GRect ub     = layer_get_unobstructed_bounds(root);
  int ub_top   = ub.origin.y, ub_h = ub.size.h;
  int center_y = ub_top + ub_h / 2;

  graphics_context_set_fill_color(ctx, s_bg);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  int h_tens, h_ones, m_tens, m_ones, size;
  if (s_demo_override) {
    h_tens = h_ones = m_tens = m_ones = s_demo_digit; size = s_demo_size;
  } else {
    int h = s_hour % 12; if (!h) h = 12;
    h_tens = h/10; h_ones = h%10; m_tens = s_minute/10; m_ones = s_minute%10; size = s_size;
  }

  time_t now_t = time(NULL);
  struct tm *tm = (s_phase == PHASE_DONE) ? localtime(&now_t) : NULL;

  if (s_demo_override && s_phase == PHASE_COUNTDOWN) {
    draw_digits_countdown_split(ctx, s_demo_digit, s_demo_size, center_y);
  } else if (s_layout == LAYOUT_VECTOR) {
    draw_digits_vec(ctx, h_tens, h_ones, m_tens, m_ones, size, center_y);
  } else if (s_layout == LAYOUT_WIDE) {
    draw_digits(ctx, h_tens, h_ones, m_tens, m_ones, size, center_y - SCREEN_H/2, true);
  } else if (s_layout == LAYOUT_WIDE_COMPS) {
    int above_h = COMP_LINES_ABOVE * INFO_LINE_H, below_h = COMP_LINES_BELOW * INFO_LINE_H;
    int above_top = ub_top, below_top = ub_top + ub_h - below_h;
    int tz_top = above_top + above_h + COMP_GAP, tz_bot = below_top - COMP_GAP;
    int tz_h = tz_bot - tz_top, dsz = pick_size(tz_h);
    int y = (tz_top + tz_h/2) - SCREEN_H/2;
    draw_digits(ctx, h_tens, h_ones, m_tens, m_ones, dsz, y, true);
    if (tm) {
      char al[COMP_LINES_ABOVE][32], bl[COMP_LINES_BELOW][32];
      int na = build_comp_above(al, COMP_LINES_ABOVE, tm);
      int nb = build_comp_below(bl, COMP_LINES_BELOW, tm);
      if (na) draw_info_block(ctx, 0, above_top, SCREEN_W, al, na);
      if (nb) draw_info_block(ctx, 0, below_top, SCREEN_W, bl, nb);
    }
  } else {
    int sdh = stack_digit_h(s_stack_size);
    int h_y = (center_y - sdh/2 - HALF_UNIT) - SCREEN_H/2;
    int m_y = (center_y + sdh/2 + HALF_UNIT) - SCREEN_H/2;
    int tens_x, ones_x, info_x, info_w;
    if (s_layout == LAYOUT_LEFT) {
      tens_x = SIDE_MARGIN; ones_x = SIDE_MARGIN + SLOT_W;
      info_x = SIDE_MARGIN + SLOT_W*2 + SIDE_MARGIN; info_w = SCREEN_W - info_x - SIDE_MARGIN;
    } else {
      ones_x = SCREEN_W - SIDE_MARGIN - SLOT_W; tens_x = ones_x - SLOT_W;
      info_x = SIDE_MARGIN; info_w = tens_x - SIDE_MARGIN*2;
    }
    blit(ctx, get_bitmap(h_tens, s_stack_size), tens_x, h_y);
    blit(ctx, get_bitmap(h_ones, s_stack_size), ones_x, h_y);
    blit(ctx, get_bitmap(m_tens, s_stack_size), tens_x, m_y);
    blit(ctx, get_bitmap(m_ones, s_stack_size), ones_x, m_y);
    if (tm && info_w > 20) draw_info_column(ctx, GRect(info_x, ub_top, info_w, ub_h), tm);
  }
}

// ============================================================
// TIMER
// ============================================================
static void timer_cb(void *data);
static void schedule(uint32_t ms) { if (s_timer) app_timer_cancel(s_timer); s_timer = app_timer_register(ms, timer_cb, NULL); }

static void timer_cb(void *data) {
  s_timer = NULL;
  switch (s_phase) {
    case PHASE_COUNTDOWN:
      free_digit_bitmaps(s_countdown_digit);
      if (s_countdown_digit > 0) {
        s_countdown_digit--; s_demo_digit = s_countdown_digit;
        s_demo_size = WIDE_FULL_SIZE; s_demo_override = true;
        layer_mark_dirty(s_canvas_layer); schedule(COUNTDOWN_HOLD_MS);
      } else {
        s_demo_override = false;
        for (int s = 0; s < 6; s++) if (s_colon_bm[s]) { gbitmap_destroy(s_colon_bm[s]); s_colon_bm[s] = NULL; }
        s_size=6; s_going_down=true; s_anim_rep=0; s_phase=PHASE_BLINK;
        layer_mark_dirty(s_canvas_layer); schedule(ANIM_FAST_MS);
      }
      break;
    case PHASE_BLINK:
      if (s_going_down) { s_size--; layer_mark_dirty(s_canvas_layer); if(s_size<=1){s_size=1;s_going_down=false;} schedule(ANIM_FAST_MS); }
      else { s_size++; layer_mark_dirty(s_canvas_layer); if(s_size>=WIDE_FULL_SIZE){s_size=WIDE_FULL_SIZE;if(++s_anim_rep<BLINK_REPS){s_going_down=true;schedule(ANIM_FAST_MS);}else{s_target_size=WIDE_FULL_SIZE;s_phase=PHASE_DONE;layer_mark_dirty(s_canvas_layer);}}else schedule(ANIM_FAST_MS); }
      break;
    case PHASE_SQUISH:
      if (s_going_down) { s_size--; layer_mark_dirty(s_canvas_layer); if(s_size<=1){s_size=1;if(s_digit_pending){s_hour=s_pending_hour;s_minute=s_pending_minute;s_digit_pending=false;}s_going_down=false;} schedule(ANIM_FAST_MS); }
      else { s_size++; layer_mark_dirty(s_canvas_layer); if(s_size>=s_target_size){s_size=s_target_size;s_phase=PHASE_DONE;layer_mark_dirty(s_canvas_layer);}else schedule(ANIM_FAST_MS); }
      break;
    case PHASE_SHAKE_CYCLE:
      if(++s_anim_step<SIZE_CYCLE_LEN){s_size=SIZE_CYCLE[s_anim_step];layer_mark_dirty(s_canvas_layer);schedule(ANIM_FAST_MS);}
      else if(++s_anim_rep<2){s_anim_step=0;s_size=SIZE_CYCLE[0];layer_mark_dirty(s_canvas_layer);schedule(ANIM_FAST_MS);}
      else{s_phase=PHASE_DONE;s_size=s_target_size;layer_mark_dirty(s_canvas_layer);}
      break;
    case PHASE_DONE: break;
  }
}

static void prv_update_targets(void) {
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  s_stack_size = pick_stack_size(ub.size.h);
  if (s_layout==LAYOUT_WIDE||s_layout==LAYOUT_VECTOR) s_target_size=WIDE_FULL_SIZE;
  else if (s_layout==LAYOUT_WIDE_COMPS) s_target_size=pick_size(comp_time_space(ub.size.h));
}

static void unobstructed_change(AnimationProgress progress, void *ctx) {
  prv_update_targets();
  if (s_phase==PHASE_DONE&&(s_layout==LAYOUT_WIDE||s_layout==LAYOUT_WIDE_COMPS||s_layout==LAYOUT_VECTOR))
    { s_size=s_target_size; layer_mark_dirty(s_canvas_layer); }
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_phase != PHASE_DONE) return;
  s_layout = (s_layout + 1) % LAYOUT_COUNT;
  prv_update_targets();
  if (s_layout==LAYOUT_WIDE){s_phase=PHASE_SHAKE_CYCLE;s_anim_step=0;s_anim_rep=0;s_size=SIZE_CYCLE[0];schedule(ANIM_FAST_MS);}
  else{s_size=s_target_size;}
  layer_mark_dirty(s_canvas_layer);
}

static void tick_handler(struct tm *t, TimeUnits units) {
  bool sq=(s_layout==LAYOUT_WIDE||s_layout==LAYOUT_WIDE_COMPS||s_layout==LAYOUT_VECTOR);
  if(s_phase==PHASE_DONE&&sq){s_pending_hour=t->tm_hour;s_pending_minute=t->tm_min;s_digit_pending=true;s_phase=PHASE_SQUISH;s_going_down=true;schedule(ANIM_FAST_MS);}
  else if(s_phase==PHASE_SQUISH){s_pending_hour=t->tm_hour;s_pending_minute=t->tm_min;s_digit_pending=true;}
  else{s_hour=t->tm_hour;s_minute=t->tm_min;layer_mark_dirty(s_canvas_layer);}
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
#if defined(PBL_PLATFORM_EMERY)||defined(PBL_PLATFORM_DIORITE)
  if(event==HealthEventHeartRateUpdate){
    HealthServiceAccessibilityMask mask=health_service_metric_accessible(HealthMetricHeartRateBPM,time(NULL),time(NULL)+1);
    s_heart_rate=(mask&HealthServiceAccessibilityMaskAvailable)?(int)health_service_peek_current_value(HealthMetricHeartRateBPM):0;
  }
#endif
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
  s_target_size=WIDE_FULL_SIZE; s_stack_size=pick_stack_size(ub.size.h); s_size=6;
  UnobstructedAreaHandlers ua={.change=unobstructed_change};
  unobstructed_area_service_subscribe(ua,NULL);
  accel_tap_service_subscribe(accel_tap_handler);
  s_phase=PHASE_COUNTDOWN; s_countdown_digit=9;
  s_demo_override=true; s_demo_digit=9; s_demo_size=WIDE_FULL_SIZE;
  layer_mark_dirty(s_canvas_layer); schedule(COUNTDOWN_HOLD_MS);
}

static void window_unload(Window *window) {
  unobstructed_area_service_unsubscribe(); accel_tap_service_unsubscribe();
  if(s_timer){app_timer_cancel(s_timer);s_timer=NULL;}
  free_bitmaps(); layer_destroy(s_canvas_layer);
}

static void init(void) {
  s_fg=GColorWhite; s_bg=GColorBlack;
  memset(s_bitmaps,0,sizeof(s_bitmaps)); memset(s_colon_bm,0,sizeof(s_colon_bm));
  s_window=window_create(); window_set_background_color(s_window,GColorBlack);
  window_set_window_handlers(s_window,(WindowHandlers){.load=window_load,.unload=window_unload});
  window_stack_push(s_window,true);
  time_t now=time(NULL); struct tm *t=localtime(&now); s_hour=t->tm_hour; s_minute=t->tm_min;
  tick_timer_service_subscribe(MINUTE_UNIT,tick_handler);
  battery_state_service_subscribe(battery_handler); battery_handler(battery_state_service_peek());
  bluetooth_connection_service_subscribe(bt_handler); s_bt_connected=bluetooth_connection_service_peek();
#if defined(PBL_HEALTH)
  health_service_events_subscribe(health_handler,NULL); health_handler(HealthEventSignificantUpdate,NULL);
#endif
  app_message_register_inbox_received(inbox_received); app_message_open(256,64);
}

static void deinit(void) {
  tick_timer_service_unsubscribe(); battery_state_service_unsubscribe(); bluetooth_connection_service_unsubscribe();
#if defined(PBL_HEALTH)
  health_service_events_unsubscribe();
#endif
  window_destroy(s_window);
}

int main(void){init();app_event_loop();deinit();}
