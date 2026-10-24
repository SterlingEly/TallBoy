# TallBoy — Store Listing (DRAFT, 2026-10-24)

> **Status: unreleased draft.** Internal version v3.70. v3.65–v3.70 have not yet been compiled against the real SDK or tested on a device, and the app is not on any store. Feature claims below come from `README.md` and `PROJECT_CONTEXT.md`; re-check them against the build before submitting. Public version number, screenshots and banner are all still to do.

## Name
TallBoy

## Short Description
Huge vector-drawn digits that fill your screen, squish on every minute, and sit on a background color that shows how your day is going.

## Full Description

TallBoy is a watchface that makes the time as big as the screen allows.

The digits are drawn as vectors, not fonts, so they stretch to use all the height there is. Every minute they anticipate, squish and expand into the new time. Choose a full-screen time, a wide layout with info lines above and below, or a stacked layout with the time on one side and a column of info on the other. All three layouts work on round watches too.

Behind the digits, the background color is a pace indicator: it compares your steps so far today with what you typically have done by this time of day, and shifts from red through the spectrum to violet as you fall behind or pull ahead. Color is mood, not a number. The watch is the barometer; the phone is the app.

Pick what you want to see: day, date, weather, UV index, air quality, steps, distance, calories, sleep, heart rate (on supported models), sunrise, sunset, daylight remaining, battery, Bluetooth status, and more. Anything that is missing or out of date quietly disappears instead of showing a wrong number.

TallBoy is an original design by Sterling Ely.

---
FEATURES:
- Oversized vector-drawn digits that fill the available height and animate on every minute change
- Three layouts: full-screen time, wide (info lines above and below), and stacked left/right (time on one side, info column on the other), on every platform including round
- Pace-based background color: today's steps compared with your typical steps at this time of day, shown as a color spectrum
- Three pace palettes (dark, medium, light), or a static background color
- Separately colorable colon, and independent color choices for digits, info text and background
- Weather, UV index and air quality from Open-Meteo (no account or API key needed)
- Steps, distance, active and total calories, sleep, and heart rate on supported models
- Sunrise, sunset, daylight duration and daylight remaining
- Hide, don't mislead: stale or unavailable data disappears rather than displaying zeros
- Round-screen geometry measured against the bezel for Pebble Time Round and Pebble Round 2
- Responds to Quick Look (the notification peek)

PLATFORMS:
Pebble Classic/Steel · Pebble Time/Steel · Pebble Time Round · Pebble 2 · Pebble Time 2 · Pebble 2 Duo · Pebble Round 2

---
Design: Sterling Ely
2026: Implemented by Sterling Ely & Claude (Anthropic)

Weather, UV and air quality data from Open-Meteo. Air quality data is CAMS ENSEMBLE (Copernicus Atmosphere Monitoring Service) via Open-Meteo. (Attribution required before release; see drafting notes.)

Feedback is appreciated: please hit me up at @sterlingely on Twitter/X with any bugs or feature requests!

---

## Release Notes

### 1.0 (first release — number TBD)
- First public release.

---

## Drafting notes (not part of the listing — for Sterling/Claude)

- **Framing (decided 2026-10-24):** TallBoy is more original work than Radium 2 or Bar Graph 2. It is not a rebuild of an older face, so there is no "rebuilt from" timeline. Credits read: design by Sterling, implementation by Sterling & Claude.
- **Monogram relationship:** Monogram is shelved because TallBoy on round covers about 90% of what Monogram was meant to be. The public listing need not mention Monogram.
- **Version label:** `appinfo.json` has `versionLabel` 1.0; internal builds are v3.xx. Collapse the internal history into one first-release note.
- **UUID:** `b5c2d3e4-f6a7-4890-bcde-f12345678901` in `appinfo.json`. Confirm it is a real, unique UUID before submitting.
- **Hardware claims:** heart rate is only on emery and diorite (HR model); say "on supported models", never list platforms in store copy.
- **Attribution TODO:** the CAMS/Open-Meteo acknowledgement must be in the shipped store copy and README (see PROJECT_CONTEXT, Source of Truth).
- **Not yet verified on device:** v3.65–v3.70 (round spacing, stacked/wide on round, Quick Look). The "all three layouts on round" claim depends on that device test.
- **Plugins (preview, announced 2026-10-23):** TallBoy and Radium 2 fetch weather in PebbleKit JS today. Do not move to plugins or publish a plugin-using build until the API stabilizes.
- Modeled on the Radium 2 and Bar Graph 2 STORE_LISTING.md structure.
