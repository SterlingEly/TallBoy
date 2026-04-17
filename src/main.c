#include <pebble.h>

// ============================================================
// TallBoy — main.c  v0.1
//
// HH:MM watchface using Sterling Ely's custom digit font.
// Target: emery (200×228) only.
//
// DIGIT ASSIGNMENT:
//   H_tens  (0 or 1 only) — VECTOR drawn in C
//   H_ones  (0–9)         — RASTER PNG bitmap
//   colon                 — RASTER PNG bitmap (size-matched)
//   M_tens  (0–5 only)    — RASTER PNG bitmap
//   M_ones  (0–9)         — VECTOR drawn in C
//
// DEMO MODE: shake to enter/exit. Cycles all positions 0→9
// at 1-second intervals to verify both raster and vector digits.
//
// GEOMETRY (all in pixels, 1:1 emery scale):
//   Unit U = 8px
//   Screen: 200×228
//   Digit content width: 32px (4U), file slot: 40px (5U, ½U margin each side)
//   6 size levels: outer_h = 24 + 32*size, centered vertically
//   Cap outer radius = 16px (2U), inner radius = 8px (1U)
//   Horizontal layout (25U × 8px = 200px):
//     margin(2U) + digit(4U) + gap(1U) + digit(4U) + gap(1U) +
//     colon(1U) + gap(1U) + digit(4U) + gap(1U) + digit(4U) + margin(2U)
//   Slot x positions: H_tens=16, H_ones=64, colon_cx=116, M_tens=128, M_ones=176
// ============================================================

#define SCREEN_W    200
#define SCREEN_H    228
#define U             8   // 1 unit = 8px
#define DIGIT_W      32   // digit content width (4U)
#define FILE_W       40   // file slot width (5U, includes ½U margins)
#define CAP_OUTER_R  16   // 2U
#define CAP_INNER_R   8   // 1U

// Horizontal slot left-edge positions (left edge of 40px file slot)
#define SLOT_H_TENS   16
#define SLOT_H_ONES   64
#define SLOT_M_TENS  128
#define SLOT_M_ONES  176
#define COLON_CX     116  // colon center x (1U wide, centered in gap)

// Within each 40px slot, digit strokes occupy:
//   Left stroke:  x = slot + 4  .. slot + 11  (8px)
//   Right stroke: x = slot + 28 .. slot + 35  (8px)
//   Inner gap:    x = slot + 12 .. slot + 27  (16px)
//   Digit center: x = slot + 20 (for arc centers — between col 19 and 20)
#define SLOT_LEFT_X(slot)   ((slot) + 4)
#define SLOT_RIGHT_X(slot)  ((slot) + 28)
#define SLOT_CX(slot)       ((slot) + 20)

// Size helpers
static int outer_h(int size)     { return 24 + 32 * size; }
static int top_margin(int size)  { return (SCREEN_H - outer_h(size)) / 2; }
static int mid_y(int size)       { return top_margin(size) + outer_h(size) / 2; }

// ============================================================
// STATE
// ============================================================
static Window    *s_window;
static Layer     *s_canvas_layer;

static int        s_hour   = 0;
static int        s_minute = 0;
static int        s_size   = 6;

// Demo mode
static bool       s_demo        = false;
static int        s_demo_digit  = 0;
static AppTimer  *s_demo_timer  = NULL;

// Colors
static GColor     s_fg;
static GColor     s_bg;

// Bitmaps: [digit 0-9][size 1-6], loaded lazily
static GBitmap   *s_bitmaps[10][6];

// Colon bitmaps: [size 1-6]
static GBitmap   *s_colon_bm[6];

// ============================================================
// RESOURCE ID TABLE
// Resource names: IMG_<dd>_<s>  e.g. IMG_00_1 = digit 0 size 1
// Missing sizes: digit 2 only has sizes 4,5; digit 8 only 2-6.
// ============================================================
static const uint32_t s_res[10][6] = {
  { RESOURCE_ID_IMG_00_1, RESOURCE_ID_IMG_00_2, RESOURCE_ID_IMG_00_3,
    RESOURCE_ID_IMG_00_4, RESOURCE_ID_IMG_00_5, RESOURCE_ID_IMG_00_6 },
  { RESOURCE_ID_IMG_01_1, RESOURCE_ID_IMG_01_2, RESOURCE_ID_IMG_01_3,
    RESOURCE_ID_IMG_01_4, RESOURCE_ID_IMG_01_5, RESOURCE_ID_IMG_01_6 },
  { 0, 0, 0,
    RESOURCE_ID_IMG_02_4, RESOURCE_ID_IMG_02_5, 0 },
  { RESOURCE_ID_IMG_03_1, RESOURCE_ID_IMG_03_2, RESOURCE_ID_IMG_03_3,
    RESOURCE_ID_IMG_03_4, RESOURCE_ID_IMG_03_5, RESOURCE_ID_IMG_03_6 },
  { RESOURCE_ID_IMG_04_1, RESOURCE_ID_IMG_04_2, RESOURCE_ID_IMG_04_3,
    RESOURCE_ID_IMG_04_4, RESOURCE_ID_IMG_04_5, RESOURCE_ID_IMG_04_6 },
  { RESOURCE_ID_IMG_05_1, RESOURCE_ID_IMG_05_2, RESOURCE_ID_IMG_05_3,
    RESOURCE_ID_IMG_05_4, RESOURCE_ID_IMG_05_5, RESOURCE_ID_IMG_05_6 },
  { RESOURCE_ID_IMG_06_1, RESOURCE_ID_IMG_06_2, RESOURCE_ID_IMG_06_3,
    RESOURCE_ID_IMG_06_4, RESOURCE_ID_IMG_06_5, RESOURCE_ID_IMG_06_6 },
  { RESOURCE_ID_IMG_07_1, RESOURCE_ID_IMG_07_2, RESOURCE_ID_IMG_07_3,
    RESOURCE_ID_IMG_07_4, RESOURCE_ID_IMG_07_5, RESOURCE_ID_IMG_07_6 },
  { 0, RESOURCE_ID_IMG_08_2, RESOURCE_ID_IMG_08_3,
    RESOURCE_ID_IMG_08_4, RESOURCE_ID_IMG_08_5, RESOURCE_ID_IMG_08_6 },
  { RESOURCE_ID_IMG_09_1, RESOURCE_ID_IMG_09_2, RESOURCE_ID_IMG_09_3,
    RESOURCE_ID_IMG_09_4, RESOURCE_ID_IMG_09_5, RESOURCE_ID_IMG_09_6 },
};

static const uint32_t s_colon_res[6] = {
  RESOURCE_ID_IMG_COLON_1, RESOURCE_ID_IMG_COLON_2, RESOURCE_ID_IMG_COLON_3,
  RESOURCE_ID_IMG_COLON_4, RESOURCE_ID_IMG_COLON_5, RESOURCE_ID_IMG_COLON_6,
};

// Find the nearest available size >= requested, then <
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

// ============================================================
// RASTER DRAW
// The PNG files are 40×228 content (may export with extra white
// padding — we crop via GRect to exactly 40×228 at (slot_x, 0)).
// ============================================================
static void draw_raster(GContext *ctx, int digit, int size, int slot_x) {
  GBitmap *bm = get_bitmap(digit, size);
  if (!bm) return;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, bm, GRect(slot_x, 0, FILE_W, SCREEN_H));
}

static void draw_colon_raster(GContext *ctx, int size) {
  GBitmap *bm = get_colon(size);
  if (!bm) return;
  // Colon file is also 40px wide but the colon itself is in the center 8px.
  // Position it so its center lands on COLON_CX.
  // Colon stroke occupies cols 16-23 of the 40px slot → center = 19.5 within slot.
  // We want that center at x=COLON_CX=116. So slot_x = 116 - 20 = 96.
  int colon_slot_x = COLON_CX - 20;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, bm, GRect(colon_slot_x, 0, FILE_W, SCREEN_H));
}

// ============================================================
// VECTOR DRAW: digit 0 and digit 1
//
// draw_vector_digit(ctx, digit, slot_x, size, fg, bg)
// Only digits 0 and 1 are implemented (H_tens is always 0 or 1,
// M_ones needs full 0-9 — to be done next iteration).
// ============================================================

static void fr(GContext *ctx, int x, int y, int w, int h) {
  if (w > 0 && h > 0) graphics_fill_rect(ctx, GRect(x, y, w, h), 0, GCornerNone);
}

static void fc(GContext *ctx, int cx, int cy, int r) {
  graphics_fill_circle(ctx, GPoint(cx, cy), r);
}

// Annular arc cap: draws the rounded end of digit 0 (and shared by 3,6,8,9).
// cap_cy = Y coordinate of the arc's center circle.
// top=true → cap opens downward (top of digit), top=false → opens upward (bottom).
static void draw_arc_cap(GContext *ctx, int slot_x, int cap_cy, bool is_top,
                         GColor fg, GColor bg) {
  int cx = SLOT_CX(slot_x);  // horizontal center of digit

  graphics_context_set_fill_color(ctx, fg);
  if (is_top) {
    // Fill the upper half of the outer circle + rect above it
    fr(ctx, SLOT_LEFT_X(slot_x), cap_cy - CAP_OUTER_R, DIGIT_W, CAP_OUTER_R);
    fc(ctx, cx, cap_cy, CAP_OUTER_R);
    // Punch inner hole (upper half + rect)
    graphics_context_set_fill_color(ctx, bg);
    fr(ctx, SLOT_LEFT_X(slot_x) + U, cap_cy - CAP_INNER_R, DIGIT_W - 2*U, CAP_INNER_R);
    fc(ctx, cx, cap_cy, CAP_INNER_R);
  } else {
    // Fill the lower half of the outer circle + rect below it
    fr(ctx, SLOT_LEFT_X(slot_x), cap_cy, DIGIT_W, CAP_OUTER_R);
    fc(ctx, cx, cap_cy, CAP_OUTER_R);
    // Punch inner hole
    graphics_context_set_fill_color(ctx, bg);
    fr(ctx, SLOT_LEFT_X(slot_x) + U, cap_cy, DIGIT_W - 2*U, CAP_INNER_R);
    fc(ctx, cx, cap_cy, CAP_INNER_R);
  }
}

static void draw_vector_0(GContext *ctx, int slot_x, int size, GColor fg, GColor bg) {
  int tm   = top_margin(size);
  int oh   = outer_h(size);
  int tc_y = tm + CAP_OUTER_R;           // top cap center y
  int bc_y = tm + oh - CAP_OUTER_R;      // bottom cap center y

  graphics_context_set_fill_color(ctx, fg);

  // Left and right vertical body between cap centers
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
  int tm  = top_margin(size);
  int oh  = outer_h(size);
  int top = tm;
  int bot = tm + oh - 1;

  // Main vertical stroke: occupies cols 16–23 within the 40px slot
  // = slot_x + 16 .. slot_x + 23
  int sx = slot_x + 16;

  graphics_context_set_fill_color(ctx, fg);
  fr(ctx, sx, top, U, oh);

  // Diagonal serif: fixed 25-row shape at top of digit.
  // Measured pixel-exactly from tallboy_1x.png files.
  // Phase 1 (rows 0–12 from top): solid diagonal, left edge slides
  //   from col (sx-1) leftward 1px/row until reaching slot_x+4.
  // Phase 2 (rows 13–14): peak, left=slot_x+4, right=slot_x+13 (held 2 rows).
  // Phase 3 (rows 15–24): right edge slides from slot_x+13 rightward 1px/row
  //   back toward slot_x+4 until gone.
  for (int r = 0; r < 25; r++) {
    int y = top + r;
    if (y >= SCREEN_H) break;
    int left_x, right_x;
    if (r <= 12) {
      left_x  = sx - 1 - r;             // slides left from col 15
      right_x = sx - 1;                  // = slot_x + 15
      if (left_x < slot_x + 4) left_x = slot_x + 4;
    } else if (r <= 14) {
      left_x  = slot_x + 4;
      right_x = slot_x + 13;
    } else {
      left_x  = slot_x + 4;
      right_x = slot_x + 13 - (r - 14); // slides right (shrinks) from col 13
      if (right_x < left_x) break;
    }
    fr(ctx, left_x, y, right_x - left_x + 1, 1);
  }

  // Bottom foot: full digit width (32px), 8px tall, bottom-aligned
  fr(ctx, SLOT_LEFT_X(slot_x), bot - U + 1, DIGIT_W, U);
}

// Dispatch: only 0 and 1 are vector-implemented.
// For M_ones (full 0-9), we fall back to raster for digits 2-9 until
// vector versions are added.
static void draw_vector_digit(GContext *ctx, int digit, int slot_x, int size,
                              GColor fg, GColor bg) {
  switch (digit) {
    case 0: draw_vector_0(ctx, slot_x, size, fg, bg); break;
    case 1: draw_vector_1(ctx, slot_x, size, fg, bg); break;
    default:
      // Fallback to raster for unimplemented vector digits
      draw_raster(ctx, digit, size, slot_x);
      break;
  }
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

  // H_tens: vector (always 0 or 1 in 12h mode)
  draw_vector_digit(ctx, h_tens, SLOT_H_TENS, size, s_fg, s_bg);

  // H_ones: raster
  draw_raster(ctx, h_ones, size, SLOT_H_ONES);

  // Colon: raster
  draw_colon_raster(ctx, size);

  // M_tens: raster
  draw_raster(ctx, m_tens, size, SLOT_M_TENS);

  // M_ones: vector (0 and 1 done; 2-9 fall back to raster)
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
    s_demo       = true;
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
  for (int d = 0; d < 10; d++)
    for (int s = 0; s < 6; s++)
      if (s_bitmaps[d][s]) { gbitmap_destroy(s_bitmaps[d][s]); s_bitmaps[d][s] = NULL; }
  for (int s = 0; s < 6; s++)
    if (s_colon_bm[s]) { gbitmap_destroy(s_colon_bm[s]); s_colon_bm[s] = NULL; }
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
