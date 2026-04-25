#include <pebble.h>

// ============================================================
// TallBoy — main.c  v1.5
//
// Added: LAYOUT_VECTOR — 5th carousel stop, draws digits
// procedurally using the unit-grid geometry. Shake cycles:
//   WIDE → WIDE_COMPS → LEFT → RIGHT → VECTOR → WIDE
//
// Unit grid:
//   emery:   1u = 8px,  slot = 5u (40px), glyph zone = 4u (32px)
//   low-res: 1u = 6px,  slot = 5u (30px), glyph zone = 4u (24px)
//
// Digit height = (3 + 4*size) units
// Stroke = 1 unit everywhere
// ============================================================

#define LAYOUT_WIDE        0
#define LAYOUT_WIDE_COMPS  1
#define LAYOUT_LEFT        2
#define LAYOUT_RIGHT       3
#define LAYOUT_VECTOR      4
#define LAYOUT_COUNT       5
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

// Half-unit padding each side of slot; glyph zone = 4 units
#define HALF_SLOT_PAD  (UNIT / 2)
#define GLYPH_W        (UNIT * 4)

#define COMP_GAP  4
#define COMP_LINES_ABOVE  2
#define COMP_LINES_BELOW  2

static int s_stack_size = 2;

static int pick_stack_size(int ub_h) {
  return (ub_h >= STACK_SZ2_MIN) ? 2 : 1;
}

static int stack_digit_h(int sz) {
  return (sz == 2) ? STACK_H_SZ2 : STACK_H_SZ1;
}

static int pick_size(int available_h) {
  for (int s = 6; s >= 1; s--)
    if ((UNIT * (3 + 4 * s)) <= available_h - UNIT) return s;
  return 1;
}

static int digit_outer_h(int sz) {
  return UNIT * (3 + 4 * sz);
}

static int comp_time_space(int ub_h) {
  int above_h = COMP_LINES_ABOVE * INFO_LINE_H + COMP_GAP;
  int below_h = COMP_LINES_BELOW * INFO_LINE_H + COMP_GAP;
  int avail = ub_h - above_h - below_h;
  return avail > 0 ? avail : 0;
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
#define WIDE_FULL_SIZE 6

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

static GBitmap *get_colon(int size) {
  int si = size - 1;
  if (!s_colon_bm[si]) {
    if (s_colon_res[si])
      s_colon_bm[si] = gbitmap_create_with_resource(s_colon_res[si]);
    if (!s_colon_bm[si]) {
      for (int i = 0; i < 6; i++) {
        if (i == si) continue;
        if (!s_colon_bm[i] && s_colon_res[i])
          s_colon_bm[i] = gbitmap_create_with_resource(s_colon_res[i]);
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

static void blit(GContext *ctx, GBitmap *bm, int x, int y) {
  if (!bm) return;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, bm, GRect(x, y, SLOT_W, SCREEN_H));
}

static void draw_digits(GContext *ctx, int h_tens, int h_ones,
                        int m_tens, int m_ones, int size,
                        int y, bool draw_colon_flag) {
  blit(ctx, get_bitmap(h_tens, size), SLOT_H_TENS, y);
  blit(ctx, get_bitmap(h_ones, size), SLOT_H_ONES, y);
  if (draw_colon_flag) blit(ctx, get_colon(size), COLON_SLOT_X, y);
  blit(ctx, get_bitmap(m_tens, size), SLOT_M_TENS, y);
  blit(ctx, get_bitmap(m_ones, size), SLOT_M_ONES, y);
}

// ============================================================
// VECTOR DIGIT DRAWING
//
// Coordinate convention: slot_x is the left edge of the 5u slot.
// cy is the vertical center of the digit in screen coordinates.
// All math in pixels derived from UNIT constant.
//
// Geometry for size s:
//   h = (3 + 4*s) * UNIT          — total digit height
//   w = 4 * UNIT                   — glyph width (within slot)
//   r_outer = 2 * UNIT             — outer cap radius (= w/2)
//   r_inner = 1 * UNIT             — inner hole radius
//   stroke  = 1 * UNIT
//   body_h  = h - 2 * r_outer      — straight vertical section height
//             = h - 4*UNIT = 4*(s-1)*UNIT
//   glyph left edge within slot: slot_x + HALF_SLOT_PAD
// ============================================================

// Fill a ring annulus: outer radius r_outer, inner radius r_inner,
// centered at (cx, cy), for the given angle range.
// We use graphics_fill_radial which takes an inset amount from the
// bounding rect rather than explicit radii, so:
//   inset = r_outer - r_inner  (subtracts from outside inward)
// The bounding rect is sized to the outer circle.
static void fill_arc(GContext *ctx, int cx, int cy, int r_outer, int r_inner,
                     int32_t angle_start, int32_t angle_end) {
  GRect bounds = GRect(cx - r_outer, cy - r_outer, r_outer * 2, r_outer * 2);
  uint16_t inset = (uint16_t)(r_outer - r_inner);
  graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, inset,
                       DEG_TO_TRIGANGLE(angle_start),
                       DEG_TO_TRIGANGLE(angle_end));
}

// Draw a single vector digit.
// slot_x: left pixel of the 5-unit slot
// cy: vertical center of the digit in screen pixels
// size: 1-6
static void draw_digit_vec(GContext *ctx, int digit, int slot_x, int cy, int size) {
  graphics_context_set_fill_color(ctx, s_fg);

  int h       = digit_outer_h(size);   // total height in px
  int r_outer = UNIT * 2;              // 2u outer radius
  int r_inner = UNIT * 1;              // 1u inner radius
  int stroke  = UNIT;                  // 1u stroke
  int body_h  = h - r_outer * 2;      // straight section between caps

  // Glyph left edge (skip half-unit padding)
  int gx = slot_x + HALF_SLOT_PAD;
  // Top/bottom cap centers
  int top_cy    = cy - body_h / 2;
  int bot_cy    = cy + body_h / 2;
  // Top of digit, bottom of digit
  int top_y     = cy - h / 2;
  int bot_y     = cy + h / 2;

  switch (digit) {
    case 0:
      // Top cap: ring, top 180°
      fill_arc(ctx, gx + r_outer, top_cy, r_outer, r_inner, 180, 360);
      // Bottom cap: ring, bottom 180°
      fill_arc(ctx, gx + r_outer, bot_cy, r_outer, r_inner, 0, 180);
      // Left stroke (vertical bar connecting caps)
      if (body_h > 0)
        graphics_fill_rect(ctx, GRect(gx, top_cy, stroke, body_h), 0, GCornerNone);
      // Right stroke
      if (body_h > 0)
        graphics_fill_rect(ctx, GRect(gx + GLYPH_W - stroke, top_cy, stroke, body_h),
                           0, GCornerNone);
      break;

    case 1:
      // Single vertical stroke, right-aligned (matching bitmap 1 which is right-of-center)
      graphics_fill_rect(ctx, GRect(gx + GLYPH_W - stroke, top_y, stroke, h),
                         0, GCornerNone);
      break;

    case 2:
      // Top-right cap (right half of top cap ring)
      fill_arc(ctx, gx + r_outer, top_cy, r_outer, r_inner, 270, 360);
      // Top-right corner continued to right edge: top half right stroke
      // Right stroke top half: top_cy down to mid
      {
        int mid_y = cy;
        int top_stroke_h = mid_y - top_cy;
        if (top_stroke_h > 0)
          graphics_fill_rect(ctx, GRect(gx + GLYPH_W - stroke, top_cy,
                                        stroke, top_stroke_h), 0, GCornerNone);
        // Diagonal / middle horizontal bar (fills middle as solid rect)
        // The 2 goes: top-right arc → down right side → diagonal to left → down left → bottom cap
        // Simplified: horizontal bar at midpoint
        graphics_fill_rect(ctx, GRect(gx, mid_y - stroke / 2, GLYPH_W, stroke),
                           0, GCornerNone);
        // Bottom-left vertical stroke (mid to bottom)
        int bot_stroke_h = bot_y - mid_y - stroke / 2;
        if (bot_stroke_h > 0)
          graphics_fill_rect(ctx, GRect(gx, mid_y + stroke / 2, stroke, bot_stroke_h),
                             0, GCornerNone);
        // Bottom cap left half
        fill_arc(ctx, gx + r_outer, bot_cy, r_outer, r_inner, 90, 180);
        // Bottom horizontal bar
        graphics_fill_rect(ctx, GRect(gx, bot_y - stroke, GLYPH_W, stroke),
                           0, GCornerNone);
      }
      break;

    case 3:
      // Like 2 but mirrored: left-side arcs, right vertical stroke
      fill_arc(ctx, gx + r_outer, top_cy, r_outer, r_inner, 270, 360);
      fill_arc(ctx, gx + r_outer, bot_cy, r_outer, r_inner, 0, 90);
      {
        int mid_y = cy;
        // Full right stroke
        graphics_fill_rect(ctx, GRect(gx + GLYPH_W - stroke, top_cy,
                                      stroke, bot_cy - top_cy), 0, GCornerNone);
        // Middle horizontal bar
        graphics_fill_rect(ctx, GRect(gx, mid_y - stroke / 2, GLYPH_W, stroke),
                           0, GCornerNone);
      }
      break;

    case 4:
      // Left stroke: top half only
      graphics_fill_rect(ctx, GRect(gx, top_y, stroke, h / 2), 0, GCornerNone);
      // Right stroke: full height
      graphics_fill_rect(ctx, GRect(gx + GLYPH_W - stroke, top_y, stroke, h),
                         0, GCornerNone);
      // Horizontal bar at midpoint
      graphics_fill_rect(ctx, GRect(gx, cy - stroke / 2, GLYPH_W, stroke),
                         0, GCornerNone);
      break;

    case 5:
      // Top horizontal bar
      graphics_fill_rect(ctx, GRect(gx, top_y, GLYPH_W, stroke), 0, GCornerNone);
      // Top-left vertical stroke (top to mid)
      graphics_fill_rect(ctx, GRect(gx, top_y, stroke, h / 2), 0, GCornerNone);
      // Middle horizontal bar
      graphics_fill_rect(ctx, GRect(gx, cy - stroke / 2, GLYPH_W, stroke),
                         0, GCornerNone);
      // Bottom-right cap (right half of bottom ring)
      fill_arc(ctx, gx + r_outer, bot_cy, r_outer, r_inner, 0, 90);
      // Bottom-right vertical stroke (mid to bot_cy)
      {
        int mid_y = cy + stroke / 2;
        graphics_fill_rect(ctx, GRect(gx + GLYPH_W - stroke, mid_y,
                                      stroke, bot_cy - mid_y), 0, GCornerNone);
        // Bottom cap left half
        fill_arc(ctx, gx + r_outer, bot_cy, r_outer, r_inner, 90, 180);
      }
      break;

    case 6:
      // Like 0 but with top-right open: full left stroke, right stroke only bottom half
      // Top cap left half only
      fill_arc(ctx, gx + r_outer, top_cy, r_outer, r_inner, 180, 360);
      // Full left stroke
      graphics_fill_rect(ctx, GRect(gx, top_cy, stroke, bot_cy - top_cy), 0, GCornerNone);
      // Middle horizontal bar
      graphics_fill_rect(ctx, GRect(gx, cy - stroke / 2, GLYPH_W, stroke),
                         0, GCornerNone);
      // Bottom cap full ring
      fill_arc(ctx, gx + r_outer, bot_cy, r_outer, r_inner, 0, 180);
      // Right stroke bottom half (mid to bot_cy)
      {
        int mid_y = cy + stroke / 2;
        graphics_fill_rect(ctx, GRect(gx + GLYPH_W - stroke, mid_y,
                                      stroke, bot_cy - mid_y), 0, GCornerNone);
      }
      break;

    case 7:
      // Top horizontal bar
      graphics_fill_rect(ctx, GRect(gx, top_y, GLYPH_W, stroke), 0, GCornerNone);
      // Right stroke full height
      graphics_fill_rect(ctx, GRect(gx + GLYPH_W - stroke, top_y, stroke, h),
                         0, GCornerNone);
      break;

    case 8:
      // Full 0 shape plus middle bar
      fill_arc(ctx, gx + r_outer, top_cy, r_outer, r_inner, 180, 360);
      fill_arc(ctx, gx + r_outer, bot_cy, r_outer, r_inner, 0, 180);
      if (body_h > 0) {
        graphics_fill_rect(ctx, GRect(gx, top_cy, stroke, body_h), 0, GCornerNone);
        graphics_fill_rect(ctx, GRect(gx + GLYPH_W - stroke, top_cy, stroke, body_h),
                           0, GCornerNone);
      }
      // Middle horizontal bar
      graphics_fill_rect(ctx, GRect(gx, cy - stroke / 2, GLYPH_W, stroke),
                         0, GCornerNone);
      break;

    case 9:
      // Mirror of 6: top cap full, right stroke top half, left stroke top half,
      // middle bar, full right stroke bottom, bottom-right cap
      fill_arc(ctx, gx + r_outer, top_cy, r_outer, r_inner, 180, 360);
      if (body_h > 0)
        graphics_fill_rect(ctx, GRect(gx, top_cy, stroke, body_h / 2), 0, GCornerNone);
      if (body_h > 0)
        graphics_fill_rect(ctx, GRect(gx + GLYPH_W - stroke, top_cy, stroke, body_h),
                           0, GCornerNone);
      graphics_fill_rect(ctx, GRect(gx, cy - stroke / 2, GLYPH_W, stroke),
                         0, GCornerNone);
      fill_arc(ctx, gx + r_outer, bot_cy, r_outer, r_inner, 0, 90);
      break;

    default:
      break;
  }
}

// Draw vector colon: two square dots centered in slot
static void draw_colon_vec(GContext *ctx, int slot_x, int cy, int size) {
  graphics_context_set_fill_color(ctx, s_fg);
  int h      = digit_outer_h(size);
  int dot_sz = UNIT;
  int gx     = slot_x + HALF_SLOT_PAD + UNIT;  // center the dots in the slot
  // Upper dot: 1/3 of the way up from center
  int upper_y = cy - h / 6 - dot_sz / 2;
  // Lower dot: 1/3 of the way down from center
  int lower_y = cy + h / 6 - dot_sz / 2;
  graphics_fill_rect(ctx, GRect(gx, upper_y, dot_sz, dot_sz), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(gx, lower_y, dot_sz, dot_sz), 0, GCornerNone);
}

// Draw all four digit slots + colon, vector style
// cy: vertical center of digit in screen pixels
static void draw_digits_vec(GContext *ctx, int h_tens, int h_ones,
                            int m_tens, int m_ones, int size, int cy) {
  draw_digit_vec(ctx, h_tens, SLOT_H_TENS, cy, size);
  draw_digit_vec(ctx, h_ones, SLOT_H_ONES, cy, size);
  draw_colon_vec(ctx, COLON_SLOT_X, cy, size);
  draw_digit_vec(ctx, m_tens, SLOT_M_TENS, cy, size);
  draw_digit_vec(ctx, m_ones, SLOT_M_ONES, cy, size);
}

// ============================================================
// INFO LINES
// ============================================================
static const char *s_day_names[]   = { "SUN","MON","TUE","WED","THU","FRI","SAT" };
static const char *s_day_full[]    = { "SUNDAY","MONDAY","TUESDAY","WEDNESDAY",
                                       "THURSDAY","FRIDAY","SATURDAY" };
static const char *s_month_abbrs[] = {
  "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"
};

static int build_comp_above(char lines[][32], int max_lines, struct tm *t) {
  int n = 0;
  if (n < max_lines && t) snprintf(lines[n++], 32, "%s", s_day_full[t->tm_wday]);
  if (n < max_lines && t) snprintf(lines[n++], 32, "%s %d", s_month_abbrs[t->tm_mon], t->tm_mday);
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
    if (strcmp(i18n_get_system_locale(), "en_US") == 0) {
      int mx = (s_distance_m * 10) / 1609;
      snprintf(lines[n++], 32, "%d.%d mi", mx/10, mx%10);
    } else {
      int kx = (s_distance_m * 10) / 1000;
      snprintf(lines[n++], 32, "%d.%d km", kx/10, kx%10);
    }
  }
#endif
  if (n == 0 && n < max_lines)
    snprintf(lines[n++], 32, "bat %d%%%s", s_battery_pct, s_charging ? "+" : "");
  if (n < max_lines && s_heart_rate > 0) snprintf(lines[n++], 32, "%d bpm", s_heart_rate);
  return n;
}

static int build_info_lines_short(char lines[][32], int max_lines, struct tm *t) {
  int n = 0;
  if (n < max_lines && t)
    snprintf(lines[n++], 32, "%s %s %d", s_day_names[t->tm_wday], s_month_abbrs[t->tm_mon], t->tm_mday);
  if (n < max_lines && s_weather_temp[0]) snprintf(lines[n++], 32, "%s", s_weather_temp);
#if defined(PBL_HEALTH)
  if (n < max_lines) {
    if (s_steps >= 1000) snprintf(lines[n++], 32, "%dk steps", s_steps/1000);
    else snprintf(lines[n++], 32, "%d steps", s_steps);
  }
  if (n < max_lines && s_distance_m > 0) {
    if (strcmp(i18n_get_system_locale(), "en_US") == 0) {
      int mx = (s_distance_m * 10) / 1609;
      snprintf(lines[n++], 32, "%d.%d mi", mx/10, mx%10);
    } else {
      int kx = (s_distance_m * 10) / 1000;
      snprintf(lines[n++], 32, "%d.%d km", kx/10, kx%10);
    }
  }
  if (n < max_lines && s_heart_rate > 0) snprintf(lines[n++], 32, "%d bpm", s_heart_rate);
#endif
  if (n < max_lines) snprintf(lines[n++], 32, "bat %d%%", s_battery_pct);
  return n;
}

static GFont prv_info_font(void) {
  return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
}

static void draw_info_block(GContext *ctx, int x, int y, int width,
                            char lines[][32], int n) {
  if (n == 0) return;
  GFont font = prv_info_font();
  graphics_context_set_text_color(ctx, s_fg);
  for (int i = 0; i < n; i++) {
    GRect r = GRect(x, y + i * INFO_LINE_H, width, INFO_FONT_H + 4);
    graphics_draw_text(ctx, lines[i], font, r,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void draw_info_column(GContext *ctx, GRect area, struct tm *t) {
  char lines[6][32];
  int max_fit = area.size.h / INFO_LINE_H;
  int n = build_info_lines_short(lines, max_fit < 6 ? max_fit : 6, t);
  if (n == 0) return;
  int total_h = n * INFO_LINE_H;
  int start_y = area.origin.y + (area.size.h - total_h) / 2;
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

  bool draw_colon = true;

  time_t now_t  = time(NULL);
  struct tm *tm = (s_phase == PHASE_DONE) ? localtime(&now_t) : NULL;

  if (s_layout == LAYOUT_VECTOR) {
    // Pure vector rendering — same position/size logic as LAYOUT_WIDE
    draw_digits_vec(ctx, h_tens, h_ones, m_tens, m_ones, size, center_y);

  } else if (s_layout == LAYOUT_WIDE) {
    int y = center_y - SCREEN_H / 2;
    draw_digits(ctx, h_tens, h_ones, m_tens, m_ones, size, y, draw_colon);

  } else if (s_layout == LAYOUT_WIDE_COMPS) {
    int above_h   = COMP_LINES_ABOVE * INFO_LINE_H;
    int below_h   = COMP_LINES_BELOW * INFO_LINE_H;
    int above_top = ub_top;
    int below_top = ub_top + ub_h - below_h;
    int time_zone_top = above_top + above_h + COMP_GAP;
    int time_zone_bot = below_top - COMP_GAP;
    int time_zone_h   = time_zone_bot - time_zone_top;
    int draw_size = s_demo_override ? size : pick_size(time_zone_h);
    int digit_center = time_zone_top + time_zone_h / 2;
    int y = digit_center - SCREEN_H / 2;
    draw_digits(ctx, h_tens, h_ones, m_tens, m_ones, draw_size, y, draw_colon);
    if (tm) {
      char above_lines[COMP_LINES_ABOVE][32];
      char below_lines[COMP_LINES_BELOW][32];
      int na = build_comp_above(above_lines, COMP_LINES_ABOVE, tm);
      int nb = build_comp_below(below_lines, COMP_LINES_BELOW, tm);
      if (na) draw_info_block(ctx, 0, above_top, SCREEN_W, above_lines, na);
      if (nb) draw_info_block(ctx, 0, below_top, SCREEN_W, below_lines, nb);
    }

  } else {
    // LEFT / RIGHT stacked
    int sdh = stack_digit_h(s_stack_size);
    int h_y = (center_y - sdh / 2 - HALF_UNIT) - SCREEN_H / 2;
    int m_y = (center_y + sdh / 2 + HALF_UNIT) - SCREEN_H / 2;
    int tens_x, ones_x, info_x, info_w;
    if (s_layout == LAYOUT_LEFT) {
      tens_x = SIDE_MARGIN; ones_x = SIDE_MARGIN + SLOT_W;
      info_x = SIDE_MARGIN + SLOT_W * 2 + SIDE_MARGIN;
      info_w = SCREEN_W - info_x - SIDE_MARGIN;
    } else {
      ones_x = SCREEN_W - SIDE_MARGIN - SLOT_W; tens_x = ones_x - SLOT_W;
      info_x = SIDE_MARGIN; info_w = tens_x - SIDE_MARGIN * 2;
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
      s_demo_size  = (frame < GROW_LEN) ? GROW[frame] : SHRINK[frame - GROW_LEN];
      s_demo_digit = s_countdown_digit;
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
          s_size = 6; s_going_down = true; s_anim_rep = 0;
          s_phase = PHASE_BLINK;
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
        s_size--; layer_mark_dirty(s_canvas_layer);
        if (s_size <= 1) { s_size = 1; s_going_down = false; }
        schedule(ANIM_FAST_MS);
      } else {
        s_size++; layer_mark_dirty(s_canvas_layer);
        if (s_size >= WIDE_FULL_SIZE) {
          s_size = WIDE_FULL_SIZE; s_anim_rep++;
          if (s_anim_rep < BLINK_REPS) { s_going_down = true; schedule(ANIM_FAST_MS); }
          else { s_target_size = WIDE_FULL_SIZE; s_phase = PHASE_DONE; layer_mark_dirty(s_canvas_layer); }
        } else { schedule(ANIM_FAST_MS); }
      }
      break;
    case PHASE_SQUISH:
      if (s_going_down) {
        s_size--; layer_mark_dirty(s_canvas_layer);
        if (s_size <= 1) {
          s_size = 1;
          if (s_digit_pending) { s_hour = s_pending_hour; s_minute = s_pending_minute; s_digit_pending = false; }
          s_going_down = false;
        }
        schedule(ANIM_FAST_MS);
      } else {
        s_size++; layer_mark_dirty(s_canvas_layer);
        if (s_size >= s_target_size) { s_size = s_target_size; s_phase = PHASE_DONE; layer_mark_dirty(s_canvas_layer); }
        else { schedule(ANIM_FAST_MS); }
      }
      break;
    case PHASE_SHAKE_CYCLE:
      s_anim_step++;
      if (s_anim_step < SIZE_CYCLE_LEN) {
        s_size = SIZE_CYCLE[s_anim_step]; layer_mark_dirty(s_canvas_layer); schedule(ANIM_FAST_MS);
      } else {
        s_anim_rep++;
        if (s_anim_rep < 2) { s_anim_step = 0; s_size = SIZE_CYCLE[0]; layer_mark_dirty(s_canvas_layer); schedule(ANIM_FAST_MS); }
        else { s_phase = PHASE_DONE; s_size = s_target_size; layer_mark_dirty(s_canvas_layer); }
      }
      break;
    case PHASE_DONE: break;
  }
}

// ============================================================
// EVENT HANDLERS
// ============================================================
static void prv_update_targets(void) {
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  s_stack_size = pick_stack_size(ub.size.h);
  if (s_layout == LAYOUT_WIDE || s_layout == LAYOUT_VECTOR) {
    s_target_size = WIDE_FULL_SIZE;
  } else if (s_layout == LAYOUT_WIDE_COMPS) {
    s_target_size = pick_size(comp_time_space(ub.size.h));
  }
}

static void unobstructed_change(AnimationProgress progress, void *ctx) {
  prv_update_targets();
  if (s_phase == PHASE_DONE &&
      (s_layout == LAYOUT_WIDE || s_layout == LAYOUT_WIDE_COMPS || s_layout == LAYOUT_VECTOR)) {
    s_size = s_target_size;
    layer_mark_dirty(s_canvas_layer);
  }
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_phase != PHASE_DONE) return;
  s_layout = (s_layout + 1) % LAYOUT_COUNT;
  prv_update_targets();
  if (s_layout == LAYOUT_WIDE) {
    s_phase = PHASE_SHAKE_CYCLE;
    s_anim_step = 0; s_anim_rep = 0;
    s_size = SIZE_CYCLE[0];
    schedule(ANIM_FAST_MS);
  } else {
    s_size = s_target_size;
  }
  layer_mark_dirty(s_canvas_layer);
}

static void tick_handler(struct tm *t, TimeUnits units) {
  bool do_squish = (s_layout == LAYOUT_WIDE || s_layout == LAYOUT_WIDE_COMPS ||
                    s_layout == LAYOUT_VECTOR);
  if (s_phase == PHASE_DONE && do_squish) {
    s_pending_hour = t->tm_hour; s_pending_minute = t->tm_min;
    s_digit_pending = true;
    s_phase = PHASE_SQUISH; s_going_down = true;
    schedule(ANIM_FAST_MS);
  } else if (s_phase == PHASE_SQUISH) {
    s_pending_hour = t->tm_hour; s_pending_minute = t->tm_min;
    s_digit_pending = true;
  } else {
    s_hour = t->tm_hour; s_minute = t->tm_min;
    layer_mark_dirty(s_canvas_layer);
  }
}

static void battery_handler(BatteryChargeState state) {
  s_battery_pct = state.charge_percent; s_charging = state.is_charging;
  layer_mark_dirty(s_canvas_layer);
}

static void bt_handler(bool connected) {
  s_bt_connected = connected; layer_mark_dirty(s_canvas_layer);
}

#if defined(PBL_HEALTH)
static void health_handler(HealthEventType event, void *context) {
  if (event == HealthEventMovementUpdate || event == HealthEventSignificantUpdate) {
    HealthServiceAccessibilityMask mask;
    mask = health_service_metric_accessible(HealthMetricStepCount, time_start_of_today(), time(NULL));
    s_steps = (mask & HealthServiceAccessibilityMaskAvailable) ? (int)health_service_sum_today(HealthMetricStepCount) : 0;
    mask = health_service_metric_accessible(HealthMetricWalkedDistanceMeters, time_start_of_today(), time(NULL));
    s_distance_m = (mask & HealthServiceAccessibilityMaskAvailable) ? (int)health_service_sum_today(HealthMetricWalkedDistanceMeters) : 0;
  }
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_DIORITE)
  if (event == HealthEventHeartRateUpdate) {
    HealthServiceAccessibilityMask mask = health_service_metric_accessible(HealthMetricHeartRateBPM, time(NULL), time(NULL)+1);
    s_heart_rate = (mask & HealthServiceAccessibilityMaskAvailable) ? (int)health_service_peek_current_value(HealthMetricHeartRateBPM) : 0;
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
  if (t && !s_weather_temp[0]) snprintf(s_weather_temp, sizeof(s_weather_temp), "%dC", (int)t->value->int32);
  t = dict_find(iter, MESSAGE_KEY_WeatherCode);
  if (t) {
    int code = (int)t->value->int32;
    const char *desc = "WEATHER";
    if (code == 0) desc = "CLEAR";
    else if (code <= 3) desc = "CLOUDY";
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
  s_fg = GColorWhite; s_bg = GColorBlack;
  memset(s_bitmaps, 0, sizeof(s_bitmaps));
  memset(s_colon_bm, 0, sizeof(s_colon_bm));
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

int main(void) { init(); app_event_loop(); deinit(); }
