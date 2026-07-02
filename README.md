# TallBoy

A Pebble watchface built around oversized vector-drawn digits that dynamically scale to fill the screen — squishing and expanding as an animation mechanic on every minute change.

**Design:** Sterling Ely
**Implementation:** Sterling Ely + Claude
**Primary device:** Pebble Time 2 (emery, 200×228, color)

---

> **Developers and AI collaborators:** Read [`PROJECT_CONTEXT.md`](PROJECT_CONTEXT.md) before making any code changes. It contains the current version status, architecture reference, critical build rules, known traps, and TODO list.

---

## How it works

Everything lives in `src/main.c`. Digits are drawn entirely from geometric primitives (`fill_arc` + `graphics_fill_rect`) — no bitmaps, no system fonts for digit rendering.

Each digit occupies a **5×UNIT slot** and is drawn with arc caps (outer radius 2u, inner radius 1u) that remain constant while vertical stems stretch with the digit height `h`. At minimum height (`H_MIN = 11u`), the arcs just touch and bars disappear.

Digits 1, 2, and 7 use diagonal strokes via `gpath_create` — these allocate/free on every draw frame with null checks in place.

---

## Layouts

Three display modes, accessible by shake (or always-on via config):

| Layout | Description |
|--------|-------------|
| Full | Full-screen time; anticipate → squish → expand animation each minute |
| Wide | Up to 3 info slots above + 3 below time; digits fill remaining space; empty slots collapse |
| Stacked L / R | Stacked hour/minute digits on one side, up to 8 info slots in a column on the other |

Round platforms (chalk, gabbro) support Full mode only. Per-column digit heights follow the circular bezel geometry — outer columns shorter, inner columns taller, no clipping.

---

## Info slots

Info lines display health, weather, solar, and status data. **Data caching philosophy: hide > mislead** — if data is unavailable or stale, the line disappears entirely rather than showing zeros or stale values.

28 slot types are available (IDs 0–28). See [`PROJECT_CONTEXT.md`](PROJECT_CONTEXT.md) for the complete slot table and formatting details.

---

## Step pace background color

On color platforms with health data, the background reflects today's step pace vs. a typical day (same day-of-week average from the last 4 weeks via Pebble Health API):

| Pace | Color |
|------|-------|
| No data / 0% | Black |
| ≤ 30% | Red |
| ≤ 60% | Orange |
| ≤ 90% | Yellow |
| ≤ 200% | Green |
| ≤ 400% | Blue |
| ≤ 700% | Indigo |
| ≤ 1000% | Vivid Violet |

---

## Weather & solar data

Fetched by PebbleKit JS (`src/pkjs/index.js`) on watch connect and every 30 minutes, using the [Open-Meteo API](https://open-meteo.com/) (no API key required).

Weather/UV data ages out after 3 hours without an update. Sunrise/sunset never ages out.

---

## Target platforms

aplite, basalt, chalk, diorite, emery, flint, gabbro — all 7 Pebble platforms.

---

## Build

Import `SterlingEly/TallBoy` (branch: `main`) into [CloudPebble](https://cloudpebble.net) and build. See [`PROJECT_CONTEXT.md`](PROJECT_CONTEXT.md) for CloudPebble-specific rules and known traps.
