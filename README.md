# TallBoy

A Pebble watchface displaying HH:MM using custom vector-drawn digits.

Digits are constructed from uniform-stroke primitives (rectangles + filled circles for caps) making them fully scalable — change the bounding box height and the digits grow or shrink cleanly, with caps always perfectly circular.

**Design:** Sterling Ely  
**Implementation:** Sterling Ely + Claude

## Architecture

- `src/digit.h` / `src/digit.c` — standalone digit drawing library
- `src/main.c` — watchface layout and time display

## Digit geometry

Each digit is drawn with:
- Uniform stroke weight `s`
- Semicircular end caps (radius = `s/2`) on all free-floating terminals
- Square joints where strokes meet structural edges
- The "1" uses a diagonal chamfer serif at top-left

Height is a free parameter — only the vertical body segments change length.
