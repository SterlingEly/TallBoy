#pragma once
#include <pebble.h>

// ============================================================
// TallBoy digit drawing library
//
// Draws digits 0-9 as vector shapes using uniform-stroke
// geometry: straight segments + semicircular end caps.
//
// Each digit is parameterized by:
//   x, y   — top-left of bounding box
//   w      — width of bounding box
//   h      — height of bounding box
//   s      — stroke weight (must be even; radius = s/2)
//   color  — fill color
//
// The cap radius is always s/2. Vertical body segments are
// (h - s) tall. Horizontal bodies are (w - s) wide. This
// means all caps stay circular regardless of h.
// ============================================================

void digit_draw(GContext *ctx, int digit, int x, int y, int w, int h, int s, GColor color);

// Draw the HH:MM colon — two filled circles
void digit_draw_colon(GContext *ctx, int x, int cy, int s, GColor color);
