#include <pebble.h>

// ============================================================
// TallBoy — main.c  v0.8
//
// Supports all rect platforms:
//   emery  (200x228): slot=40px, unit=8
//   others (144x168): slot=30px, unit=6
//
// BEHAVIORS:
//   On minute change: quick squish (size 6→1→6, 80ms/frame)
//   On shake:         2 fast size cycles (80ms/frame)
//   Quick View:       pick_size() tracks unobstructed height
//
// LAUNCH DEMO SEQUENCE:
//   5s  real time, size 6
//   10s digit cycle 0-9, 1s each
//   5s  real time, size 6
//   2x  slow size cycle (1000ms/frame)
//   2x  fast size cycle (80ms/frame)
//   done — real time, responsive sizing
// ============================================================

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
#else
  // aplite, basalt, diorite, flint: 144x168
  #define SCREEN_W      144
  #define SCREEN_H      168
  #define SLOT_W         30
  #define COLON_SLOT_X   57
  #define SLOT_H_TENS     6
  #define SLOT_H_ONES    36
  #define SLOT_M_TENS    78
  #define SLOT_M_ONES   108
#endif

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

// ---- Draw ----
static void draw_layer(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, s_bg);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  int size = s_size;
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

  draw_digit(ctx, h_tens, size, SLOT_H_TENS);
  draw_digit(ctx, h_ones, size, SLOT_H_ONES);
  draw_colon(ctx, size);
  draw_digit(ctx, m_tens, size, SLOT_M_TENS);
  draw_digit(ctx, m_ones, size, SLOT_M_ONES);
}

// ---- Timer ----
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

// ---- Quick View ----
static void unobstructed_change(AnimationProgress progress, void *ctx) {
  Layer *root = window_get_root_layer(s_window);
  GRect ub = layer_get_unobstructed_bounds(root);
  s_target_size = pick_size(ub.size.h);
  if (s_phase == PHASE_DONE) {
    s_size = s_target_size;
    layer_mark_dirty(s_canvas_layer);
  }
}

// ---- Shake ----
static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_phase != PHASE_DONE) return;
  s_phase = PHASE_SHAKE_CYCLE;
  s_anim_step = 0; s_anim_rep = 0;
  s_size = SIZE_CYCLE[0];
  layer_mark_dirty(s_canvas_layer);
  schedule(ANIM_FAST_MS);
}

// ---- Tick ----
static void tick_handler(struct tm *t, TimeUnits units) {
  s_hour   = t->tm_hour;
  s_minute = t->tm_min;
  if (s_phase == PHASE_DONE) {
    s_phase = PHASE_SQUISH;
    s_anim_step = 0;
    s_size = SQUISH_DOWN[0];
    schedule(ANIM_FAST_MS);
  }
  layer_mark_dirty(s_canvas_layer);
}

// ---- Lifecycle ----
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
