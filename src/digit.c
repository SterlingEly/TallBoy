#include "digit.h"

// ============================================================
// Primitive helpers
// ============================================================

// Filled rectangle (top-left origin)
static void frect(GContext *ctx, int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  graphics_fill_rect(ctx, GRect(x, y, w, h), 0, GCornerNone);
}

// Filled circle at center (cx, cy) with radius r
static void fcirc(GContext *ctx, int cx, int cy, int r) {
  if (r <= 0) return;
  graphics_fill_circle(ctx, GPoint(cx, cy), r);
}

// Horizontal stroke: left-to-right bar of height s, capped on left/right with semicircles.
// x,y = top-left of bounding rect; w = total span including caps; s = stroke weight.
static void hstroke(GContext *ctx, int x, int y, int w, int s) {
  int r = s / 2;
  // Body rect between the two cap centers
  frect(ctx, x + r, y, w - s, s);
  fcirc(ctx, x + r,     y + r, r);
  fcirc(ctx, x + w - r, y + r, r);
}

// Horizontal stroke, left cap only (right end is a structural join — square)
static void hstroke_lcap(GContext *ctx, int x, int y, int w, int s) {
  int r = s / 2;
  frect(ctx, x + r, y, w - r, s);
  fcirc(ctx, x + r, y + r, r);
}

// Horizontal stroke, right cap only
static void hstroke_rcap(GContext *ctx, int x, int y, int w, int s) {
  int r = s / 2;
  frect(ctx, x, y, w - r, s);
  fcirc(ctx, x + w - r, y + r, r);
}

// Horizontal stroke, no caps (square both ends — interior join)
static void hstroke_square(GContext *ctx, int x, int y, int w, int s) {
  frect(ctx, x, y, w, s);
}

// Vertical stroke: top-to-bottom bar of width s, capped top/bottom.
// x,y = top-left; h = total span including caps; s = stroke weight.
static void vstroke(GContext *ctx, int x, int y, int h, int s) {
  int r = s / 2;
  frect(ctx, x, y + r, s, h - s);
  fcirc(ctx, x + r, y + r,     r);
  fcirc(ctx, x + r, y + h - r, r);
}

// Vertical stroke, top cap only
static void vstroke_tcap(GContext *ctx, int x, int y, int h, int s) {
  int r = s / 2;
  frect(ctx, x, y + r, s, h - r);
  fcirc(ctx, x + r, y + r, r);
}

// Vertical stroke, bottom cap only
static void vstroke_bcap(GContext *ctx, int x, int y, int h, int s) {
  int r = s / 2;
  frect(ctx, x, y, s, h - r);
  fcirc(ctx, x + r, y + h - r, r);
}

// Vertical stroke, no caps
static void vstroke_square(GContext *ctx, int x, int y, int h, int s) {
  frect(ctx, x, y, s, h);
}

// ============================================================
// Digit geometry
//
// For all digits: x,y = top-left of bounding box, w,h = size.
// s = stroke weight. All positions derived from these.
//
// Key coordinates:
//   left edge:   x
//   right edge:  x + w - s  (left edge of rightmost stroke)
//   mid y:       y + h/2 - s/2  (top of crossbar at vertical midpoint)
//   bottom:      y + h - s  (top of bottom stroke body)
// ============================================================

static void draw_0(GContext *ctx, int x, int y, int w, int h, int s) {
  // Left vertical, full height, both caps
  vstroke(ctx, x, y, h, s);
  // Right vertical, full height, both caps
  vstroke(ctx, x + w - s, y, h, s);
  // Top connector (no caps — joins the two verticals)
  int top_inner_w = w - 2*s;
  if (top_inner_w > 0)
    hstroke_square(ctx, x + s, y, top_inner_w, s);
  // Bottom connector
  if (top_inner_w > 0)
    hstroke_square(ctx, x + s, y + h - s, top_inner_w, s);
}

static void draw_1(GContext *ctx, int x, int y, int w, int h, int s) {
  // Single centered vertical, full height, both caps
  int cx = x + w / 2 - s / 2;
  vstroke(ctx, cx, y, h, s);
  // Diagonal serif at top-left: a 45-deg chamfer suggestion.
  // Drawn as a short diagonal stroke going from the top of the
  // vertical upward-left. We approximate with a filled triangle.
  // The serif extends s units left and s units up from the top of the stroke.
  int sx = cx;           // base of serif = top of vertical, left edge
  int sy = y + s / 2;    // top of vertical body (below cap center)
  // Diagonal line: 1px wide antialiased is unavailable on Pebble,
  // so draw as a filled right-triangle GPath.
  GPoint pts[3] = {
    GPoint(sx,          sy),
    GPoint(sx - s,      sy - s),
    GPoint(sx + s / 2,  sy),
  };
  GPathInfo info = { .num_points = 3, .points = pts };
  GPath *path = gpath_create(&info);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

static void draw_2(GContext *ctx, int x, int y, int w, int h, int s) {
  int mid_y = y + h / 2 - s / 2;
  // Top horizontal — right cap, left end joins left edge (square)
  // Spans full width, right-capped
  hstroke_rcap(ctx, x, y, w, s);
  // Right vertical, upper half only — from top stroke down to midline
  int upper_h = mid_y - y;
  if (upper_h > 0)
    vstroke_square(ctx, x + w - s, y + s, upper_h - s, s);
  // Middle horizontal — full width, square both ends (touches both verticals)
  hstroke_square(ctx, x, mid_y, w, s);
  // Left vertical, lower half — from midline to bottom
  int lower_h = (y + h) - mid_y - s;
  if (lower_h > 0)
    vstroke_square(ctx, x, mid_y + s, lower_h, s);
  // Bottom horizontal — left cap, right end square
  hstroke_lcap(ctx, x, y + h - s, w, s);
}

static void draw_3(GContext *ctx, int x, int y, int w, int h, int s) {
  int mid_y = y + h / 2 - s / 2;
  // Right vertical, full height, both caps
  vstroke(ctx, x + w - s, y, h, s);
  // Top horizontal — left cap, right end joins vertical (square)
  hstroke_lcap(ctx, x, y, w, s);
  // Middle horizontal — left cap
  hstroke_lcap(ctx, x, mid_y, w, s);
  // Bottom horizontal — left cap
  hstroke_lcap(ctx, x, y + h - s, w, s);
}

static void draw_4(GContext *ctx, int x, int y, int w, int h, int s) {
  int mid_y = y + h / 2 - s / 2;
  // Left vertical, upper half — top cap, bottom square at midline
  int upper_h = mid_y - y + s;
  vstroke_tcap(ctx, x, y, upper_h, s);
  // Right vertical, full height, both caps
  vstroke(ctx, x + w - s, y, h, s);
  // Middle horizontal — square both ends (structural join)
  hstroke_square(ctx, x + s, mid_y, w - s, s);
}

static void draw_5(GContext *ctx, int x, int y, int w, int h, int s) {
  int mid_y = y + h / 2 - s / 2;
  // Top horizontal — right cap (left end square at left edge)
  hstroke_rcap(ctx, x, y, w, s);
  // Left vertical, upper half — from top stroke to midline
  int upper_h = mid_y - y - s;
  if (upper_h > 0)
    vstroke_square(ctx, x, y + s, upper_h, s);
  // Middle horizontal — full width square (structural join)
  hstroke_square(ctx, x, mid_y, w, s);
  // Right vertical, lower half — from midline to near bottom
  int lower_h = (y + h - s) - (mid_y + s);
  if (lower_h > 0)
    vstroke_square(ctx, x + w - s, mid_y + s, lower_h, s);
  // Bottom horizontal — left cap
  hstroke_lcap(ctx, x, y + h - s, w, s);
}

static void draw_6(GContext *ctx, int x, int y, int w, int h, int s) {
  int mid_y = y + h / 2 - s / 2;
  // Left vertical, full height, both caps
  vstroke(ctx, x, y, h, s);
  // Top horizontal — right cap (attaches to top of left vertical)
  hstroke_rcap(ctx, x + s, y, w - s, s);
  // Middle horizontal — right end square (joins right vertical)
  hstroke_square(ctx, x + s, mid_y, w - s, s);
  // Right vertical, lower half
  int lower_h = (y + h) - mid_y;
  vstroke_square(ctx, x + w - s, mid_y, lower_h, s);
  // Bottom horizontal — square (covered by left + right vertical bottoms)
  int bot_inner_w = w - 2 * s;
  if (bot_inner_w > 0)
    hstroke_square(ctx, x + s, y + h - s, bot_inner_w, s);
}

static void draw_7(GContext *ctx, int x, int y, int w, int h, int s) {
  // Top horizontal — right cap (left end square)
  hstroke_rcap(ctx, x, y, w, s);
  // Right vertical — top square (joins top bar), bottom cap
  vstroke_bcap(ctx, x + w - s, y, h, s);
}

static void draw_8(GContext *ctx, int x, int y, int w, int h, int s) {
  int mid_y = y + h / 2 - s / 2;
  // Left vertical, full height, both caps
  vstroke(ctx, x, y, h, s);
  // Right vertical, full height, both caps
  vstroke(ctx, x + w - s, y, h, s);
  // Top connector
  int inner_w = w - 2 * s;
  if (inner_w > 0) {
    hstroke_square(ctx, x + s, y,          inner_w, s);
    hstroke_square(ctx, x + s, mid_y,      inner_w, s);
    hstroke_square(ctx, x + s, y + h - s,  inner_w, s);
  }
}

static void draw_9(GContext *ctx, int x, int y, int w, int h, int s) {
  int mid_y = y + h / 2 - s / 2;
  // Right vertical, full height, both caps
  vstroke(ctx, x + w - s, y, h, s);
  // Left vertical, upper half — top cap, bottom square at midline
  int upper_h = mid_y - y + s;
  vstroke_tcap(ctx, x, y, upper_h, s);
  // Top horizontal connector
  int inner_w = w - 2 * s;
  if (inner_w > 0)
    hstroke_square(ctx, x + s, y, inner_w, s);
  // Middle horizontal — left cap
  hstroke_lcap(ctx, x, mid_y, w, s);
}

// ============================================================
// Public API
// ============================================================

void digit_draw(GContext *ctx, int digit, int x, int y, int w, int h, int s, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_context_set_stroke_color(ctx, color);
  switch (digit) {
    case 0: draw_0(ctx, x, y, w, h, s); break;
    case 1: draw_1(ctx, x, y, w, h, s); break;
    case 2: draw_2(ctx, x, y, w, h, s); break;
    case 3: draw_3(ctx, x, y, w, h, s); break;
    case 4: draw_4(ctx, x, y, w, h, s); break;
    case 5: draw_5(ctx, x, y, w, h, s); break;
    case 6: draw_6(ctx, x, y, w, h, s); break;
    case 7: draw_7(ctx, x, y, w, h, s); break;
    case 8: draw_8(ctx, x, y, w, h, s); break;
    case 9: draw_9(ctx, x, y, w, h, s); break;
    default: break;
  }
}

void digit_draw_colon(GContext *ctx, int x, int cy, int s, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  int r = s / 2;
  int spacing = s * 3 / 2;  // vertical gap between the two dots
  fcirc(ctx, x, cy - spacing / 2, r);
  fcirc(ctx, x, cy + spacing / 2, r);
}
