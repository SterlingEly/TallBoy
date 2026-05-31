// ============================================================
// TallBoy — src/pkjs/index.js  v3.53
// PebbleKit JS: weather, solar, config page
//
// SLOT TYPE IDs (must match main.c SlotType enum):
//   0=empty, 1=day, 2=date, 3=day+date, 4=temp, 5=weather
//   6=steps, 7=distance, 8=exp_steps, 9=pace
//   10=calories, 11=heart_rate, 12=sunrise, 13=sunset
//   14=daylight, 15=battery, 16=bluetooth, 17=sunrise+sunset
//
// Wide rendering behavior:
//   SLOT_DAY_DATE → "Monday, Aug 21" (full name)
//   SLOT_WEATHER  → icon + temp + conditions
//   SLOT_STEPS    → "3,450 · 1.7mi"
//   SLOT_CALORIES → "212 cal · 72 bpm"
//   SLOT_PACE     → "exp 3,200 · 108%"
//   SLOT_SUN_TIMES → "6:12am · 8:45pm"
//
// Stacked rendering (compact, icon = label):
//   SLOT_DAY_DATE → "Mon, Aug 21"
//   SLOT_DAY      → "Monday"
//   SLOT_WEATHER  → icon + temp only
//   SLOT_STEPS    → icon + "3,450"
//   SLOT_CALORIES → icon + "212"
//   SLOT_HEART    → icon + "72"
//   SLOT_PACE     → icon + "108%"
// ============================================================

var STORAGE_KEY = 'tallboy_settings_v353';

// Wide: only offer slots that make sense at full width / combined display
var SLOT_NAMES_WIDE = {
  0:  'Empty',
  3:  'Day & Date  \u2192  Monday, Aug 21',
  5:  'Weather  \u2192  72\u00b0F, partly cloudy',
  6:  'Steps  \u2192  3,450 \u00b7 1.7mi',
  9:  'Pace  \u2192  exp 3,200 \u00b7 108%',
  10: 'Calories  \u2192  212 cal \u00b7 72 bpm',
  11: 'Heart rate  \u2192  72 bpm',
  12: 'Sunrise',
  13: 'Sunset',
  14: 'Daylight hours',
  15: 'Battery',
  16: 'Bluetooth',
  17: 'Sunrise & Sunset  \u2192  6:12am \u00b7 8:45pm'
};

// Stacked: full slot list with clear labels for compact rendering
var SLOT_NAMES_STACK = {
  0:  'Empty',
  1:  'Day  \u2192  Monday',
  2:  'Date  \u2192  Aug 21',
  3:  'Day & Date  \u2192  Mon, Aug 21',
  5:  'Weather  \u2192  72\u00b0F',
  6:  'Steps',
  7:  'Distance',
  8:  'Expected steps',
  9:  'Pace %',
  10: 'Calories',
  11: 'Heart rate',
  12: 'Sunrise',
  13: 'Sunset',
  14: 'Daylight',
  15: 'Battery',
  16: 'Bluetooth'
};

var DEFAULT_SETTINGS = {
  infoMode:   0,   // debug
  infoLayout: 1,   // stack_l
  wide:    [3, 5, 0, 6, 17, 15],
  stack:   [1, 3, 6, 9, 11, 5, 15, 16],
  tempUnit:  0,   // F
  distUnit:  0    // mi
};

function loadSettings() {
  try {
    var saved = localStorage.getItem(STORAGE_KEY);
    if (saved) {
      var p = JSON.parse(saved);
      return {
        infoMode:   p.infoMode   !== undefined ? p.infoMode   : DEFAULT_SETTINGS.infoMode,
        infoLayout: p.infoLayout !== undefined ? p.infoLayout : DEFAULT_SETTINGS.infoLayout,
        wide:       p.wide       || DEFAULT_SETTINGS.wide.slice(),
        stack:      p.stack      || DEFAULT_SETTINGS.stack.slice(),
        tempUnit:   p.tempUnit   !== undefined ? p.tempUnit   : DEFAULT_SETTINGS.tempUnit,
        distUnit:   p.distUnit   !== undefined ? p.distUnit   : DEFAULT_SETTINGS.distUnit
      };
    }
  } catch(e) { console.log('Load error: ' + e); }
  return JSON.parse(JSON.stringify(DEFAULT_SETTINGS));
}

function saveSettings(s) {
  try { localStorage.setItem(STORAGE_KEY, JSON.stringify(s)); } catch(e) {}
}

function sendSettings(s) {
  var msg = {
    CfgInfoMode:   s.infoMode,
    CfgInfoLayout: s.infoLayout,
    CfgTempUnit:   s.tempUnit,
    CfgDistUnit:   s.distUnit
  };
  for (var i = 0; i < 6; i++)  msg['CfgWide'  + (i + 1)] = s.wide[i];
  for (var i = 0; i < 8; i++)  msg['CfgStack' + (i + 1)] = s.stack[i];
  Pebble.sendAppMessage(msg,
    function() { console.log('Settings sent OK'); },
    function(e) { console.log('Settings error: ' + e.error.message); });
}

// ============================================================
// WEATHER + SOLAR (open-meteo, no API key required)
// ============================================================
function fetchWeather() {
  navigator.geolocation.getCurrentPosition(function(pos) {
    var lat = pos.coords.latitude, lon = pos.coords.longitude;
    var url = 'https://api.open-meteo.com/v1/forecast'
      + '?latitude=' + lat + '&longitude=' + lon
      + '&current=temperature_2m,weather_code'
      + '&daily=sunrise,sunset&temperature_unit=celsius'
      + '&timezone=auto&forecast_days=2';
    var xhr = new XMLHttpRequest();
    xhr.onload = function() {
      try {
        var d = JSON.parse(this.responseText);
        var tempC = Math.round(d.current.temperature_2m);
        var tempF = Math.round(tempC * 9 / 5 + 32);
        var code  = d.current.weather_code;
        var rise = 0, set = 0;
        if (d.daily && d.daily.sunrise) {
          rise = Math.round(new Date(d.daily.sunrise[0]).getTime() / 1000);
          set  = Math.round(new Date(d.daily.sunset[0]).getTime()  / 1000);
        }
        var msg = { WeatherTempF: tempF, WeatherTempC: tempC, WeatherCode: code };
        if (rise > 0) { msg.SunriseTime = rise; msg.SunsetTime = set; }
        Pebble.sendAppMessage(msg,
          function() { console.log('Weather sent: ' + tempF + 'F code ' + code); },
          function(e) { console.log('Weather error: ' + e.error.message); });
      } catch(e) { console.log('Parse error: ' + e); }
    };
    xhr.open('GET', url);
    xhr.send();
  }, function(e) { console.log('Geo error: ' + e.message); },
  { timeout: 15000, maximumAge: 300000 });
}

// ============================================================
// CONFIG PAGE
// ============================================================
function slotSelect(id, currentVal, names) {
  var out = '<select id="' + id + '">';
  for (var v in names) {
    var sel = (parseInt(v) === currentVal) ? ' selected' : '';
    out += '<option value="' + v + '"' + sel + '>' + names[v] + '<\/option>';
  }
  out += '<\/select>';
  return out;
}

function radioGroup(name, labels, values, currentVal) {
  var out = '<div class="toggle">';
  for (var i = 0; i < labels.length; i++) {
    var chk = (values[i] === currentVal) ? ' checked' : '';
    out += '<input type="radio" name="' + name + '" id="' + name + i + '" value="' + values[i] + '"' + chk + '>';
    out += '<label for="' + name + i + '">' + labels[i] + '<\/label>';
  }
  out += '<\/div>';
  return out;
}

function buildConfigPage(s) {
  var wideRows = '';
  var wideLabels = ['Above 1', 'Above 2', 'Above 3', 'Below 1', 'Below 2', 'Below 3'];
  for (var i = 0; i < 6; i++) {
    wideRows += '<div class="row">'
      + '<span class="lbl">' + wideLabels[i] + '<\/span>'
      + slotSelect('w' + i, s.wide[i], SLOT_NAMES_WIDE)
      + '<\/div>';
    if (i === 2) {
      wideRows += '<div class="divider">\u23F0 Time<\/div>';
    }
  }

  var stackRows = '';
  for (var i = 0; i < 8; i++) {
    stackRows += '<div class="row">'
      + '<span class="lbl">' + (i + 1) + '<\/span>'
      + slotSelect('s' + i, s.stack[i], SLOT_NAMES_STACK)
      + '<\/div>';
  }

  var html = '<!DOCTYPE html><html><head>'
    + '<meta name="viewport" content="width=device-width,initial-scale=1">'
    + '<style>'
    + 'body{font:14px sans-serif;background:#111;color:#eee;padding:14px;max-width:380px;margin:0 auto}'
    + 'h2{color:#fff;margin:0 0 16px;font-size:20px}'
    + '.section{margin-bottom:22px}'
    + '.section h3{color:#aaa;font-size:11px;text-transform:uppercase;letter-spacing:1px;margin:0 0 8px;border-bottom:1px solid #333;padding-bottom:4px}'
    + '.row{display:flex;align-items:center;gap:8px;padding:5px 6px;background:#1a1a1a;border:1px solid #2a2a2a;border-radius:6px;margin-bottom:4px}'
    + '.lbl{color:#888;font-size:12px;min-width:52px;flex-shrink:0}'
    + 'select{flex:1;padding:6px 8px;background:#222;border:1px solid #444;color:#eee;border-radius:4px;font-size:12px}'
    + '.divider{text-align:center;color:#555;font-size:12px;padding:6px 0;border-top:1px dashed #333;border-bottom:1px dashed #333;margin:4px 0}'
    + '.toggle{display:flex;margin-bottom:0}'
    + '.toggle input{display:none}'
    + '.toggle label{flex:1;text-align:center;padding:8px 4px;background:#222;border:1px solid #444;cursor:pointer;color:#aaa;font-size:12px;line-height:1.2}'
    + '.toggle input:checked+label{background:#4a9;color:#fff;border-color:#4a9}'
    + '.toggle label:first-of-type{border-radius:6px 0 0 6px}'
    + '.toggle label:last-of-type{border-radius:0 6px 6px 0}'
    + '.pair{display:flex;gap:10px}.pair>div{flex:1}'
    + '#save{width:100%;padding:14px;background:#4a9;border:none;color:#fff;font-size:16px;font-weight:bold;border-radius:8px;cursor:pointer;margin-top:4px}'
    + '#save:active{background:#3a8}'
    + '<\/style><\/head><body>'
    + '<h2>TallBoy<\/h2>'

    + '<div class="section"><h3>Info Display Mode<\/h3>'
    + radioGroup('im',
        ['Always On', 'Always Off', 'Shake to Toggle', 'Shake \u2014 1 min', 'Debug'],
        [2, 1, 3, 4, 0],
        s.infoMode)
    + '<\/div>'

    + '<div class="section"><h3>Info Layout<\/h3>'
    + radioGroup('il',
        ['Wide', 'Stacked Left', 'Stacked Right'],
        [0, 1, 2],
        s.infoLayout)
    + '<\/div>'

    + '<div class="section"><h3>Wide Mode Lines<\/h3>'
    + '<p style="color:#666;font-size:12px;margin:0 0 8px">First 3 above the time, last 3 below.<\/p>'
    + wideRows
    + '<\/div>'

    + '<div class="section"><h3>Stacked Mode Lines<\/h3>'
    + '<p style="color:#666;font-size:12px;margin:0 0 8px">Shared by Stacked Left and Stacked Right.<\/p>'
    + stackRows
    + '<\/div>'

    + '<div class="section"><h3>Units<\/h3>'
    + '<div class="pair">'
    + '<div><label style="color:#aaa;font-size:12px;display:block;margin-bottom:4px">Temperature<\/label>'
    + radioGroup('tu', ['\u00b0F', '\u00b0C'], [0, 1], s.tempUnit)
    + '<\/div>'
    + '<div><label style="color:#aaa;font-size:12px;display:block;margin-bottom:4px">Distance<\/label>'
    + radioGroup('du', ['mi', 'km'], [0, 1], s.distUnit)
    + '<\/div><\/div>'
    + '<p style="color:#555;font-size:11px;margin:8px 0 0">Clock format uses your Pebble system setting.<\/p>'
    + '<\/div>'

    + '<button id="save" onclick="doSave()">Save<\/button>'

    + '<script>'
    + 'function doSave() {'
    + '  var im = document.querySelector("input[name=im]:checked");'
    + '  var il = document.querySelector("input[name=il]:checked");'
    + '  var tu = document.querySelector("input[name=tu]:checked");'
    + '  var du = document.querySelector("input[name=du]:checked");'
    + '  var wide = [], stack = [];'
    + '  for (var i = 0; i < 6; i++) wide.push(parseInt(document.getElementById("w"+i).value));'
    + '  for (var i = 0; i < 8; i++) stack.push(parseInt(document.getElementById("s"+i).value));'
    + '  var s = {'
    + '    infoMode:   im ? parseInt(im.value) : 0,'
    + '    infoLayout: il ? parseInt(il.value) : 1,'
    + '    wide:  wide,'
    + '    stack: stack,'
    + '    tempUnit: tu ? parseInt(tu.value) : 0,'
    + '    distUnit: du ? parseInt(du.value) : 0'
    + '  };'
    + '  location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(s));'
    + '}'
    + '<\/script><\/body><\/html>';

  return html;
}

// ============================================================
// LIFECYCLE
// ============================================================
Pebble.addEventListener('ready', function() {
  console.log('TallBoy JS ready');
  var s = loadSettings();
  sendSettings(s);
  fetchWeather();
  setInterval(fetchWeather, 30 * 60 * 1000);
});

Pebble.addEventListener('showConfiguration', function() {
  var s = loadSettings();
  Pebble.openURL('data:text/html,' + encodeURIComponent(buildConfigPage(s)));
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e.response || e.response === 'CANCELLED') return;
  try {
    var s = JSON.parse(decodeURIComponent(e.response));
    saveSettings(s);
    sendSettings(s);
  } catch(err) { console.log('Config parse error: ' + err); }
});
