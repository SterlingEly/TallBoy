#include <pebble.h>

// ============================================================
// TallBoy — main.c  v0.4
//
// HH:MM watchface — pure raster PNG digits, emery only.
// Vector drawing removed; layout and sizing focus.
//
// LAYOUT (emery 200×228):
//   Unit U = 8px. 25U = 200px exactly.
//   Each digit slot = 5U (40px) wide, containing:
//     4px left margin + 32px digit + 4px right margin
//   Colon slot = 1U (8px) wide, centered
//
//   Layout (L→R):
//     2U margin | 5U H_tens | 1U gap | 5U H_ones | 1U gap |
//     1U colon | 1U gap | 5U M_tens | 1U gap | 5U M_ones | 2U margin
//     = 2+5+1+5+1+1+1+5+1+5+2 = 29U ... too wide.
//
//   Actual Sterling layout (confirmed from pixel files):
//     The digit files are 40px (5U) wide and include their own
//     4px half-margin on each side. Adjacent slots butt up together
//     with no extra gap — the half-margins from adjacent slots create
//     the 8px (1U) inter-digit gap naturally.
//
//   So layout is just: margin + 4 slots + colon slot + margin
//   where slots are placed edge-to-edge.
//
//   Colon: 8px wide, positioned in the center gap between digit pairs.
//   From pixel analysis: colon center x = 100 (screen center).
//
//   Slot positions (left edge of each 40px slot):
//     H_tens: (200 - 4*40 - 8) / 2 = (200 - 168) / 2 = 16
//     H_ones: 16 + 40 = 56
//     [colon centered at 56+40+4 = 100, i.e. screen center]
//     M_tens: 56 + 40 + 8 = 104   (8px colon gap)
//     M_ones: 104 + 40 = 144
//     Right edge: 144 + 40 = 184. Right margin = 200-184 = 16. ✓
//
// SIZES: 6 levels matching Sterling's asset sizes.
//   Files are 40×228 — full screen height, digit centered vertically.
//   The size is baked into the asset; we select the right file.
//   For now: always use size 6 (largest, fills screen).
//   TODO: pick size dynamically based on available vertical space.
//
// DEMO SEQUENCE (auto on launch):
//   5s real time → 10s digit cycle (0-9, 1s each) → 5s real time → done
// ============================================================

#define SCREEN_W   200
#define SCREEN_H   228
#define SLOT_W      40   // each digit file slot width (5U)
#define COLON_W      8   // colon gap width (1U)

// Horizontal slot positions (left edge of each 40px slot)
#define SLOT_H_TENS   16
#define SLOT_H_ONES   56
#define SLOT_M_TENS  104
#define SLOT_M_ONES  144

// Colon center x = midpoint of the 8px gap between H_ones and M_tens
// H_ones right edge = 56+40 = 96. M_tens left edge = 104. Center = 100.
#define COLON_CX     100

// Colon slot left edge: position the 40px colon file so its internal
// colon content (at cols 16-23 of the file = center col 19.5) lands on COLON_CX.
// colon_slot_x = COLON_CX - 19 = 81 ... but let's think about it differently:
// The colon file has its dots centered at x=20 within the 40px slot.
// So slot_x = COLON_CX - 20 = 80.
#define COLON_SLOT_X  80

// Demo phases
typedef enum { PHASE_TIME_1, PHASE_CYCLE, PHASE_TIME_2, PHASE_DONE } DemoPhase;
#define DEMO_TIME_MS   5000
#define DEMO_DIGIT_MS  1000

static Window    *s_window;
static Layer     *s_canvas_layer;
static int        s_hour    = 0;
static int        s_minute  = 0;
static int        s_size    = 6;
static GColor     s_fg, s_bg;
static DemoPhase  s_demo_phase = PHASE_TIME_1;
static int        s_demo_digit = 0;
static AppTimer  *s_demo_timer = NULL;

// ============================================================
// BITMAPS
// ============================================================
static GBitmap *s_bitmaps[10][6];
static GBitmap *s_colon_bm[6];

// Resource table [digit 0-9][size 1-6]
// digit 2: sizes 1-3 are the shared style, 4-5 are 2B variant, no size 6.
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

// Find nearest available size, preferring exact match, then larger, then smaller
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

// Draw a digit file at (slot_x, 0). The file is 40×228 and self-contained —
// the digit is already vertically centered within the 228px height by Sterling's
// design, so we just blit the full file at the correct x position.
static void draw_digit(GContext *ctx, int digit, int size, int slot_x) {
  GBitmap *bm = get_bitmap(digit, size);
  if (!bm) return;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, bm, GRect(slot_x, 0, SLOT_W, SCREEN_H));
}

static void draw_colon(GContext *ctx, int size) {
  GBitmap *bm = get_colon(size);
  if (!bm) return;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, bm, GRect(COLON_SLOT_X, 0, SLOT_W, SCREEN_H));
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

  draw_digit(ctx, h_tens, size, SLOT_H_TENS);
  draw_digit(ctx, h_ones, size, SLOT_H_ONES);
  draw_colon(ctx, size);
  draw_digit(ctx, m_tens, size, SLOT_M_TENS);
  draw_digit(ctx, m_ones, size, SLOT_M_ONES);
}

// ============================================================
// DEMO TIMER
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
      s_demo_phase = PHASE_CYCLE;
      s_demo_digit = 0;
      layer_mark_dirty(s_canvas_layer);
      schedule_demo(DEMO_DIGIT_MS);
      break;
    case PHASE_CYCLE:
      s_demo_digit++;
      if (s_demo_digit < 10) {
        layer_mark_dirty(s_canvas_layer);
        schedule_demo(DEMO_DIGIT_MS);
      } else {
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
  schedule_demo(DEMO_TIME_MS);
}

static void window_unload(Window *window) {
  if (s_demo_timer) { app_timer_cancel(s_demo_timer); s_demo_timer = NULL; }
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
