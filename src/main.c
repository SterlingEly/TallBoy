#include <pebble.h>
#include "digit.h"

// ============================================================
// TallBoy — main.c
// HH:MM watchface using custom vector digit drawing.
// Rect platforms only (aplite, basalt, diorite, emery, flint).
// ============================================================

#define SETTINGS_KEY 1

// Layout constants — all derived from screen dimensions at runtime
// but these are the proportions for a 144×168 reference screen.
//
// Digit layout: [HH] [colon] [MM]
// Two digits per group, each digit W_DIGIT wide.
// Colon takes COLON_W.
// Total: 4*W_DIGIT + COLON_W + gaps = screen width

// Stroke weight as fraction of digit width (1/4 of digit width)
#define STROKE_DENOM 4

// Digit width as fraction of screen width
// On 144px: 4 digits + colon + 5 gaps
// We target digit width = ~28px, gap = ~6px, colon = ~10px
// 4*28 + 10 + 5*6 = 112 + 10 + 30 = 152 -- a hair over, scale down
// Let's use 26px digits, 8px colon, 4px gaps:
// 4*26 + 8 + 5*4 = 104 + 8 + 20 = 132 (12px margin each side = 6px)
// These are computed dynamically below.

static Window *s_window;
static Layer  *s_canvas_layer;

static int s_hour   = 0;
static int s_minute = 0;

// Foreground color (white on B&W or color)
static GColor col_fg;
static GColor col_bg;

static void draw_layer(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int W = bounds.size.w;
  int H = bounds.size.h;

  // Background
  graphics_context_set_fill_color(ctx, col_bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // ---- Layout arithmetic ----------------------------------------
  // We want the digits to be as tall as possible while fitting
  // horizontally. Start with a target digit width and derive height.
  //
  // Digit aspect ratio from design: the reference design shows
  // digits roughly 1:2 wide:tall (excluding serifs). We'll use
  // a configurable ratio but start at W:H = 1:2.2 so that at
  // digit_w=26 we get digit_h=57 (fits well on 168px screen).
  //
  // Horizontal allocation:
  //   margin*2 + 4*digit_w + colon_w + 4*gap = W
  //   colon_w  = digit_w / 2
  //   gap      = digit_w / 5  (between digit groups and colon)
  //   margin is what's left
  //
  // We solve for digit_w:
  //   4*dw + dw/2 + 4*(dw/5) = W - 2*margin_min
  //   dw * (4 + 0.5 + 0.8) = W - margin_min*2
  //   dw * 5.3 ≈ W - 12
  //   dw = (W - 12) / 5.3  →  multiply by 10/53 in integer
  //
  // Integer version: dw = (W - 12) * 10 / 53
  int dw = (W - 12) * 10 / 53;  // digit width
  if (dw < 14) dw = 14;
  // Round dw to even (stroke weight = dw/4 works cleanly on even widths)
  if (dw % 2 != 0) dw--;

  int dh     = dw * 22 / 10;      // digit height ~2.2× width
  int cw     = dw / 2;            // colon zone width
  int gap    = dw / 5;            // inter-element gap
  int stroke = dw / STROKE_DENOM; // stroke weight
  if (stroke < 3) stroke = 3;
  if (stroke % 2 != 0) stroke++;  // keep even for clean radius

  // Total horizontal span
  int total_w = 4 * dw + cw + 4 * gap;
  int left    = (W - total_w) / 2;  // left margin

  // Vertical centering
  int top = (H - dh) / 2;

  // X positions of each digit and colon center
  int x0 = left;                           // tens of hours
  int x1 = x0 + dw + gap;                 // ones of hours
  int xc = x1 + dw + gap;                 // colon zone left
  int x2 = xc + cw + gap;                 // tens of minutes
  int x3 = x2 + dw + gap;                 // ones of minutes
  int colon_cx = xc + cw / 2;             // colon dot center x

  // Digits
  int h_tens = s_hour   / 10;
  int h_ones = s_hour   % 10;
  int m_tens = s_minute / 10;
  int m_ones = s_minute % 10;

  digit_draw(ctx, h_tens, x0, top, dw, dh, stroke, col_fg);
  digit_draw(ctx, h_ones, x1, top, dw, dh, stroke, col_fg);
  digit_draw(ctx, m_tens, x2, top, dw, dh, stroke, col_fg);
  digit_draw(ctx, m_ones, x3, top, dw, dh, stroke, col_fg);

  // Colon
  digit_draw_colon(ctx, colon_cx, top + dh / 2, stroke, col_fg);
}

static void tick_handler(struct tm *t, TimeUnits units_changed) {
  s_hour   = t->tm_hour;
  s_minute = t->tm_min;
  layer_mark_dirty(s_canvas_layer);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, draw_layer);
  layer_add_child(root, s_canvas_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

static void init(void) {
#if defined(PBL_BW)
  col_bg = GColorBlack;
  col_fg = GColorWhite;
#else
  col_bg = GColorBlack;
  col_fg = GColorWhite;
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
