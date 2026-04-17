#include <pebble.h>

// ============================================================
// TallBoy — main.c  v0.3
//
// HH:MM watchface using Sterling Ely's custom digit font.
// Targets: emery (200×228) and flint (144×168, vector-only).
//
// DEMO SEQUENCE (auto, no interaction needed):
//   5s  — show real time
//   10s — cycle digits 0→9, 1s each, all positions in sync
//   5s  — show real time again
//   (then stays on real time)
//
// DIGIT ASSIGNMENT (emery):
//   H_tens (0,1) — VECTOR
//   H_ones (0–9) — RASTER
//   colon        — RASTER
//   M_tens (0–5) — RASTER
//   M_ones (0–9) — VECTOR (0,1 done; 2–9 fall back to raster)
//
// FLINT: pure vector, only 0 and 1 drawn; others blank.
//
// GEOMETRY:
//   Unit U = 8px emery, 5px flint.
//   25U wide layout. 6 sizes: outer_h = (3+4*size)*U.
//   Cap outer radius = 2U, inner = 1U, stroke = 1U.
// ============================================================

#if defined(PBL_PLATFORM_EMERY)
  #define SCREEN_W  200
  #define SCREEN_H  228
  #define U           8
#else
  #define SCREEN_W  144
  #define SCREEN_H  168
  #define U           5
#endif

#define DIGIT_W      (4*U)
#define FILE_W       (5*U)
#define CAP_OUTER_R  (2*U)
#define CAP_INNER_R  (1*U)

#define LAYOUT_MARGIN  ((SCREEN_W - 25*U) / 2)
#define SLOT_H_TENS    (LAYOUT_MARGIN)
#define SLOT_H_ONES    (LAYOUT_MARGIN + FILE_W + U)
#define SLOT_M_TENS    (LAYOUT_MARGIN + FILE_W + U + FILE_W + U + U + U)
#define SLOT_M_ONES    (LAYOUT_MARGIN + FILE_W + U + FILE_W + U + U + U + FILE_W + U)
#define COLON_CX       (LAYOUT_MARGIN + FILE_W + U + FILE_W + U + U/2 + (U%2))

#define SLOT_LEFT_X(s)  ((s) + U)
#define SLOT_RIGHT_X(s) ((s) + 3*U)
#define SLOT_CX(s)      ((s) + 2*U + U/2)

static int digit_outer_h(int size)    { return (3 + 4*size) * U; }
static int digit_top_margin(int size) { return (SCREEN_H - digit_outer_h(size)) / 2; }

// ============================================================
// DEMO STATE
// Phase 0: show real time (5s)
// Phase 1: cycle digits 0-9 (10s = 10 × 1s)
// Phase 2: show real time (5s), then stay
// ============================================================
#define DEMO_TIME_MS   5000
#define DEMO_DIGIT_MS  1000
#define DEMO_DIGITS    10

typedef enum { PHASE_TIME_1, PHASE_CYCLE, PHASE_TIME_2, PHASE_DONE } DemoPhase;

static Window    *s_window;
static Layer     *s_canvas_layer;
static int        s_hour   = 0;
static int        s_minute = 0;
static int        s_size   = 6;
static GColor     s_fg, s_bg;
static DemoPhase  s_demo_phase  = PHASE_TIME_1;
static int        s_demo_digit  = 0;
static AppTimer  *s_demo_timer  = NULL;

// ============================================================
// RASTER (emery only)
// ============================================================
#if defined(PBL_PLATFORM_EMERY)

static GBitmap *s_bitmaps[10][6];
static GBitmap *s_colon_bm[6];

// digit 2: sizes 1-3 share the same design, sizes 4-5 use 2B variant.
// Resource names: TALLBOY_<digit><size>, no separator.
static const uint32_t s_res[10][6] = {
  { RESOURCE_ID_TALLBOY_01, RESOURCE_ID_TALLBOY_02, RESOURCE_ID_TALLBOY_03,
    RESOURCE_ID_TALLBOY_04, RESOURCE_ID_TALLBOY_05, RESOURCE_ID_TALLBOY_06 },
  { RESOURCE_ID_TALLBOY_11, RESOURCE_ID_TALLBOY_12, RESOURCE_ID_TALLBOY_13,
    RESOURCE_ID_TALLBOY_14, RESOURCE_ID_TALLBOY_15, RESOURCE_ID_TALLBOY_16 },
  { RESOURCE_ID_TALLBOY_21, RESOURCE_ID_TALLBOY_22, RESOURCE_ID_TALLBOY_23,
    RESOURCE_ID_TALLBOY_24, RESOURCE_ID_TALLBOY_25, 0 },
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
  graphics_draw_bitmap_in_rect(ctx, bm, GRect(slot_x, 0, FILE_W, SCREEN_H));
}

static void draw_colon_raster(GContext *ctx, int size) {
  GBitmap *bm = get_colon_bm(size);
  if (!bm) return;
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
// VECTOR DRAWING (all platforms)
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
  int sx  = slot_x + 2*U;  // main stroke left edge (2U from slot left)

  graphics_context_set_fill_color(ctx, fg);
  fr(ctx, sx, top, U, oh);

  // Diagonal serif, scaled by U.
  // At U=8 (emery): 25 rows total, peak 12px left of stroke.
  // At U=5 (flint): 25*5/8 = ~15 rows total, peak 12*5/8 = ~7px left.
  int serif_left  = slot_x + U/2;            // leftmost extent of serif
  int serif_right = sx - 1;                  // right edge = just left of stroke
  int total_rows  = 25 * U / 8;
  int phase1_rows = 13 * U / 8;

  for (int r = 0; r < total_rows; r++) {
    int y = top + r;
    if (y >= SCREEN_H) break;
    int lx, rx;
    if (r < phase1_rows) {
      rx = serif_right;
      lx = serif_right - r;
      if (lx < serif_left) lx = serif_left;
    } else {
      lx = serif_left;
      rx = serif_right - (r - phase1_rows);
      if (rx < lx) break;
    }
    fr(ctx, lx, y, rx - lx + 1, 1);
  }

  // Bottom foot: 4U wide, 1U tall
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
    default: break;
#endif
  }
}

static void draw_colon_vector(GContext *ctx, int size) {
  int oh = digit_outer_h(size);
  int half_spacing = (oh - U) / 4;
  graphics_context_set_fill_color(ctx, s_fg);
  fc(ctx, COLON_CX, SCREEN_H/2 - half_spacing, U/2);
  fc(ctx, COLON_CX, SCREEN_H/2 + half_spacing, U/2);
}

// ============================================================
// DRAW LAYER
// ============================================================
static void draw_layer(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, s_bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  int size = s_size;

  // Determine what digits to show
  int h_tens, h_ones, m_tens, m_ones;
  if (s_demo_phase == PHASE_CYCLE) {
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
// DEMO TIMER SEQUENCE
// ============================================================
static void demo_timer_cb(void *data);

static void schedule_demo(uint32_t ms) {
  if (s_demo_timer) app_timer_cancel(s_demo_timer);
  s_demo_timer = app_timer_register(ms, demo_timer_cb, NULL);
}

static void demo_timer_cb(void *data) {
  s_demo_timer = NULL;
  switch (s_demo_phase) {
    case PHASE_TIME_1:
      // Time shown for 5s — now start cycling
      s_demo_phase = PHASE_CYCLE;
      s_demo_digit = 0;
      layer_mark_dirty(s_canvas_layer);
      schedule_demo(DEMO_DIGIT_MS);
      break;
    case PHASE_CYCLE:
      s_demo_digit++;
      if (s_demo_digit < DEMO_DIGITS) {
        layer_mark_dirty(s_canvas_layer);
        schedule_demo(DEMO_DIGIT_MS);
      } else {
        // Done cycling — show real time for 5s then stay
        s_demo_phase = PHASE_TIME_2;
        layer_mark_dirty(s_canvas_layer);
        schedule_demo(DEMO_TIME_MS);
      }
      break;
    case PHASE_TIME_2:
      s_demo_phase = PHASE_DONE;
      layer_mark_dirty(s_canvas_layer);
      break;
    case PHASE_DONE:
      break;
  }
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
  // Kick off demo sequence
  schedule_demo(DEMO_TIME_MS);
}

static void window_unload(Window *window) {
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
