#include <pebble.h>

// ============================================================
// TallBoy — main.c  v0.2
//
// HH:MM watchface using Sterling Ely's custom digit font.
// Targets: emery (200×228) and flint (144×168, vector-only).
//
// DIGIT ASSIGNMENT (emery):
//   H_tens (0,1 only) — VECTOR
//   H_ones (0–9)      — RASTER PNG
//   colon             — RASTER PNG
//   M_tens (0–5)      — RASTER PNG
//   M_ones (0–9)      — VECTOR (0,1 done; 2–9 fall back to raster)
//
// FLINT: no raster (wrong size/platform) — pure vector, 0 and 1 only for now.
//
// DEMO MODE: shake cycles all positions 0→9 at 1-second intervals.
//
// GEOMETRY:
//   Unit U = 8px on emery, 5px on flint.
//   25 units wide: margin(2) digit(4) gap(1) digit(4) gap(1)
//                  colon(1) gap(1) digit(4) gap(1) digit(4) margin(2)
//   Emery: 25×8 = 200px ✓   Flint: 25×5 = 125px, ~9px margin each side.
//   6 size levels: outer_h = (3 + 4*size) * U
//   Cap outer radius = 2U, inner radius = 1U, stroke = 1U.
// ============================================================

// ---- Platform geometry ----
#if defined(PBL_PLATFORM_EMERY)
  #define SCREEN_W   200
  #define SCREEN_H   228
  #define U            8
#else
  // flint (and any other rect non-emery)
  #define SCREEN_W   144
  #define SCREEN_H   168
  #define U            5
#endif

#define DIGIT_W      (4*U)   // digit content width
#define FILE_W       (5*U)   // file slot (digit + half-margin each side)
#define CAP_OUTER_R  (2*U)
#define CAP_INNER_R  (1*U)

// Horizontal layout — computed from unit size
// Left margin = (SCREEN_W - 25*U) / 2
#define LAYOUT_MARGIN  ((SCREEN_W - 25*U) / 2)
#define SLOT_H_TENS    (LAYOUT_MARGIN)
#define SLOT_H_ONES    (LAYOUT_MARGIN + FILE_W + U)
#define SLOT_M_TENS    (LAYOUT_MARGIN + FILE_W + U + FILE_W + U + U + U)
#define SLOT_M_ONES    (LAYOUT_MARGIN + FILE_W + U + FILE_W + U + U + U + FILE_W + U)
#define COLON_CX       (LAYOUT_MARGIN + FILE_W + U + FILE_W + U + U/2 + (U%2))

// Within a slot: left stroke at +U offset, right stroke at +3U, center at +2U+U/2
#define SLOT_LEFT_X(s)  ((s) + U)
#define SLOT_RIGHT_X(s) ((s) + 3*U)
#define SLOT_CX(s)      ((s) + 2*U + U/2)

// Size helpers (all sizes share same formula, just different U)
static int digit_outer_h(int size)    { return (3 + 4*size) * U; }
static int digit_top_margin(int size) { return (SCREEN_H - digit_outer_h(size)) / 2; }

// ============================================================
// STATE
// ============================================================
static Window   *s_window;
static Layer    *s_canvas_layer;
static int       s_hour   = 0;
static int       s_minute = 0;
static int       s_size   = 6;
static bool      s_demo       = false;
static int       s_demo_digit = 0;
static AppTimer *s_demo_timer = NULL;
static GColor    s_fg, s_bg;

// ============================================================
// RASTER RESOURCES (emery only)
// Resource names match filenames: tallboy_XY.png -> TALLBOY_XY
// Lookup table: s_res[digit][size-1], 0 = not available.
// Digit 2: sizes 1-3 available (tallboy_21/22/23), sizes 4-5 use 2B variant.
// ============================================================
#if defined(PBL_PLATFORM_EMERY)

static GBitmap *s_bitmaps[10][6];
static GBitmap *s_colon_bm[6];

static const uint32_t s_res[10][6] = {
  { RESOURCE_ID_TALLBOY_01, RESOURCE_ID_TALLBOY_02, RESOURCE_ID_TALLBOY_03,
    RESOURCE_ID_TALLBOY_04, RESOURCE_ID_TALLBOY_05, RESOURCE_ID_TALLBOY_06 },
  { RESOURCE_ID_TALLBOY_11, RESOURCE_ID_TALLBOY_12, RESOURCE_ID_TALLBOY_13,
    RESOURCE_ID_TALLBOY_14, RESOURCE_ID_TALLBOY_15, RESOURCE_ID_TALLBOY_16 },
  { RESOURCE_ID_TALLBOY_21, RESOURCE_ID_TALLBOY_22, RESOURCE_ID_TALLBOY_23,
    RESOURCE_ID_TALLBOY_2B4, RESOURCE_ID_TALLBOY_2B5, 0 },
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

static GBitmap *get_colon_bm(int size) {
  int si = size - 1;
  if (!s_colon_bm[si])
    s_colon_bm[si] = gbitmap_create_with_resource(s_colon_res[si]);
  return s_colon_bm[si];
}

static void draw_raster(GContext *ctx, int digit, int size, int slot_x) {
  GBitmap *bm = get_bitmap(digit, size);
  if (!bm) return;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  // Files are 40x228 content. Blit at (slot_x, 0) covering full screen height.
  graphics_draw_bitmap_in_rect(ctx, bm, GRect(slot_x, 0, FILE_W, SCREEN_H));
}

static void draw_colon_raster(GContext *ctx, int size) {
  GBitmap *bm = get_colon_bm(size);
  if (!bm) return;
  // Colon content is in cols 16-23 of a 40px slot (center = col 19.5).
  // Position so that center lands on COLON_CX.
  int colon_slot_x = COLON_CX - (2*U + U/2);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, bm, GRect(colon_slot_x, 0, FILE_W, SCREEN_H));
}

static void free_bitmaps(void) {
  for (int d = 0; d < 10; d++)
    for (int s = 0; s < 6; s++)
      if (s_bitmaps[d][s]) { gbitmap_destroy(s_bitmaps[d][s]); s_bitmaps[d][s] = NULL; }
  for (int s = 0; s < 6; s++)
    if (s_colon_bm[s]) { gbitmap_destroy(s_colon_bm[s]); s_colon_bm[s] = NULL; }
}

#endif // PBL_PLATFORM_EMERY

// ============================================================
// VECTOR DRAWING
// Parameterised entirely on U — works on any platform.
// draw_arc_cap, draw_vector_0, draw_vector_1.
// The digit 1 serif is 25px at emery (U=8). At flint (U=5) it
// scales proportionally: 25*5/8 ≈ 16 rows (rounded).
// ============================================================

static void fr(GContext *ctx, int x, int y, int w, int h) {
  if (w > 0 && h > 0) graphics_fill_rect(ctx, GRect(x, y, w, h), 0, GCornerNone);
}
static void fc(GContext *ctx, int cx, int cy, int r) {
  graphics_fill_circle(ctx, GPoint(cx, cy), r);
}

static void draw_arc_cap(GContext *ctx, int slot_x, int cap_cy, bool is_top,
                         GColor fg, GColor bg) {
  int cx = SLOT_CX(slot_x);
  graphics_context_set_fill_color(ctx, fg);
  if (is_top) {
    fr(ctx, SLOT_LEFT_X(slot_x), cap_cy - CAP_OUTER_R, DIGIT_W, CAP_OUTER_R);
    fc(ctx, cx, cap_cy, CAP_OUTER_R);
    graphics_context_set_fill_color(ctx, bg);
    fr(ctx, SLOT_LEFT_X(slot_x) + U, cap_cy - CAP_INNER_R, DIGIT_W - 2*U, CAP_INNER_R);
    fc(ctx, cx, cap_cy, CAP_INNER_R);
  } else {
    fr(ctx, SLOT_LEFT_X(slot_x), cap_cy, DIGIT_W, CAP_OUTER_R);
    fc(ctx, cx, cap_cy, CAP_OUTER_R);
    graphics_context_set_fill_color(ctx, bg);
    fr(ctx, SLOT_LEFT_X(slot_x) + U, cap_cy, DIGIT_W - 2*U, CAP_INNER_R);
    fc(ctx, cx, cap_cy, CAP_INNER_R);
  }
}

static void draw_vector_0(GContext *ctx, int slot_x, int size, GColor fg, GColor bg) {
  int tm   = digit_top_margin(size);
  int oh   = digit_outer_h(size);
  int tc_y = tm + CAP_OUTER_R;
  int bc_y = tm + oh - CAP_OUTER_R;
  graphics_context_set_fill_color(ctx, fg);
  int body_h = bc_y - tc_y;
  if (body_h > 0) {
    fr(ctx, SLOT_LEFT_X(slot_x),  tc_y, U, body_h);
    fr(ctx, SLOT_RIGHT_X(slot_x), tc_y, U, body_h);
  }
  draw_arc_cap(ctx, slot_x, tc_y, true,  fg, bg);
  draw_arc_cap(ctx, slot_x, bc_y, false, fg, bg);
}

static void draw_vector_1(GContext *ctx, int slot_x, int size, GColor fg, GColor bg) {
  (void)bg;
  int tm  = digit_top_margin(size);
  int oh  = digit_outer_h(size);
  int top = tm;
  int bot = tm + oh - 1;

  // Main stroke: centered in the slot, 1U wide.
  // At emery (U=8): cols 16-23 of 40px slot = slot_x+16.
  // General: slot_x + 2*U  (left edge of stroke, which is 2U from slot left).
  int sx = slot_x + 2*U;

  graphics_context_set_fill_color(ctx, fg);
  fr(ctx, sx, top, U, oh);

  // Diagonal serif: pixel-exact at U=8 (emery); scaled proportionally at U=5 (flint).
  // At U=8: serif spans 25 rows, reaches 12px left of stroke, foot is 32px wide 8px tall.
  // At any U: scale all offsets by U/8, keeping serif visually proportional.
  // We do this with integer arithmetic: pixel_offset = (emery_px * U + 4) / 8
  // Precomputed key values at U=8:
  //   stroke left = sx (= slot_x + 2U)
  //   serif peak left = slot_x + U/2    (half-unit from slot edge)
  //   serif total rows = 25 * U / 8
  //   phase1 rows = 13 * U / 8
  //   phase2 rows = 2  * U / 8  (hold at peak)
  //   phase3 rows = 10 * U / 8  (taper)
  //   peak right = sx - U/8            (just left of stroke, ~1px at U=8)

  int serif_peak_left = slot_x + U/2;
  int serif_peak_right = sx - 1;  // right edge of serif at peak = left of stroke gap
  int total_serif_rows = 25 * U / 8;
  int phase1_rows = 13 * U / 8;
  int peak_width = serif_peak_right - serif_peak_left + 1;

  for (int r = 0; r < total_serif_rows; r++) {
    int y = top + r;
    if (y >= SCREEN_H) break;
    int left_x, right_x;
    if (r < phase1_rows) {
      // Phase 1: left edge slides from stroke-1 toward serif_peak_left
      right_x = serif_peak_right;
      left_x  = serif_peak_right - r;
      if (left_x < serif_peak_left) left_x = serif_peak_left;
    } else {
      // Phase 2+3: right edge tapers from serif_peak_right toward serif_peak_left
      left_x  = serif_peak_left;
      int taper = r - phase1_rows;
      right_x = serif_peak_right - taper;
      if (right_x < left_x) break;
    }
    fr(ctx, left_x, y, right_x - left_x + 1, 1);
  }

  // Bottom foot: full digit width (4U), 1U tall, bottom-aligned
  fr(ctx, SLOT_LEFT_X(slot_x), bot - U + 1, DIGIT_W, U);
}

static void draw_vector_digit(GContext *ctx, int digit, int slot_x, int size,
                              GColor fg, GColor bg) {
  switch (digit) {
    case 0: draw_vector_0(ctx, slot_x, size, fg, bg); break;
    case 1: draw_vector_1(ctx, slot_x, size, fg, bg); break;
#if defined(PBL_PLATFORM_EMERY)
    default: draw_raster(ctx, digit, size, slot_x); break;
#else
    default: break;  // flint: unimplemented digits blank for now
#endif
  }
}

// Vector colon: two filled circles, scaled by U.
// Centers at screen_h/2 ± (outer_h - U) / 4
static void draw_colon_vector(GContext *ctx, int size) {
  int oh = digit_outer_h(size);
  int half_spacing = (oh - U) / 4;
  int dot_r = U / 2;
  int cy = SCREEN_H / 2;
  graphics_context_set_fill_color(ctx, s_fg);
  fc(ctx, COLON_CX, cy - half_spacing, dot_r);
  fc(ctx, COLON_CX, cy + half_spacing, dot_r);
}

// ============================================================
// DRAW LAYER
// ============================================================
static void draw_layer(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, s_bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  int size = s_size;
  int h_tens, h_ones, m_tens, m_ones;
  if (s_demo) {
    h_tens = h_ones = m_tens = m_ones = s_demo_digit;
  } else {
    int h = s_hour % 12;
    if (h == 0) h = 12;
    h_tens = h / 10;
    h_ones = h % 10;
    m_tens = s_minute / 10;
    m_ones = s_minute % 10;
  }

  draw_vector_digit(ctx, h_tens, SLOT_H_TENS, size, s_fg, s_bg);

#if defined(PBL_PLATFORM_EMERY)
  draw_raster(ctx, h_ones, size, SLOT_H_ONES);
  draw_colon_raster(ctx, size);
  draw_raster(ctx, m_tens, size, SLOT_M_TENS);
#else
  draw_vector_digit(ctx, h_ones, SLOT_H_ONES, size, s_fg, s_bg);
  draw_colon_vector(ctx, size);
  draw_vector_digit(ctx, m_tens, SLOT_M_TENS, size, s_fg, s_bg);
#endif

  draw_vector_digit(ctx, m_ones, SLOT_M_ONES, size, s_fg, s_bg);
}

// ============================================================
// DEMO MODE
// ============================================================
static void demo_next(void *data) {
  s_demo_digit = (s_demo_digit + 1) % 10;
  layer_mark_dirty(s_canvas_layer);
  s_demo_timer = app_timer_register(1000, demo_next, NULL);
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (!s_demo) {
    s_demo = true;
    s_demo_digit = 0;
    s_demo_timer = app_timer_register(1000, demo_next, NULL);
  } else {
    s_demo = false;
    if (s_demo_timer) { app_timer_cancel(s_demo_timer); s_demo_timer = NULL; }
  }
  layer_mark_dirty(s_canvas_layer);
}

// ============================================================
// TICK / LIFECYCLE
// ============================================================
static void tick_handler(struct tm *t, TimeUnits units) {
  s_hour   = t->tm_hour;
  s_minute = t->tm_min;
  layer_mark_dirty(s_canvas_layer);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas_layer, draw_layer);
  layer_add_child(root, s_canvas_layer);
  accel_tap_service_subscribe(accel_tap_handler);
}

static void window_unload(Window *window) {
  accel_tap_service_unsubscribe();
  if (s_demo_timer) { app_timer_cancel(s_demo_timer); s_demo_timer = NULL; }
#if defined(PBL_PLATFORM_EMERY)
  free_bitmaps();
#endif
  layer_destroy(s_canvas_layer);
}

static void init(void) {
  s_fg = GColorWhite;
  s_bg = GColorBlack;
#if defined(PBL_PLATFORM_EMERY)
  memset(s_bitmaps,  0, sizeof(s_bitmaps));
  memset(s_colon_bm, 0, sizeof(s_colon_bm));
#endif
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  time_t now = time(NULL);
  tick_handler(localtime(&now), MINUTE_UNIT);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
