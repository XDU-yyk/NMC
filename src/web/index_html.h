/**
 * @file    index_html.h
 * @brief   Minimal diagnostic dashboard
 */

#ifndef INDEX_HTML_H
#define INDEX_HTML_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NMC Web MVP</title>
<style>
:root{color-scheme:dark;--bg:#0a0e17;--panel:#111827;--line:#334155;--text:#e2e8f0;--muted:#94a3b8;--ok:#22c55e;--warn:#f59e0b;--err:#ef4444;--blue:#60a5fa;}
*{box-sizing:border-box}
body{margin:0;font-family:Arial,sans-serif;background:var(--bg);color:var(--text);padding:18px;line-height:1.45}
.wrap{max-width:980px;margin:0 auto}
header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:14px;flex-wrap:wrap}
h1{font-size:1.35rem;margin:0}
.pill{border:1px solid var(--line);border-radius:999px;padding:6px 10px;color:var(--muted);font-size:.82rem}
.pill.ok{border-color:var(--ok);color:var(--ok)}
.pill.warn{border-color:var(--warn);color:var(--warn)}
.pill.err{border-color:var(--err);color:var(--err)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:12px;min-height:92px}
.label{font-size:.78rem;color:var(--muted);margin-bottom:6px}
.value{font-size:1.45rem;font-weight:700}
.unit{font-size:.82rem;color:var(--muted);font-weight:400;margin-left:3px}
.ok{color:var(--ok)}.warn{color:var(--warn)}.err{color:var(--err)}.blue{color:var(--blue)}
.tag{display:inline-block;font-size:.7rem;padding:2px 6px;border-radius:4px;margin-left:6px;vertical-align:middle}
.tag.on{background:var(--ok);color:#000}
.tag.off{background:var(--line);color:var(--muted)}
.tag.err-tag{background:var(--err);color:#fff}
.log{margin-top:12px;color:var(--muted);font-size:.85rem}
pre{background:#0f172a;border:1px solid var(--line);border-radius:8px;padding:10px;font-size:.75rem;color:var(--muted);overflow:auto;max-height:200px}
.warnbox{border:1px solid #78350f;background:#451a03;border-radius:8px;padding:10px;font-size:.82rem;color:#fdba74;margin-bottom:10px;line-height:1.5}
.warnbox strong{color:#fbbf24}
</style>
</head>
<body>
<div class="wrap">

<div class="warnbox">
  <strong>&#x26A0; &#x98DE;&#x63A7;&#x5DF2;&#x635F;&#x574F;</strong><br>
  &#x6682;&#x505C;&#x98DE;&#x884C;/&#x7535;&#x673A;/&#x89E3;&#x9501;/&#x81EA;&#x4E3B;&#x63A7;&#x5236;&#x3002;&#x4EC5;&#x5C55;&#x793A; ESP32-S3 &#x4F20;&#x611F;&#x5668;&#x4E0E; Web &#x4EA4;&#x4E92;&#x3002;
</div>

<header>
  <div>
    <h1>NMC Web MVP</h1>
    <div class="log">HTTP polling &middot; <span id="dataSrc">sim</span></div>
  </div>
  <div class="pill warn" id="net">connecting</div>
</header>
<div class="grid">
  <div class="card"><div class="label">System</div><div class="value ok" id="sys">OK</div><div class="log" id="uptime">--</div></div>
  <div class="card" style="grid-column:span 2">
    <div class="label">ToF <span id="tofTag" class="tag off">off</span></div>
    <div class="value" id="tof">--<span class="unit">mm</span></div>
    <div class="log" id="tofDiag"></div>
  </div>
  <div class="card"><div class="label">Altitude</div><div class="value" id="alt">--<span class="unit">cm</span></div></div>
  <div class="card"><div class="label">Battery</div><div class="value" id="bat">--<span class="unit">V</span></div></div>
  <div class="card"><div class="label">Attitude</div><div class="value blue" id="att">--</div></div>
  <div class="card">
    <div class="label">GPS <span id="gpsTag" class="tag off">off</span></div>
    <div class="value" id="gps">--</div>
  </div>
  <div class="card">
    <div class="label">Flight Controller <span id="fcTag" class="tag off">off</span></div>
    <div class="value warn" id="fc">offline</div>
  </div>
  <div class="card"><div class="label">Heap</div><div class="value" id="heap">--<span class="unit">B</span></div></div>
</div>
<div class="log" id="errLog"></div>
<div class="log" id="log">Waiting for telemetry...</div>
<hr style="border-color:#1e293b;margin:14px 0">
<div style="text-align:center">
  <img id="cam" src="/capture.jpg" style="max-width:100%;border-radius:8px;display:none"
       onerror="this.style.display='none'"
       onload="this.style.display='inline';document.getElementById('camOff').style.display='none'">
  <div id="camOff" style="color:#94a3b8;font-size:.85rem;padding:20px">Camera unavailable</div>
  <div style="margin-top:6px;font-size:.78rem;color:#94a3b8">
    <a href="/stream" target="_blank">/stream (MJPEG)</a>
  </div>
</div>
<pre id="raw">(waiting)</pre>
</div>
<script>
(function(){
function $(id){return document.getElementById(id);}

function tick(){
  var url = '/api/telemetry';
  fetch(url)
    .then(function(r){ return r.json(); })
    .then(function(d){
      // Show raw
      $('raw').textContent = JSON.stringify(d, null, 2);

      // Update values
      $('net').textContent = 'online';
      $('net').className = 'pill ok';

      // ToF
      var v = d.tofDist;
      if(v !== undefined && v !== null) {
        $('tof').innerHTML = Math.round(v) + '<span class="unit">mm</span>';
      }
      var on = d.tofOnline;
      if(on !== undefined) {
        $('tofTag').textContent = on ? 'on' : 'off';
        $('tofTag').className = 'tag ' + (on ? 'on' : 'off');
      }

      // GPS
      var gpsOn = d.gpsOnline;
      if(gpsOn !== undefined) {
        $('gpsTag').textContent = gpsOn ? 'on' : 'off';
        $('gpsTag').className = 'tag ' + (gpsOn ? 'on' : 'off');
      }

      // FC
      var fcOn = d.fcOnline;
      if(fcOn !== undefined) {
        $('fcTag').textContent = fcOn ? 'on' : 'off';
        $('fcTag').className = 'tag ' + (fcOn ? 'on' : 'off');
        $('fc').textContent = fcOn ? 'online' : 'offline';
        $('fc').className = 'value ' + (fcOn ? 'ok' : 'warn');
      }

      $('log').textContent = 'OK ' + new Date().toLocaleTimeString();

      // Camera refresh
      var ci = document.getElementById('cam');
      if(ci && ci.style.display !== 'none') {
        ci.src = '/capture.jpg?_=' + Date.now();
      }
    })
    .catch(function(e){
      $('raw').textContent = 'FETCH ERROR: ' + e.message;
      $('net').textContent = 'retrying';
      $('net').className = 'pill warn';
    });
}

tick();
setInterval(tick, 2000);
})();
</script>
</body>
</html>
)rawliteral";

#endif // INDEX_HTML_H
