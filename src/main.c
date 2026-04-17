#include <pebble.h>

// ============================================================
// TallBoy — main.c  v0.6
//
// DEMO SEQUENCE:
//   5s   real time, size 6
//   10s  digit cycle 0-9, 1s each
//   5s   real time, size 6
//   2x   slow size cycle (1000ms/frame): 6→5→4→3→2→1→2→3→4→5→6
//   2x   fast size cycle ( 100ms/frame): 6→5→4→3→2→1→2→3→4→5→6
//   done — real time, size 6
// ============================================================

#define SCREEN_W   200
#define SCREEN_H   228
#define SLOT_W      40

#define SLOT_H_TENS   12
#define SLOT_H_ONES   52
#define SLOT_M_TENS  108
#define SLOT_M_ONES  148
#define COLON_SLOT_X  80

typedef enum {
  PHASE_TIME_1,
  PHASE_DIGIT_CYCLE,
  PHASE_TIME_2,
  PHASE_SIZE_SLOW,
  PHASE_SIZE_FAST,
  PHASE_DONE
} DemoPhase;

#define DEMO_TIME_MS      5000
#define DEMO_DIGIT_MS     1000
#define DEMO_SIZE_SLOW_MS 1000
#define DEMO_SIZE_FAST_MS  100
#define DEMO_SIZE_SLOW_REPS 2
#define DEMO_SIZE_FAST_REPS 2

static const int SIZE_CYCLE[] = { 6, 5, 4, 3, 2, 1, 2, 3, 4, 5, 6 };
#define SIZE_CYCLE_LEN 11

static Window    *s_window;
static Layer     *s_canvas_layer;
static int        s_hour    = 0;
static int        s_minute  = 0;
static int        s_size    = 6;
static GColor     s_fg, s_bg;
static DemoPhase  s_demo_phase = PHASE_TIME_1;
static int        s_demo_digit = 0;
static int        s_size_step  = 0;
static int        s_size_rep   = 0;
static AppTimer  *s_demo_timer = NULL;

// ============================================================
// BITMAPS
// ============================================================
static GBitmap *s_bitmaps[10][6];
static GBitmap *s_colon_bm[6];

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

  if (s_demo_phase == PHASE_DIGIT_CYCLE) {
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
// DEMO TIMER — shared handler for both slow and fast size cycles
// ============================================================
static void demo_timer_cb(void *data);

static void schedule_demo(uint32_t ms) {
  if (s_demo_timer) app_timer_cancel(s_demo_timer);
  s_demo_timer = app_timer_register(ms, demo_timer_cb, NULL);
}

// Advance one step of the size cycle. Returns true if cycle is complete.
static bool size_cycle_step(uint32_t frame_ms, int max_reps) {
  s_size_step++;
  if (s_size_step < SIZE_CYCLE_LEN) {
    s_size = SIZE_CYCLE[s_size_step];
    layer_mark_dirty(s_canvas_layer);
    schedule_demo(frame_ms);
    return false;
  } else {
    s_size_rep++;
    if (s_size_rep < max_reps) {
      s_size_step = 0;
      s_size = SIZE_CYCLE[0];
      layer_mark_dirty(s_canvas_layer);
      schedule_demo(frame_ms);
      return false;
    }
    return true;
  }
}

static void demo_timer_cb(void *data) {
  s_demo_timer = NULL;
  switch (s_demo_phase) {

    case PHASE_TIME_1:
      s_demo_phase = PHASE_DIGIT_CYCLE;
      s_demo_digit = 0;
      layer_mark_dirty(s_canvas_layer);
      schedule_demo(DEMO_DIGIT_MS);
      break;

    case PHASE_DIGIT_CYCLE:
      s_demo_digit++;
      if (s_demo_digit < 10) {
        layer_mark_dirty(s_canvas_layer);
        schedule_demo(DEMO_DIGIT_MS);
      } else {
        s_demo_phase = PHASE_TIME_2;
        s_size = 6;
        layer_mark_dirty(s_canvas_layer);
        schedule_demo(DEMO_TIME_MS);
      }
      break;

    case PHASE_TIME_2:
      s_demo_phase = PHASE_SIZE_SLOW;
      s_size_step = 0;
      s_size_rep  = 0;
      s_size = SIZE_CYCLE[0];
      layer_mark_dirty(s_canvas_layer);
      schedule_demo(DEMO_SIZE_SLOW_MS);
      break;

    case PHASE_SIZE_SLOW:
      if (size_cycle_step(DEMO_SIZE_SLOW_MS, DEMO_SIZE_SLOW_REPS)) {
        // Slow done — start fast
        s_demo_phase = PHASE_SIZE_FAST;
        s_size_step = 0;
        s_size_rep  = 0;
        s_size = SIZE_CYCLE[0];
        layer_mark_dirty(s_canvas_layer);
        schedule_demo(DEMO_SIZE_FAST_MS);
      }
      break;

    case PHASE_SIZE_FAST:
      if (size_cycle_step(DEMO_SIZE_FAST_MS, DEMO_SIZE_FAST_REPS)) {
        s_demo_phase = PHASE_DONE;
        s_size = 6;
        layer_mark_dirty(s_canvas_layer);
      }
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
