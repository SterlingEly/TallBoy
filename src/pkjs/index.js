// ============================================================
// TallBoy — src/pkjs/index.js  v3.47
// PebbleKit JS: weather, solar, drag-order config page
//
// SLOT TYPE IDs (must match main.c SlotType enum):
//   0=empty, 1=day, 2=date, 3=day+date, 4=temp, 5=weather
//   6=steps, 7=distance, 8=exp_steps, 9=pace
//   10=calories, 11=heart_rate, 12=sunrise, 13=sunset
//   14=daylight, 15=battery, 16=bluetooth
//   17=TIME_MARKER (the time digits, used in order array)
//
// MESSAGE KEYS:
//   WeatherTempF, WeatherTempC, WeatherCode
//   SunriseTime, SunsetTime
//   CfgSlot1..CfgSlot7 — the FULL ordered list of 7 items
//     (6 data slots + 1 time marker), positional
//   CfgTempUnit (0=F,1=C), CfgDistUnit (0=mi,1=km)
//   CfgClockFormat (0=12h,1=24h)
// ============================================================

var STORAGE_KEY = 'tallboy_settings_v347';
var TIME_MARKER = 17;

var SLOT_NAMES = {
  0:  'Empty',
  1:  'Day of week',
  2:  'Date',
  3:  'Day & Date',
  4:  'Temperature',
  5:  'Weather',
  6:  'Steps',
  7:  'Distance',
  8:  'Expected steps',
  9:  'Pace (%)',
  10: 'Calories',
  11: 'Heart rate',
  12: 'Sunrise',
  13: 'Sunset',
  14: 'Daylight',
  15: 'Battery',
  16: 'Bluetooth'
};

// Default order: day+date, weather, sunrise | TIME | steps, pace, battery
var DEFAULT_ORDER = [3, 5, 12, TIME_MARKER, 6, 9, 15];

var DEFAULTS = {
  order:          DEFAULT_ORDER.slice(),
  CfgTempUnit:    0,
  CfgDistUnit:    0,
  CfgClockFormat: 0
};

function loadSettings() {
  try {
    var saved = localStorage.getItem(STORAGE_KEY);
    if (saved) {
      var p = JSON.parse(saved);
      return { order: p.order || DEFAULT_ORDER.slice(),
               CfgTempUnit: p.CfgTempUnit||0,
               CfgDistUnit: p.CfgDistUnit||0,
               CfgClockFormat: p.CfgClockFormat||0 };
    }
  } catch(e) { console.log('Load error: '+e); }
  return { order: DEFAULT_ORDER.slice(), CfgTempUnit:0, CfgDistUnit:0, CfgClockFormat:0 };
}

function saveSettings(s) {
  try { localStorage.setItem(STORAGE_KEY, JSON.stringify(s)); } catch(e){}
}

function sendSettings(s) {
  var msg = {
    CfgTempUnit:    s.CfgTempUnit,
    CfgDistUnit:    s.CfgDistUnit,
    CfgClockFormat: s.CfgClockFormat
  };
  // Send the full 7-item ordered list as CfgSlot1..CfgSlot7
  for (var i = 0; i < 7; i++) msg['CfgSlot' + (i+1)] = s.order[i];
  Pebble.sendAppMessage(msg,
    function(){ console.log('Settings sent'); },
    function(e){ console.log('Settings error: '+e.error.message); });
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
        var tempF = Math.round(tempC*9/5+32);
        var code  = d.current.weather_code;
        var rise = 0, set = 0;
        if (d.daily&&d.daily.sunrise) {
          rise = Math.round(new Date(d.daily.sunrise[0]).getTime()/1000);
          set  = Math.round(new Date(d.daily.sunset[0]).getTime()/1000);
        }
        var msg = { WeatherTempF:tempF, WeatherTempC:tempC, WeatherCode:code };
        if (rise>0){ msg.SunriseTime=rise; msg.SunsetTime=set; }
        Pebble.sendAppMessage(msg,
          function(){console.log('Weather sent: '+tempF+'F code '+code);},
          function(e){console.log('Weather error: '+e.error.message);});
      } catch(e){ console.log('Parse error: '+e); }
    };
    xhr.open('GET', url); xhr.send();
  }, function(e){ console.log('Geo error: '+e.message); },
  { timeout:15000, maximumAge:300000 });
}

// ============================================================
// CONFIG PAGE — drag-to-reorder with "Time" as an item
// ============================================================
function buildConfigPage(s) {
  var order = s.order;

  // Build the slot option HTML for the select in each row
  function slotOptions(current) {
    var out = '<option value="' + TIME_MARKER + '"' + (current===TIME_MARKER?' selected':'') + '>🕐 Time</option>';
    for (var id in SLOT_NAMES) {
      out += '<option value="'+id+'"'+(current==id?' selected':'')+'>'+SLOT_NAMES[id]+'</option>';
    }
    return out;
  }

  var rows = '';
  for (var i = 0; i < 7; i++) {
    rows += '<div class="row" data-idx="'+i+'" draggable="true">';
    rows += '<span class="handle">☰</span>';
    rows += '<select class="slot-sel" data-idx="'+i+'">'+slotOptions(order[i])+'</select>';
    rows += '</div>';
  }

  return '<!DOCTYPE html><html><head>'
    + '<meta name="viewport" content="width=device-width,initial-scale=1">'
    + '<style>'
    + 'body{font:14px sans-serif;background:#111;color:#eee;padding:16px;max-width:360px;margin:0 auto}'
    + 'h2{color:#fff;margin:0 0 4px}p.sub{color:#888;font-size:12px;margin:0 0 20px}'
    + '.section{margin-bottom:22px}'
    + '.section h3{color:#aaa;font-size:11px;text-transform:uppercase;letter-spacing:1px;margin:0 0 8px;border-bottom:1px solid #333;padding-bottom:4px}'
    + '.row{display:flex;align-items:center;gap:8px;padding:6px 8px;background:#1a1a1a;border:1px solid #333;border-radius:6px;margin-bottom:6px;cursor:grab}'
    + '.row.drag-over{border-color:#4a9;background:#1a2a24}'
    + '.row.dragging{opacity:0.4}'
    + '.handle{color:#555;font-size:16px;user-select:none;flex-shrink:0}'
    + 'select{flex:1;padding:6px 8px;background:#222;border:1px solid #444;color:#eee;border-radius:4px;font-size:13px}'
    + '.toggle{display:flex}'
    + '.toggle input{display:none}'
    + '.toggle label{flex:1;text-align:center;padding:8px;background:#222;border:1px solid #444;cursor:pointer;color:#aaa;font-size:13px}'
    + '.toggle input:checked+label{background:#4a9;color:#fff;border-color:#4a9}'
    + '.toggle label:first-of-type{border-radius:6px 0 0 6px}'
    + '.toggle label:last-of-type{border-radius:0 6px 6px 0}'
    + '.pair{display:flex;gap:10px}.pair>div{flex:1}'
    + '#save{width:100%;padding:14px;background:#4a9;border:none;color:#fff;font-size:16px;font-weight:bold;border-radius:8px;cursor:pointer;margin-top:8px}'
    + '#save:active{background:#3a8}'
    + '</style></head><body>'
    + '<h2>TallBoy</h2><p class="sub">Drag to reorder · ☰ = drag handle</p>'
    + '<div class="section"><h3>Order (drag 🕐 Time to position)</h3>'
    + '<div id="list">' + rows + '</div></div>'
    + '<div class="section"><h3>Units</h3>'
    + '<div class="pair">'
    + '<div><label style="color:#aaa;font-size:12px;display:block;margin-bottom:4px">Temp</label>'
    + '<div class="toggle">'
    + '<input type="radio" name="tu" id="tu0" value="0"'+(s.CfgTempUnit===0?' checked':'')+'><label for="tu0">°F</label>'
    + '<input type="radio" name="tu" id="tu1" value="1"'+(s.CfgTempUnit===1?' checked':'')+'><label for="tu1">°C</label>'
    + '</div></div>'
    + '<div><label style="color:#aaa;font-size:12px;display:block;margin-bottom:4px">Distance</label>'
    + '<div class="toggle">'
    + '<input type="radio" name="du" id="du0" value="0"'+(s.CfgDistUnit===0?' checked':'')+'><label for="du0">mi</label>'
    + '<input type="radio" name="du" id="du1" value="1"'+(s.CfgDistUnit===1?' checked':'')+'><label for="du1">km</label>'
    + '</div></div></div></div>'
    + '<div class="section"><h3>Clock</h3>'
    + '<div class="toggle">'
    + '<input type="radio" name="cf" id="cf0" value="0"'+(s.CfgClockFormat===0?' checked':'')+'><label for="cf0">12h</label>'
    + '<input type="radio" name="cf" id="cf1" value="1"'+(s.CfgClockFormat===1?' checked':'')+'><label for="cf1">24h</label>'
    + '</div></div>'
    + '<button id="save">Save</button>'
    + '<script>'
    // Drag-to-reorder
    + 'var dragging=null;'
    + 'document.querySelectorAll(".row").forEach(function(r){'
    + '  r.addEventListener("dragstart",function(){dragging=r;r.classList.add("dragging");});'
    + '  r.addEventListener("dragend",function(){r.classList.remove("dragging");});'
    + '  r.addEventListener("dragover",function(e){e.preventDefault();r.classList.add("drag-over");});'
    + '  r.addEventListener("dragleave",function(){r.classList.remove("drag-over");});'
    + '  r.addEventListener("drop",function(e){'
    + '    e.preventDefault();r.classList.remove("drag-over");'
    + '    if(dragging&&dragging!==r){'
    + '      var list=document.getElementById("list");'
    + '      var kids=[].slice.call(list.children);'
    + '      var fi=kids.indexOf(dragging),ti=kids.indexOf(r);'
    + '      if(fi<ti)list.insertBefore(dragging,r.nextSibling);'
    + '      else list.insertBefore(dragging,r);'
    + '    }'
    + '  });'
    + '});'
    // Save
    + 'document.getElementById("save").onclick=function(){'
    + '  var order=[];'
    + '  document.querySelectorAll(".slot-sel").forEach(function(s,i){'
    + '    var row=document.getElementById("list").children[i];'
    + '    var sel=row.querySelector(".slot-sel");'
    + '    order.push(parseInt(sel.value));'
    + '  });'
    + '  var tu=document.querySelector("input[name=tu]:checked");'
    + '  var du=document.querySelector("input[name=du]:checked");'
    + '  var cf=document.querySelector("input[name=cf]:checked");'
    + '  var s={order:order,CfgTempUnit:tu?parseInt(tu.value):0,'
    + '         CfgDistUnit:du?parseInt(du.value):0,CfgClockFormat:cf?parseInt(cf.value):0};'
    + '  location.href="pebblejs://close#"+encodeURIComponent(JSON.stringify(s));'
    + '};'
    + '<\/script></body></html>';
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
  var html = buildConfigPage(s);
  Pebble.openURL('data:text/html,' + encodeURIComponent(html));
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e.response || e.response === 'CANCELLED') return;
  try {
    var s = JSON.parse(decodeURIComponent(e.response));
    saveSettings(s);
    sendSettings(s);
  } catch(err) { console.log('Config parse error: '+err); }
});
