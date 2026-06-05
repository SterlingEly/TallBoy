// ============================================================
// TallBoy -- src/pkjs/index.js  v3.58
// PebbleKit JS: weather, solar, config page
// ============================================================

var STORAGE_KEY = 'tallboy_settings_v358';

var SLOT_NAMES_STACK = [
  { v:  0, n: 'Empty' },
  { v:  1, n: 'Day' },
  { v:  2, n: 'Date' },
  { v:  3, n: 'Day & Date' },
  { v:  5, n: 'Weather' },
  { v:  6, n: 'Steps' },
  { v:  7, n: 'Distance' },
  { v:  8, n: 'Expected Steps' },
  { v:  9, n: 'Step Pace %' },
  { v: 10, n: 'Active Calories' },
  { v: 11, n: 'Heart Rate' },
  { v: 12, n: 'Sunrise' },
  { v: 13, n: 'Sunset' },
  { v: 14, n: 'Daylight' },
  { v: 15, n: 'Battery' },
  { v: 16, n: 'Bluetooth' },
  { v: 21, n: '[Debug]' }
];

var SLOT_NAMES_WIDE = [
  { v:  0, n: 'Empty' },
  { v:  3, n: 'Day & Date' },
  { v:  5, n: 'Weather' },
  { v:  6, n: 'Steps & Distance' },
  { v: 18, n: 'Steps & Expected Steps' },
  { v:  9, n: 'Steps & Pace %' },
  { v: 19, n: 'Expected Steps & Pace %' },
  { v: 10, n: 'Active Calories & Heart Rate' },
  { v: 17, n: 'Sunrise & Sunset' },
  { v: 14, n: 'Daylight' },
  { v: 20, n: 'Battery & Bluetooth' },
  { v: 21, n: '[Debug]' }
];

// Pebble 64 colors: 6-bit rr_gg_bb, each component 0-3 (0/85/170/255)
// Generate all 64 colors as CSS hex strings
function pblColorToCss(idx) {
  var r = ((idx >> 4) & 3) * 85;
  var g = ((idx >> 2) & 3) * 85;
  var b = ( idx       & 3) * 85;
  return 'rgb(' + r + ',' + g + ',' + b + ')';
}
// Luminance estimate for choosing text color on swatch
function pblLuminance(idx) {
  var r = ((idx >> 4) & 3);
  var g = ((idx >> 2) & 3);
  var b = ( idx       & 3);
  return r * 2 + g * 4 + b;
}

var DEFAULT_SETTINGS = {
  infoMode:    0,
  infoLayout:  1,
  wide:     [3, 5, 0, 6, 17, 15],
  stack:    [1, 3, 6, 9, 11, 5, 15, 16],
  tempUnit:   0,
  distUnit:   0,
  invert:     0,
  colorMode:  0,   // 0=dynamic, 1=static
  colBg:      0,   // black
  colDigH:   63,   // white
  colDigM:   63,   // white
  colShadow:  0,   // black
  colInfo:   63    // white
};

function loadSettings() {
  try {
    var saved = localStorage.getItem(STORAGE_KEY);
    if (saved) {
      var p = JSON.parse(saved);
      var d = DEFAULT_SETTINGS;
      return {
        infoMode:   p.infoMode   !== undefined ? p.infoMode   : d.infoMode,
        infoLayout: p.infoLayout !== undefined ? p.infoLayout : d.infoLayout,
        wide:       p.wide   || d.wide.slice(),
        stack:      p.stack  || d.stack.slice(),
        tempUnit:   p.tempUnit   !== undefined ? p.tempUnit   : d.tempUnit,
        distUnit:   p.distUnit   !== undefined ? p.distUnit   : d.distUnit,
        invert:     p.invert     !== undefined ? p.invert     : d.invert,
        colorMode:  p.colorMode  !== undefined ? p.colorMode  : d.colorMode,
        colBg:      p.colBg      !== undefined ? p.colBg      : d.colBg,
        colDigH:    p.colDigH    !== undefined ? p.colDigH    : d.colDigH,
        colDigM:    p.colDigM    !== undefined ? p.colDigM    : d.colDigM,
        colShadow:  p.colShadow  !== undefined ? p.colShadow  : d.colShadow,
        colInfo:    p.colInfo    !== undefined ? p.colInfo    : d.colInfo
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
    CfgInfoMode:    s.infoMode,
    CfgInfoLayout:  s.infoLayout,
    CfgTempUnit:    s.tempUnit,
    CfgDistUnit:    s.distUnit,
    CfgInvert:      s.invert,
    CfgColorMode:   s.colorMode,
    CfgColorBg:     s.colBg,
    CfgColorDigH:   s.colDigH,
    CfgColorDigM:   s.colDigM,
    CfgColorShadow: s.colShadow,
    CfgColorInfo:   s.colInfo
  };
  for (var i = 0; i < 6; i++)  msg['CfgWide'  + (i + 1)] = s.wide[i];
  for (var i = 0; i < 8; i++)  msg['CfgStack' + (i + 1)] = s.stack[i];
  Pebble.sendAppMessage(msg,
    function() { console.log('Settings sent OK'); },
    function(e) { console.log('Settings error: ' + e.error.message); });
}

// ============================================================
// WEATHER + SOLAR
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
function slotSelectFromList(id, currentVal, list) {
  var out = '<select id="' + id + '">';
  for (var i = 0; i < list.length; i++) {
    var sel = (list[i].v === currentVal) ? ' selected' : '';
    out += '<option value="' + list[i].v + '"' + sel + '>' + list[i].n + '<\/option>';
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

// Render a 64-color swatch grid for picking a Pebble color
// fieldId: hidden input id; currentIdx: current 6-bit color index
function colorPicker(fieldId, currentIdx) {
  // Layout: 8 columns (vary blue 0-3 x red 0-1), 8 rows
  // Order colors roughly by hue/brightness for usability
  // Simple approach: row=luminance band, col=hue sweep
  var out = '<input type="hidden" id="' + fieldId + '" value="' + currentIdx + '">';
  out += '<div class="swatches" id="' + fieldId + '_sw">';
  // Iterate all 64 colors in a useful visual order:
  // Sort by value for now — good enough, shows the full palette
  for (var idx = 0; idx < 64; idx++) {
    var css = pblColorToCss(idx);
    var lum = pblLuminance(idx);
    var textCol = lum >= 5 ? '#000' : '#fff';
    var sel = (idx === currentIdx) ? ' sw-sel' : '';
    out += '<div class="sw' + sel + '" style="background:' + css + ';color:' + textCol + '" '
         + 'onclick="pickColor(\'' + fieldId + '\',' + idx + ')">'
         + '<\/div>';
  }
  out += '<\/div>';
  return out;
}

function buildConfigPage(s) {
  var wideRows = '';
  var wideLabels = ['Above 1', 'Above 2', 'Above 3', 'Below 1', 'Below 2', 'Below 3'];
  for (var i = 0; i < 6; i++) {
    wideRows += '<div class="row"><span class="lbl">' + wideLabels[i] + '<\/span>'
      + slotSelectFromList('w' + i, s.wide[i], SLOT_NAMES_WIDE) + '<\/div>';
  }
  var stackRows = '';
  for (var i = 0; i < 8; i++) {
    stackRows += '<div class="row"><span class="lbl">' + (i+1) + '<\/span>'
      + slotSelectFromList('s' + i, s.stack[i], SLOT_NAMES_STACK) + '<\/div>';
  }

  var colorLabels = [
    ['colBg',     'Background',    s.colBg],
    ['colDigH',   'Hour digits',   s.colDigH],
    ['colDigM',   'Minute digits', s.colDigM],
    ['colShadow', 'Shadow',        s.colShadow],
    ['colInfo',   'Info & icons',  s.colInfo]
  ];
  var colorRows = '';
  for (var i = 0; i < colorLabels.length; i++) {
    var fid = colorLabels[i][0], label = colorLabels[i][1], cur = colorLabels[i][2];
    colorRows += '<div class="color-row"><span class="lbl">' + label + '<\/span>'
      + colorPicker(fid, cur) + '<\/div>';
  }

  var html = '<!DOCTYPE html><html><head>'
    + '<meta name="viewport" content="width=device-width,initial-scale=1">'
    + '<style>'
    + 'body{font:14px sans-serif;background:#111;color:#eee;padding:14px;max-width:400px;margin:0 auto}'
    + 'h2{color:#fff;margin:0 0 16px;font-size:20px}'
    + '.section{margin-bottom:22px}'
    + '.section h3{color:#aaa;font-size:11px;text-transform:uppercase;letter-spacing:1px;margin:0 0 8px;border-bottom:1px solid #333;padding-bottom:4px}'
    + '.row{display:flex;align-items:center;gap:8px;padding:5px 6px;background:#1a1a1a;border:1px solid #2a2a2a;border-radius:6px;margin-bottom:4px}'
    + '.lbl{color:#888;font-size:12px;min-width:80px;flex-shrink:0}'
    + 'select{flex:1;padding:6px 8px;background:#222;border:1px solid #444;color:#eee;border-radius:4px;font-size:12px}'
    + '.toggle{display:flex;margin-bottom:0}'
    + '.toggle input{display:none}'
    + '.toggle label{flex:1;text-align:center;padding:8px 4px;background:#222;border:1px solid #444;cursor:pointer;color:#aaa;font-size:12px;line-height:1.2}'
    + '.toggle input:checked+label{background:#4a9;color:#fff;border-color:#4a9}'
    + '.toggle label:first-of-type{border-radius:6px 0 0 6px}'
    + '.toggle label:last-of-type{border-radius:0 6px 6px 0}'
    + '.pair{display:flex;gap:10px}.pair>div{flex:1}'
    + '.color-row{display:flex;align-items:flex-start;gap:8px;padding:6px 6px;background:#1a1a1a;border:1px solid #2a2a2a;border-radius:6px;margin-bottom:4px}'
    + '.swatches{display:flex;flex-wrap:wrap;gap:2px;width:200px}'
    + '.sw{width:22px;height:22px;cursor:pointer;border:2px solid transparent;border-radius:3px;flex-shrink:0}'
    + '.sw-sel{border-color:#fff!important;transform:scale(1.15)}'
    + '#static-colors{display:none}'
    + '#save{width:100%;padding:14px;background:#4a9;border:none;color:#fff;font-size:16px;font-weight:bold;border-radius:8px;cursor:pointer;margin-top:4px}'
    + '#save:active{background:#3a8}'
    + '.note{color:#555;font-size:11px;margin:6px 0 0}'
    + '<\/style><\/head><body>'
    + '<h2>TallBoy<\/h2>'

    + '<div class="section"><h3>Info Display Mode<\/h3>'
    + radioGroup('im', ['Always On','Always Off','Shake','Shake - 1 min','Debug'], [2,1,3,4,0], s.infoMode)
    + '<\/div>'

    + '<div class="section"><h3>Info Layout<\/h3>'
    + radioGroup('il', ['Wide','Stacked Left','Stacked Right'], [0,1,2], s.infoLayout)
    + '<\/div>'

    + '<div class="section"><h3>Wide Mode Lines<\/h3>'
    + '<p class="note" style="margin:0 0 8px">3 lines above the time, 3 below.<\/p>'
    + wideRows + '<\/div>'

    + '<div class="section"><h3>Stacked Mode Lines<\/h3>'
    + '<p class="note" style="margin:0 0 8px">Shared by Stacked Left and Stacked Right.<\/p>'
    + stackRows + '<\/div>'

    + '<div class="section"><h3>Units<\/h3>'
    + '<div class="pair">'
    + '<div><label style="color:#aaa;font-size:12px;display:block;margin-bottom:4px">Temperature<\/label>'
    + radioGroup('tu', ['F','C'], [0,1], s.tempUnit) + '<\/div>'
    + '<div><label style="color:#aaa;font-size:12px;display:block;margin-bottom:4px">Distance<\/label>'
    + radioGroup('du', ['mi','km'], [0,1], s.distUnit) + '<\/div><\/div>'
    + '<p class="note">Clock format uses your Pebble system setting.<\/p>'
    + '<\/div>'

    + '<div class="section"><h3>Colors<\/h3>'
    + '<label style="color:#aaa;font-size:12px;display:block;margin-bottom:6px">Color mode (color Pebbles)<\/label>'
    + radioGroup('cm', ['Step Pace Gradient','Static'], [0,1], s.colorMode)
    + '<div id="static-colors" style="margin-top:12px">'
    + colorRows
    + '<\/div>'
    + '<\/div>'

    + '<div class="section"><h3>Display<\/h3>'
    + '<label style="color:#aaa;font-size:12px;display:block;margin-bottom:4px">Theme (black & white Pebbles only)<\/label>'
    + radioGroup('inv', ['Black on white','White on black'], [1,0], s.invert)
    + '<\/div>'

    + '<button id="save" onclick="doSave()">Save<\/button>'

    + '<script>'
    // Show/hide static color section based on color mode radio
    + '(function(){'
    + '  function updateColorVis(){'
    + '    var cm=document.querySelector("input[name=cm]:checked");'
    + '    document.getElementById("static-colors").style.display=(cm&&parseInt(cm.value)===1)?"block":"none";'
    + '  }'
    + '  document.querySelectorAll("input[name=cm]").forEach(function(r){r.addEventListener("change",updateColorVis);});'
    + '  updateColorVis();'
    + '})();'
    // Color swatch picker
    + 'function pickColor(fieldId, idx) {'
    + '  document.getElementById(fieldId).value = idx;'
    + '  var sw = document.getElementById(fieldId+"_sw");'
    + '  var swatches = sw.querySelectorAll(".sw");'
    + '  swatches.forEach(function(s,i){s.classList.toggle("sw-sel",i===idx);});'
    + '}'
    + 'function doSave() {'
    + '  var im  = document.querySelector("input[name=im]:checked");'
    + '  var il  = document.querySelector("input[name=il]:checked");'
    + '  var tu  = document.querySelector("input[name=tu]:checked");'
    + '  var du  = document.querySelector("input[name=du]:checked");'
    + '  var inv = document.querySelector("input[name=inv]:checked");'
    + '  var cm  = document.querySelector("input[name=cm]:checked");'
    + '  var wide=[],stack=[];'
    + '  for(var i=0;i<6;i++) wide.push(parseInt(document.getElementById("w"+i).value));'
    + '  for(var i=0;i<8;i++) stack.push(parseInt(document.getElementById("s"+i).value));'
    + '  function ci(id){return parseInt(document.getElementById(id).value);}'
    + '  var s={'
    + '    infoMode:  im  ? parseInt(im.value)  : 0,'
    + '    infoLayout:il  ? parseInt(il.value)  : 1,'
    + '    wide:wide, stack:stack,'
    + '    tempUnit:  tu  ? parseInt(tu.value)  : 0,'
    + '    distUnit:  du  ? parseInt(du.value)  : 0,'
    + '    invert:    inv ? parseInt(inv.value) : 0,'
    + '    colorMode: cm  ? parseInt(cm.value)  : 0,'
    + '    colBg:     ci("colBg"),'
    + '    colDigH:   ci("colDigH"),'
    + '    colDigM:   ci("colDigM"),'
    + '    colShadow: ci("colShadow"),'
    + '    colInfo:   ci("colInfo")'
    + '  };'
    + '  location.href="pebblejs://close#"+encodeURIComponent(JSON.stringify(s));'
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
