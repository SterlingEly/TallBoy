// ============================================================
// TallBoy -- src/pkjs/index.js  v3.59
// PebbleKit JS: weather, solar, config page
// ============================================================

var STORAGE_KEY = 'tallboy_settings_v359';

var SLOT_NAMES_STACK = [
  { v:  0, n: 'Empty' },
  { v:  1, n: 'Day' },
  { v:  2, n: 'Date' },
  { v:  3, n: 'Day & Date' },
  { v:  5, n: 'Weather' },
  { v:  6, n: 'Steps' },
  { v:  7, n: 'Distance' },
  { v:  8, n: 'Typical Steps' },
  { v:  9, n: 'Step Pace %' },
  { v: 10, n: 'Active Calories' },
  { v: 11, n: 'Heart Rate' },
  { v: 12, n: 'Sunrise' },
  { v: 13, n: 'Sunset' },
  { v: 14, n: 'Daylight' },
  { v: 15, n: 'Battery' },
  { v: 16, n: 'Bluetooth' },
  { v: 21, n: '[ Debug ]' },
  { v: 22, n: 'UV Index' },
  { v: 23, n: 'Light Remaining' },
  { v: 26, n: 'Daily Step Goal' }
];

var SLOT_NAMES_WIDE = [
  { v:  0, n: 'Empty' },
  { v:  3, n: 'Day & Date' },
  { v:  5, n: 'Weather' },
  { v:  6, n: 'Steps & Distance' },
  { v: 18, n: 'Steps & Typical Steps' },
  { v:  9, n: 'Steps & Pace %' },
  { v: 19, n: 'Typical Steps & Pace %' },
  { v: 10, n: 'Active Calories & Heart Rate' },
  { v: 17, n: 'Sunrise & Sunset' },
  { v: 14, n: 'Daylight' },
  { v: 20, n: 'Battery & Bluetooth' },
  { v: 21, n: '[ Debug ]' },
  { v: 22, n: 'UV Index' },
  { v: 23, n: 'Light Remaining' },
  { v: 24, n: 'UV & Light Remaining' },
  { v: 25, n: 'Temp & UV Index' },
  { v: 26, n: 'Daily Step Goal' }
];

function pblColorToCss(idx) {
  var r = ((idx >> 4) & 3) * 85;
  var g = ((idx >> 2) & 3) * 85;
  var b = ( idx       & 3) * 85;
  return 'rgb(' + r + ',' + g + ',' + b + ')';
}
function pblLuminance(idx) {
  return ((idx >> 4) & 3) * 2 + ((idx >> 2) & 3) * 4 + (idx & 3);
}

var DEFAULT_SETTINGS = {
  infoMode:   0, infoLayout: 1,
  wide:    [3, 5, 0, 6, 17, 15],
  stack:   [1, 3, 6, 9, 11, 5, 15, 16],
  tempUnit: 0, distUnit: 0, invert: 0,
  colorMode: 0, colBg: 0, colDigH: 63, colDigM: 63, colShadow: 0, colInfo: 63
};

function loadSettings() {
  try {
    var saved = localStorage.getItem(STORAGE_KEY);
    if (saved) {
      var p = JSON.parse(saved), d = DEFAULT_SETTINGS;
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
  } catch(e) {}
  return JSON.parse(JSON.stringify(DEFAULT_SETTINGS));
}

function saveSettings(s) {
  try { localStorage.setItem(STORAGE_KEY, JSON.stringify(s)); } catch(e) {}
}

function sendSettings(s) {
  var msg = {
    CfgInfoMode: s.infoMode, CfgInfoLayout: s.infoLayout,
    CfgTempUnit: s.tempUnit, CfgDistUnit: s.distUnit,
    CfgInvert: s.invert, CfgColorMode: s.colorMode,
    CfgColorBg: s.colBg, CfgColorDigH: s.colDigH,
    CfgColorDigM: s.colDigM, CfgColorShadow: s.colShadow,
    CfgColorInfo: s.colInfo
  };
  for (var i = 0; i < 6; i++) msg['CfgWide'  + (i+1)] = s.wide[i];
  for (var i = 0; i < 8; i++) msg['CfgStack' + (i+1)] = s.stack[i];
  Pebble.sendAppMessage(msg,
    function() { console.log('Settings sent OK'); },
    function(e) { console.log('Settings error: ' + e.error.message); });
}

// ============================================================
// WEATHER + SOLAR
// Fetches from Open-Meteo on connect and every 30 minutes.
// Sends: WeatherTempF, WeatherTempC, WeatherCode, UvIndex,
//        SunriseTime, SunsetTime, SunriseTomorrow
// ============================================================
function fetchWeather() {
  navigator.geolocation.getCurrentPosition(function(pos) {
    var lat = pos.coords.latitude, lon = pos.coords.longitude;
    var url = 'https://api.open-meteo.com/v1/forecast'
      + '?latitude=' + lat + '&longitude=' + lon
      + '&current=temperature_2m,weather_code,uv_index'
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
        var uv = d.current.uv_index !== undefined ? Math.round(d.current.uv_index) : 0;
        var riseTom = 0;
        if (d.daily && d.daily.sunrise && d.daily.sunrise[1])
          riseTom = Math.round(new Date(d.daily.sunrise[1]).getTime() / 1000);
        var msg = { WeatherTempF: tempF, WeatherTempC: tempC, WeatherCode: code, UvIndex: uv };
        if (rise > 0) { msg.SunriseTime = rise; msg.SunsetTime = set; }
        if (riseTom > 0) msg.SunriseTomorrow = riseTom;
        Pebble.sendAppMessage(msg,
          function() { console.log('Weather sent: ' + tempF + 'F'); },
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
    out += '<option value="' + list[i].v + '"' + (list[i].v === currentVal ? ' selected' : '') + '>' + list[i].n + '<\/option>';
  }
  return out + '<\/select>';
}

function radioGroup(name, labels, values, currentVal) {
  var out = '<div class="toggle">';
  for (var i = 0; i < labels.length; i++) {
    out += '<input type="radio" name="' + name + '" id="' + name + i + '" value="' + values[i] + '"' + (values[i] === currentVal ? ' checked' : '') + '>';
    out += '<label for="' + name + i + '">' + labels[i] + '<\/label>';
  }
  return out + '<\/div>';
}

function colorPicker(fieldId, currentIdx) {
  var bg = pblColorToCss(currentIdx);
  var lum = pblLuminance(currentIdx);
  var fg = lum >= 5 ? '#000' : '#fff';
  var grid = '<div class="cp-grid" id="' + fieldId + '_grid" style="display:none">';
  for (var idx = 0; idx < 64; idx++) {
    var css = pblColorToCss(idx);
    var sel = (idx === currentIdx) ? ' cp-sel' : '';
    grid += '<div class="cp-sw' + sel + '" style="background:' + css + '"'
          + ' onclick="cpPick(\'' + fieldId + '\',' + idx + ')">'
          + '<\/div>';
  }
  grid += '<\/div>';
  return '<input type="hidden" id="' + fieldId + '" value="' + currentIdx + '">'
    + '<div class="cp-trigger" id="' + fieldId + '_tr"'
    + ' style="background:' + bg + ';color:' + fg + '"'
    + ' onclick="cpToggle(\'' + fieldId + '\')">'
    + '<span id="' + fieldId + '_lbl">' + currentIdx + '<\/span>'
    + '<\/div>'
    + grid;
}

function buildConfigPage(s, isColor) {
  var wideRows = '', stackRows = '';
  var wideLabels = ['Above 1','Above 2','Above 3','Below 1','Below 2','Below 3'];
  for (var i = 0; i < 6; i++)
    wideRows += '<div class="row"><span class="lbl">' + wideLabels[i] + '<\/span>' + slotSelectFromList('w'+i, s.wide[i], SLOT_NAMES_WIDE) + '<\/div>';
  for (var i = 0; i < 8; i++)
    stackRows += '<div class="row"><span class="lbl">' + (i+1) + '<\/span>' + slotSelectFromList('s'+i, s.stack[i], SLOT_NAMES_STACK) + '<\/div>';

  var colorDefs = [
    ['colBg','Background',s.colBg],['colDigH','Hour digits',s.colDigH],
    ['colDigM','Minute digits',s.colDigM],['colShadow','Shadow',s.colShadow],
    ['colInfo','Info & icons',s.colInfo]
  ];
  var colorRows = '';
  for (var i = 0; i < colorDefs.length; i++) {
    colorRows += '<div class="color-row">'
      + '<span class="lbl">' + colorDefs[i][1] + '<\/span>'
      + colorPicker(colorDefs[i][0], colorDefs[i][2])
      + '<\/div>';
  }

  var colorSection = isColor
    ? '<div class="section"><h3>Colors<\/h3>'
      + '<label style="color:#aaa;font-size:12px;display:block;margin-bottom:6px">Color mode<\/label>'
      + radioGroup('cm',['Step Pace Progression','Static'],[0,1],s.colorMode)
      + '<div id="static-colors" style="display:none;margin-top:12px">'
      + colorRows
      + '<\/div><\/div>'
    : '';

  var invertSection = !isColor
    ? '<div class="section"><h3>Display<\/h3>'
      + '<label style="color:#aaa;font-size:12px;display:block;margin-bottom:4px">Theme<\/label>'
      + radioGroup('inv',['Black on white','White on black'],[1,0],s.invert)
      + '<\/div>'
    : '';

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
    + '.toggle{display:flex}'
    + '.toggle input{display:none}'
    + '.toggle label{flex:1;text-align:center;padding:8px 4px;background:#222;border:1px solid #444;cursor:pointer;color:#aaa;font-size:12px;line-height:1.2}'
    + '.toggle input:checked+label{background:#4a9;color:#fff;border-color:#4a9}'
    + '.toggle label:first-of-type{border-radius:6px 0 0 6px}'
    + '.toggle label:last-of-type{border-radius:0 6px 6px 0}'
    + '.pair{display:flex;gap:10px}.pair>div{flex:1}'
    + '.color-row{display:flex;align-items:flex-start;gap:8px;padding:6px 6px;background:#1a1a1a;border:1px solid #2a2a2a;border-radius:6px;margin-bottom:4px}'
    + '.cp-trigger{flex:1;height:32px;border-radius:4px;cursor:pointer;display:flex;align-items:center;padding:0 10px;font-size:12px;font-weight:bold;border:2px solid #555;user-select:none}'
    + '.cp-grid{display:flex;flex-wrap:wrap;gap:3px;padding:8px;background:#222;border:1px solid #444;border-radius:6px;margin-top:6px;flex:1}'
    + '.cp-sw{width:20px;height:20px;border-radius:3px;cursor:pointer;border:2px solid transparent;flex-shrink:0;transition:transform .1s}'
    + '.cp-sw:hover{transform:scale(1.2);z-index:1}'
    + '.cp-sel{border-color:#fff!important;transform:scale(1.15)}'
    + '#save{width:100%;padding:14px;background:#4a9;border:none;color:#fff;font-size:16px;font-weight:bold;border-radius:8px;cursor:pointer;margin-top:4px}'
    + '#save:active{background:#3a8}'
    + '.note{color:#555;font-size:11px;margin:6px 0 0}'
    + '<\/style><\/head><body>'
    + '<h2>TallBoy<\/h2>'
    + '<div class="section"><h3>Info Display Mode<\/h3>'
    + radioGroup('im',['Always On','Always Off','Shake','Shake - 1 min','Debug'],[2,1,3,4,0],s.infoMode)
    + '<\/div>'
    + '<div class="section"><h3>Info Layout<\/h3>'
    + radioGroup('il',['Wide','Stacked Left','Stacked Right'],[0,1,2],s.infoLayout)
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
    + radioGroup('tu',['F','C'],[0,1],s.tempUnit) + '<\/div>'
    + '<div><label style="color:#aaa;font-size:12px;display:block;margin-bottom:4px">Distance<\/label>'
    + radioGroup('du',['mi','km'],[0,1],s.distUnit) + '<\/div><\/div>'
    + '<p class="note">Clock format uses your Pebble system setting.<\/p>'
    + '<\/div>'
    + colorSection
    + invertSection
    + '<button id="save" onclick="doSave()">Save<\/button>'
    + '<script>';

  if (isColor) {
    html +=
      '(function(){'
      + '  function upd(){var c=document.querySelector("input[name=cm]:checked");'
      + '    document.getElementById("static-colors").style.display=(c&&+c.value===1)?"block":"none";}'
      + '  document.querySelectorAll("input[name=cm]").forEach(function(r){r.addEventListener("change",upd);});'
      + '  upd();'
      + '})();'
      + 'function cpToggle(fid){'
      + '  var g=document.getElementById(fid+"_grid");'
      + '  var open=g.style.display==="none";'
      + '  document.querySelectorAll(".cp-grid").forEach(function(el){el.style.display="none";});'
      + '  if(open) g.style.display="flex";'
      + '}'
      + 'function cpPick(fid,idx){'
      + '  document.getElementById(fid).value=idx;'
      + '  var r=((idx>>4)&3)*85,g2=((idx>>2)&3)*85,b=(idx&3)*85;'
      + '  var tr=document.getElementById(fid+"_tr");'
      + '  tr.style.background="rgb("+r+","+g2+","+b+")";'
      + '  var lum=((idx>>4)&3)*2+((idx>>2)&3)*4+(idx&3);'
      + '  tr.style.color=lum>=5?"#000":"#fff";'
      + '  document.getElementById(fid+"_lbl").textContent=idx;'
      + '  var sw=document.getElementById(fid+"_grid");'
      + '  sw.querySelectorAll(".cp-sw").forEach(function(el,i){el.classList.toggle("cp-sel",i===idx);});'
      + '  document.getElementById(fid+"_grid").style.display="none";'
      + '}';
  }

  html +=
    'function doSave(){'
    + '  var im=document.querySelector("input[name=im]:checked");'
    + '  var il=document.querySelector("input[name=il]:checked");'
    + '  var tu=document.querySelector("input[name=tu]:checked");'
    + '  var du=document.querySelector("input[name=du]:checked");'
    + '  var inv=document.querySelector("input[name=inv]:checked");'
    + '  var cm=document.querySelector("input[name=cm]:checked");'
    + '  var wide=[],stack=[];'
    + '  for(var i=0;i<6;i++) wide.push(+document.getElementById("w"+i).value);'
    + '  for(var i=0;i<8;i++) stack.push(+document.getElementById("s"+i).value);'
    + '  function ci(id){var el=document.getElementById(id);return el?+el.value:0;}'
    + '  var s={'
    + '    infoMode:im?+im.value:0,infoLayout:il?+il.value:1,'
    + '    wide:wide,stack:stack,'
    + '    tempUnit:tu?+tu.value:0,distUnit:du?+du.value:0,'
    + '    invert:inv?+inv.value:0,colorMode:cm?+cm.value:0,'
    + '    colBg:ci("colBg"),colDigH:ci("colDigH"),colDigM:ci("colDigM"),'
    + '    colShadow:ci("colShadow"),colInfo:ci("colInfo")'
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
  sendSettings(loadSettings());
  fetchWeather();
  setInterval(fetchWeather, 30 * 60 * 1000);
});

Pebble.addEventListener('showConfiguration', function() {
  var isColor = false;
  try {
    var info = Pebble.getActiveWatchInfo();
    var p = info.platform;
    // Color platforms: basalt (Pebble Time), chalk (Time Round), emery (Time 2)
    // B&W platforms: aplite (OG/Steel), diorite (Pebble 2), flint (2 SE), gabbro
    isColor = (p === 'basalt' || p === 'chalk' || p === 'emery');
  } catch(e) {}
  Pebble.openURL('data:text/html,' + encodeURIComponent(buildConfigPage(loadSettings(), isColor)));
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e.response || e.response === 'CANCELLED') return;
  try {
    var s = JSON.parse(decodeURIComponent(e.response));
    saveSettings(s); sendSettings(s);
  } catch(err) { console.log('Config parse error: ' + err); }
});
