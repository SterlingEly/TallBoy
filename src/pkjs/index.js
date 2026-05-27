// ============================================================
// TallBoy — src/pkjs/index.js
// PebbleKit JS: weather fetch, solar fetch, config page
//
// Weather + solar: Open-Meteo API (free, no key required)
// Config: localStorage-persisted, sent as app message on change
//
// MESSAGE KEYS (must match appinfo.json appKeys):
//   WeatherTempF    : int, degrees F
//   WeatherTempC    : int, degrees C
//   WeatherCode     : int, WMO weather code
//   SunriseTime     : int, Unix timestamp of today's sunrise
//   SunsetTime      : int, Unix timestamp of today's sunset
//   CfgTimePos      : int 0-6, time position in info line order
//   CfgSlot1..6     : int, slot type ID (see SLOT_* below)
//   CfgTempUnit     : int, 0=F 1=C
//   CfgDistUnit     : int, 0=mi 1=km
//   CfgClockFormat  : int, 0=12h 1=24h
//
// SLOT TYPE IDs:
//   0 = empty
//   1 = date (day + date)
//   2 = weather
//   3 = solar (sunrise/sunset)
//   4 = steps (+ distance in wide mode)
//   5 = pace (expected + %)
//   6 = heart rate + calories
//   7 = battery
// ============================================================

var STORAGE_KEY = 'tallboy_settings';

// Default config: 3+3 layout, all 6 slots filled, F, mi, 12h
var DEFAULTS = {
  CfgTimePos:     3,
  CfgSlot1:       1,   // date
  CfgSlot2:       2,   // weather
  CfgSlot3:       3,   // solar
  CfgSlot4:       4,   // steps
  CfgSlot5:       5,   // pace
  CfgSlot6:       6,   // hr+cal
  CfgTempUnit:    0,   // F
  CfgDistUnit:    0,   // mi
  CfgClockFormat: 0    // 12h
};

function loadSettings() {
  try {
    var saved = localStorage.getItem(STORAGE_KEY);
    if (saved) {
      var parsed = JSON.parse(saved);
      // Merge with defaults so new keys are always present
      var merged = {};
      for (var k in DEFAULTS) merged[k] = DEFAULTS[k];
      for (var k in parsed) merged[k] = parsed[k];
      return merged;
    }
  } catch(e) {
    console.log('Settings load error: ' + e);
  }
  return DEFAULTS;
}

function saveSettings(settings) {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(settings));
  } catch(e) {
    console.log('Settings save error: ' + e);
  }
}

function sendSettings(settings) {
  Pebble.sendAppMessage(settings, function() {
    console.log('Settings sent: ' + JSON.stringify(settings));
  }, function(e) {
    console.log('Settings send error: ' + e.error.message);
  });
}

// ============================================================
// WEATHER + SOLAR — Open-Meteo API
// Called once on ready, then every 30 minutes.
// Solar timestamps sent as Unix int32 (safe through 2038).
// ============================================================
function fetchWeather() {
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      var lat = pos.coords.latitude;
      var lon = pos.coords.longitude;
      var url = 'https://api.open-meteo.com/v1/forecast'
        + '?latitude='  + lat
        + '&longitude=' + lon
        + '&current=temperature_2m,weather_code'
        + '&daily=sunrise,sunset'
        + '&temperature_unit=celsius'
        + '&timezone=auto'
        + '&forecast_days=2';

      var xhr = new XMLHttpRequest();
      xhr.onload = function() {
        try {
          var data = JSON.parse(this.responseText);

          var tempC = Math.round(data.current.temperature_2m);
          var tempF = Math.round(tempC * 9 / 5 + 32);
          var code  = data.current.weather_code;

          var sunriseTime = 0, sunsetTime = 0;
          try {
            var daily = data.daily;
            if (daily && daily.sunrise && daily.sunset) {
              sunriseTime = Math.round(new Date(daily.sunrise[0]).getTime() / 1000);
              sunsetTime  = Math.round(new Date(daily.sunset[0]).getTime()  / 1000);
            }
          } catch(solarErr) {
            console.log('Solar parse error: ' + solarErr);
          }

          var msg = {
            WeatherTempF: tempF,
            WeatherTempC: tempC,
            WeatherCode:  code
          };
          if (sunriseTime > 0) {
            msg.SunriseTime = sunriseTime;
            msg.SunsetTime  = sunsetTime;
          }

          Pebble.sendAppMessage(msg, function() {
            console.log('Weather+solar sent: ' + tempF + 'F, code ' + code
              + ', rise ' + sunriseTime + ', set ' + sunsetTime);
          }, function(e) {
            console.log('Weather send error: ' + e.error.message);
          });

        } catch(err) {
          console.log('Weather parse error: ' + err);
        }
      };
      xhr.open('GET', url);
      xhr.send();
    },
    function(err) {
      console.log('Geolocation error: ' + err.message);
    },
    { timeout: 15000, maximumAge: 300000 }
  );
}

// ============================================================
// CONFIG PAGE
// Inline HTML config page — no external dependencies.
// Sends settings as app message on save.
//
// Slot type options:
//   0=empty, 1=date, 2=weather, 3=solar, 4=steps, 5=pace, 6=hr+cal, 7=battery
//
// Time position 0-6: time sits after slot N
//   0 = time first, all 6 slots below
//   3 = 3 above + 3 below (default)
//   6 = all 6 above, time last
// ============================================================

var SLOT_NAMES = ['Empty', 'Date', 'Weather', 'Solar', 'Steps', 'Pace', 'HR + Cal', 'Battery'];

function buildConfigPage(settings) {
  function slotSelect(id, val) {
    var opts = '';
    for (var i = 0; i < SLOT_NAMES.length; i++) {
      opts += '<option value="' + i + '"' + (val == i ? ' selected' : '') + '>'
            + SLOT_NAMES[i] + '</option>';
    }
    return '<select id="' + id + '">' + opts + '</select>';
  }

  var posOpts = '';
  for (var p = 0; p <= 6; p++) {
    var label = p === 0 ? 'All below' : p === 6 ? 'All above' : p + ' above / ' + (6-p) + ' below';
    posOpts += '<option value="' + p + '"' + (settings.CfgTimePos == p ? ' selected' : '') + '>'
             + label + '</option>';
  }

  return '<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1">'
    + '<style>'
    + 'body{font-family:sans-serif;background:#111;color:#eee;padding:16px;max-width:360px;margin:0 auto}'
    + 'h2{color:#fff;margin:0 0 4px}p.sub{color:#888;font-size:13px;margin:0 0 20px}'
    + '.section{margin-bottom:24px}'
    + '.section h3{color:#aaa;font-size:12px;text-transform:uppercase;letter-spacing:1px;margin:0 0 10px;border-bottom:1px solid #333;padding-bottom:6px}'
    + 'label{display:block;color:#ccc;font-size:14px;margin-bottom:4px}'
    + 'select,input[type=range]{width:100%;padding:8px;background:#222;border:1px solid #444;color:#eee;border-radius:6px;box-sizing:border-box;font-size:14px;margin-bottom:12px}'
    + '.row{display:flex;gap:10px}'
    + '.row>div{flex:1}'
    + '.toggle{display:flex;gap:0}'
    + '.toggle label{flex:1;text-align:center;padding:8px 0;background:#222;border:1px solid #444;cursor:pointer;font-size:14px;color:#aaa}'
    + '.toggle input{display:none}'
    + '.toggle input:checked + label{background:#4a9;color:#fff;border-color:#4a9}'
    + '.toggle label:first-of-type{border-radius:6px 0 0 6px}'
    + '.toggle label:last-of-type{border-radius:0 6px 6px 0}'
    + '#save{width:100%;padding:14px;background:#4a9;border:none;color:#fff;font-size:16px;font-weight:bold;border-radius:8px;cursor:pointer;margin-top:8px}'
    + '#save:active{background:#3a8}'
    + '.slot-row{display:flex;align-items:center;gap:8px;margin-bottom:8px}'
    + '.slot-num{color:#666;font-size:13px;width:20px;flex-shrink:0}'
    + '.slot-row select{margin:0;flex:1}'
    + '</style></head><body>'
    + '<h2>TallBoy</h2>'
    + '<p class="sub">Watchface settings</p>'

    + '<div class="section">'
    + '<h3>Time Position</h3>'
    + '<label>Where the time digits sit among info lines</label>'
    + '<select id="CfgTimePos">' + posOpts + '</select>'
    + '</div>'

    + '<div class="section">'
    + '<h3>Info Lines (6 slots)</h3>'
    + '<p style="color:#888;font-size:12px;margin:-4px 0 10px">Drag to reorder — or just pick types. Wide mode combines some pairs automatically.</p>'
    + [1,2,3,4,5,6].map(function(n) {
        return '<div class="slot-row"><span class="slot-num">' + n + '</span>'
          + slotSelect('CfgSlot' + n, settings['CfgSlot' + n]) + '</div>';
      }).join('')
    + '</div>'

    + '<div class="section">'
    + '<h3>Units</h3>'
    + '<div class="row">'
    + '<div><label>Temperature</label><div class="toggle">'
    + '<input type="radio" name="CfgTempUnit" id="tu0" value="0"' + (settings.CfgTempUnit==0?' checked':'') + '><label for="tu0">°F</label>'
    + '<input type="radio" name="CfgTempUnit" id="tu1" value="1"' + (settings.CfgTempUnit==1?' checked':'') + '><label for="tu1">°C</label>'
    + '</div></div>'
    + '<div><label>Distance</label><div class="toggle">'
    + '<input type="radio" name="CfgDistUnit" id="du0" value="0"' + (settings.CfgDistUnit==0?' checked':'') + '><label for="du0">mi</label>'
    + '<input type="radio" name="CfgDistUnit" id="du1" value="1"' + (settings.CfgDistUnit==1?' checked':'') + '><label for="du1">km</label>'
    + '</div></div>'
    + '</div>'
    + '</div>'

    + '<div class="section">'
    + '<h3>Clock</h3>'
    + '<div class="toggle">'
    + '<input type="radio" name="CfgClockFormat" id="cf0" value="0"' + (settings.CfgClockFormat==0?' checked':'') + '><label for="cf0">12h</label>'
    + '<input type="radio" name="CfgClockFormat" id="cf1" value="1"' + (settings.CfgClockFormat==1?' checked':'') + '><label for="cf1">24h</label>'
    + '</div>'
    + '</div>'

    + '<button id="save">Save</button>'

    + '<script>'
    + 'document.getElementById("save").onclick = function() {'
    + '  var s = {};'
    + '  ["CfgTimePos","CfgSlot1","CfgSlot2","CfgSlot3","CfgSlot4","CfgSlot5","CfgSlot6"].forEach(function(k){'
    + '    s[k] = parseInt(document.getElementById(k).value);'
    + '  });'
    + '  ["CfgTempUnit","CfgDistUnit","CfgClockFormat"].forEach(function(k){'
    + '    var el = document.querySelector("input[name="+k+"]:checked");'
    + '    s[k] = el ? parseInt(el.value) : 0;'
    + '  });'
    + '  location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(s));'
    + '};'
    + '<\/script>'
    + '</body></html>';
}

// ============================================================
// PEBBLEKIT JS LIFECYCLE
// ============================================================
Pebble.addEventListener('ready', function() {
  console.log('TallBoy JS ready');
  var settings = loadSettings();
  sendSettings(settings);     // push persisted config on reconnect
  fetchWeather();
  setInterval(fetchWeather, 30 * 60 * 1000);
});

Pebble.addEventListener('showConfiguration', function() {
  var settings = loadSettings();
  var html = buildConfigPage(settings);
  // Encode as data URI — no external hosting needed
  var encoded = 'data:text/html,' + encodeURIComponent(html);
  Pebble.openURL(encoded);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e.response || e.response === 'CANCELLED') return;
  try {
    var settings = JSON.parse(decodeURIComponent(e.response));
    saveSettings(settings);
    sendSettings(settings);
  } catch(err) {
    console.log('Config parse error: ' + err);
  }
});
