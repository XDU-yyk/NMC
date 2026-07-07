/**
 * @file index_html.h
 * @brief Unified NMC presentation dashboard.
 */

#ifndef INDEX_HTML_H
#define INDEX_HTML_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="Cache-Control" content="no-store">
<title>NMC Flight Deck</title>
<style>
:root{
  color-scheme:dark;
  --bg:#05080d;
  --bg2:#08111c;
  --panel:#0b1420;
  --panel2:#0f1b2a;
  --line:#203247;
  --line2:#2b455f;
  --text:#edf5ff;
  --muted:#8aa0b8;
  --cyan:#33d6ff;
  --green:#3df59a;
  --amber:#ffd166;
  --red:#ff5f6d;
  --blue:#6aa8ff;
}
*{box-sizing:border-box}
html,body{min-height:100%}
body{
  margin:0;
  color:var(--text);
  font-family:Inter,"Segoe UI",Arial,sans-serif;
  background:
    linear-gradient(rgba(51,214,255,.035) 1px,transparent 1px),
    linear-gradient(90deg,rgba(51,214,255,.035) 1px,transparent 1px),
    linear-gradient(135deg,var(--bg),var(--bg2) 62%,#07131a);
  background-size:32px 32px,32px 32px,auto;
  line-height:1.42;
}
button,input{font:inherit}
.app{width:min(1360px,100%);margin:0 auto;padding:16px}
.topbar{display:flex;align-items:center;justify-content:space-between;gap:14px;margin-bottom:14px}
.brand{display:flex;align-items:center;gap:12px;min-width:0}
.mark{
  width:46px;height:46px;display:grid;place-items:center;
  border:1px solid var(--line2);background:#0a1b29;color:var(--cyan);
  font-weight:800;border-radius:8px;box-shadow:0 0 24px rgba(51,214,255,.14) inset;
}
h1{margin:0;font-size:24px;line-height:1.05;font-weight:800}
.kicker{margin-top:4px;color:var(--muted);font-size:13px}
.statusbar{display:flex;gap:8px;align-items:center;flex-wrap:wrap;justify-content:flex-end}
.chip{
  min-height:30px;display:inline-flex;align-items:center;gap:7px;
  padding:5px 10px;border:1px solid var(--line);border-radius:999px;
  background:rgba(15,27,42,.72);color:var(--muted);font-size:12px;white-space:nowrap;
}
.dot{width:8px;height:8px;border-radius:50%;background:var(--muted);box-shadow:0 0 10px currentColor}
.ok{color:var(--green)}.warn{color:var(--amber)}.bad{color:var(--red)}.blue{color:var(--blue)}.cyan{color:var(--cyan)}
.layout{display:grid;grid-template-columns:minmax(360px,1.3fr) minmax(330px,.7fr);gap:14px}
.panel{
  border:1px solid var(--line);background:rgba(11,20,32,.9);border-radius:8px;
  box-shadow:0 18px 60px rgba(0,0,0,.22),0 0 0 1px rgba(255,255,255,.02) inset;
}
.panel-head{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:12px 12px 0}
.panel-title{font-weight:800;font-size:14px;text-transform:uppercase;color:#dcecff}
.panel-sub{font-size:12px;color:var(--muted);white-space:nowrap}
.camera{padding-bottom:12px}
.viewport{
  margin:10px 12px 0;aspect-ratio:4/3;min-height:280px;position:relative;overflow:hidden;
  border:1px solid #1b2b3d;border-radius:8px;background:#02060b;
}
.viewport:before{
  content:"";position:absolute;inset:0;pointer-events:none;
  background:linear-gradient(rgba(51,214,255,.08) 1px,transparent 1px);
  background-size:100% 32px;mix-blend-mode:screen;opacity:.38;z-index:2;
}
.viewport img{width:100%;height:100%;object-fit:contain;display:none;background:#02060b}
.empty{position:absolute;inset:0;display:grid;place-items:center;color:var(--muted);font-size:13px;text-align:center;padding:18px}
.hud-row{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:8px;margin:10px 12px 0}
.readout{border-top:1px solid var(--line);padding:10px 0 0;min-width:0}
.readout:first-child{border-top-color:transparent}
.label{font-size:11px;color:var(--muted);text-transform:uppercase;margin-bottom:3px}
.value{font-size:21px;font-weight:800;line-height:1.12;word-break:break-word}
.unit{font-size:12px;color:var(--muted);font-weight:500;margin-left:2px}
.side{display:grid;gap:14px}
.fc-panel{padding-bottom:12px}
.attitude-wrap{display:grid;grid-template-columns:150px 1fr;gap:12px;padding:12px}
.attitude{
  width:150px;height:150px;border:1px solid var(--line2);border-radius:50%;position:relative;overflow:hidden;
  background:#07111b;box-shadow:0 0 28px rgba(51,214,255,.12) inset;
}
.horizon{
  position:absolute;left:-36px;top:50%;width:222px;height:222px;margin-top:-111px;
  background:linear-gradient(#123f66 0 50%,#352818 50% 100%);
  transform-origin:center center;
}
.attitude:after{content:"";position:absolute;left:20px;right:20px;top:50%;height:1px;background:var(--cyan);box-shadow:0 0 12px var(--cyan)}
.cross{position:absolute;left:50%;top:50%;width:10px;height:10px;margin:-5px 0 0 -5px;border:1px solid var(--cyan);border-radius:50%}
.metric-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:9px}
.metric{
  min-height:66px;padding:9px;border-top:1px solid var(--line);background:rgba(15,27,42,.45);
}
.metric .value{font-size:19px}
.wide-metrics{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px;padding:0 12px 12px}
.strip{grid-column:1/-1;display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:14px}
.block{padding:12px;min-height:154px}
.block .panel-title{margin-bottom:12px}
.stack{display:grid;gap:9px}
.line{display:flex;align-items:center;justify-content:space-between;gap:12px;border-top:1px solid var(--line);padding-top:8px}
.line:first-child{border-top:0;padding-top:0}
.line span:first-child{color:var(--muted);font-size:12px;text-transform:uppercase}
.line strong{text-align:right;font-size:14px}
.rc-grid{display:grid;gap:8px;padding:0 12px 12px}
.rc-row{display:grid;grid-template-columns:54px 1fr 46px;gap:8px;align-items:center}
.rc-row span{font-size:12px;color:var(--muted)}
.bar{height:8px;border-radius:999px;background:#162234;overflow:hidden;border:1px solid #263a50}
.bar i{display:block;height:100%;width:50%;background:linear-gradient(90deg,var(--blue),var(--cyan));border-radius:inherit}
.toolbar{display:flex;gap:8px;align-items:center}
.btn{
  border:1px solid var(--line2);background:#102134;color:var(--text);
  border-radius:6px;padding:6px 10px;cursor:pointer;font-size:12px;
}
.btn:hover{border-color:var(--cyan);color:var(--cyan)}
.btn:active{transform:translateY(1px)}
@media (max-width:980px){
  .layout{grid-template-columns:1fr}
  .strip{grid-template-columns:repeat(2,minmax(0,1fr))}
}
@media (max-width:620px){
  .app{padding:10px}
  .topbar{display:block}
  .statusbar{justify-content:flex-start;margin-top:10px}
  .hud-row,.strip,.wide-metrics{grid-template-columns:1fr}
  .attitude-wrap{grid-template-columns:1fr}
  .attitude{margin:0 auto}
  .viewport{min-height:220px}
}
</style>
</head>
<body>
<div class="app">
  <header class="topbar">
    <div class="brand">
      <div class="mark">NMC</div>
      <div>
        <h1>NMC Flight Deck</h1>
        <div class="kicker">Live Telemetry Console</div>
      </div>
    </div>
    <div class="statusbar">
      <div id="netChip" class="chip warn"><span class="dot"></span><span>CONNECTING</span></div>
      <div id="fwChip" class="chip">FW --</div>
      <div id="clientChip" class="chip">CLIENTS --</div>
    </div>
  </header>

  <main class="layout">
    <section class="panel camera">
      <div class="panel-head">
        <div>
          <div class="panel-title">Camera</div>
          <div class="panel-sub" id="camSub">Frame stream</div>
        </div>
        <div class="toolbar">
          <div id="camChip" class="chip warn"><span class="dot"></span><span>STANDBY</span></div>
          <button id="retryCam" class="btn" type="button">RETRY</button>
        </div>
      </div>
      <div class="viewport">
        <img id="cam" alt="camera frame">
        <div id="camEmpty" class="empty">WAITING FOR FRAME</div>
      </div>
      <div class="hud-row">
        <div class="readout"><div class="label">Frames</div><div id="camFrames" class="value cyan">--</div></div>
        <div class="readout"><div class="label">Payload</div><div id="camBytes" class="value">--</div></div>
        <div class="readout"><div class="label">Age</div><div id="camAge" class="value">--</div></div>
        <div class="readout"><div class="label">Recoveries</div><div id="camRecoveries" class="value">--</div></div>
      </div>
    </section>

    <div class="side">
      <section class="panel fc-panel">
        <div class="panel-head">
          <div>
            <div class="panel-title">Flight Controller</div>
            <div class="panel-sub" id="fcSub">MSP telemetry</div>
          </div>
          <div id="fcChip" class="chip warn"><span class="dot"></span><span>MSP WAIT</span></div>
        </div>
        <div class="attitude-wrap">
          <div class="attitude"><div id="horizon" class="horizon"></div><div class="cross"></div></div>
          <div class="metric-grid">
            <div class="metric"><div class="label">Roll</div><div id="roll" class="value cyan">--</div></div>
            <div class="metric"><div class="label">Pitch</div><div id="pitch" class="value cyan">--</div></div>
            <div class="metric"><div class="label">Yaw</div><div id="yaw" class="value cyan">--</div></div>
            <div class="metric"><div class="label">State</div><div id="armed" class="value">--</div></div>
            <div class="metric"><div class="label">Cycle</div><div id="cycle" class="value">--</div></div>
            <div class="metric"><div class="label">Vario</div><div id="vario" class="value">--</div></div>
          </div>
        </div>
        <div class="rc-grid">
          <div class="rc-row"><span>ROLL</span><div class="bar"><i id="barRoll"></i></div><strong id="rcRoll">--</strong></div>
          <div class="rc-row"><span>PITCH</span><div class="bar"><i id="barPitch"></i></div><strong id="rcPitch">--</strong></div>
          <div class="rc-row"><span>THR</span><div class="bar"><i id="barThr"></i></div><strong id="rcThr">--</strong></div>
          <div class="rc-row"><span>YAW</span><div class="bar"><i id="barYaw"></i></div><strong id="rcYaw">--</strong></div>
        </div>
        <div class="wide-metrics">
          <div class="metric"><div class="label">AUX / Channels</div><div id="aux" class="value">--</div></div>
          <div class="metric"><div class="label">MSP Link</div><div id="msp" class="value">--</div></div>
        </div>
      </section>
    </div>

    <section class="panel block">
      <div class="panel-title">ToF Range</div>
      <div class="value cyan" id="tofMain">--</div>
      <div class="stack">
        <div class="line"><span>Status</span><strong id="tofStatus">--</strong></div>
        <div class="line"><span>Age</span><strong id="tofAge">--</strong></div>
        <div class="line"><span>Errors</span><strong id="tofErrors">--</strong></div>
      </div>
    </section>

    <section class="panel block">
      <div class="panel-title">GPS</div>
      <div class="value cyan" id="gpsMain">--</div>
      <div class="stack">
        <div class="line"><span>Location</span><strong id="gpsName">--</strong></div>
        <div class="line"><span>Satellites</span><strong id="gpsSats">--</strong></div>
        <div class="line"><span>Speed</span><strong id="gpsSpeed">--</strong></div>
      </div>
    </section>

    <section class="panel block">
      <div class="panel-title">Power</div>
      <div class="value warn" id="batMain">--</div>
      <div class="stack">
        <div class="line"><span>Cells</span><strong id="batCells">--</strong></div>
        <div class="line"><span>Altitude</span><strong id="altitude">--</strong></div>
        <div class="line"><span>Link Mode</span><strong id="outputMode">TELEMETRY</strong></div>
      </div>
    </section>

    <section class="panel block">
      <div class="panel-title">System</div>
      <div class="value blue" id="uptime">--</div>
      <div class="stack">
        <div class="line"><span>Heap</span><strong id="heap">--</strong></div>
        <div class="line"><span>Temp</span><strong id="temp">--</strong></div>
        <div class="line"><span>Errors</span><strong id="errors">--</strong></div>
      </div>
    </section>
  </main>
</div>

<script>
(function(){
  var CAMPUS={name:'Xidian University Changan Campus',lat:34.1266,lng:108.8372};
  var latest=null;
  var imgBusy=false;
  function el(id){return document.getElementById(id);}
  function finite(v){return Number.isFinite(Number(v));}
  function num(v,d){var n=Number(v);return Number.isFinite(n)?n.toFixed(d||0):'--';}
  function ms(v){return finite(v)?Math.max(0,Number(v)).toFixed(0)+' ms':'--';}
  function sec(v){return finite(v)?Math.floor(Number(v)/1000)+' s':'--';}
  function bytes(v){var n=Number(v)||0;return n>=1024?(n/1024).toFixed(1)+' KB':n+' B';}
  function meterMm(v){return finite(v)?(Number(v)/1000).toFixed(2)+' m':'--';}
  function cm(v){return finite(v)?Number(v).toFixed(0)+' cm':'--';}
  function deg(v){return finite(v)?Number(v).toFixed(1)+' deg':'--';}
  function setChip(id,state,text){
    var node=el(id);
    node.className='chip '+state;
    node.querySelector('span:last-child').textContent=text;
  }
  function setText(id,text){el(id).textContent=text;}
  function rcBar(id,v){
    var n=Number(v);
    var p=Number.isFinite(n)?Math.max(0,Math.min(100,(n-1000)/10)):0;
    el(id).style.width=p+'%';
  }
  function render(d){
    latest=d;
    setChip('netChip','ok','AP ONLINE');
    setText('fwChip','FW '+(d.fw||'unified'));
    var clients=(d.apStations!==undefined)?d.apStations:((d.clients!==undefined)?d.clients:d.clientCount);
    setText('clientChip','CLIENTS '+num(clients,0));

    var camOk=!!(d.camOnline&&d.camValid);
    setChip('camChip',camOk?'ok':'warn',camOk?'LIVE':'STANDBY');
    el('cam').style.display=camOk?'block':'none';
    el('camEmpty').style.display=camOk?'none':'grid';
    setText('camSub','errors '+num(d.camErrors,0));
    setText('camFrames',num(d.camFrames,0));
    setText('camBytes',bytes(d.camBytes));
    setText('camAge',ms(d.camAgeMs));
    setText('camRecoveries',num(d.camRecoveries,0));

    var fcLinked=!!d.fcRealOnline;
    setChip('fcChip',fcLinked?'ok':'warn',fcLinked?'MSP LINK':'MSP WAIT');
    setText('fcSub','rx '+num(d.fcMspRxBytes,0)+' bytes / timeout '+num(d.fcMspTimeouts,0));
    setText('roll',fcLinked?deg(d.roll):'--');
    setText('pitch',fcLinked?deg(d.pitch):'--');
    setText('yaw',fcLinked?deg(d.yaw):'--');
    setText('armed',fcLinked?(d.armed?'ARMED':'DISARMED'):'ACQUIRING');
    setText('cycle',fcLinked?num(d.fcCycleTimeUs,0)+' us':'--');
    setText('vario',fcLinked?cm(d.fcVario)+'/s':'--');
    el('horizon').style.transform='translateY('+(fcLinked?Math.max(-34,Math.min(34,Number(d.pitch)||0))*1.2:0)+'px) rotate('+(fcLinked?-(Number(d.roll)||0):0)+'deg)';
    setText('rcRoll',fcLinked?num(d.rcRoll,0):'--');
    setText('rcPitch',fcLinked?num(d.rcPitch,0):'--');
    setText('rcThr',fcLinked?num(d.rcThrottle,0):'--');
    setText('rcYaw',fcLinked?num(d.rcYaw,0):'--');
    rcBar('barRoll',fcLinked?d.rcRoll:1500);
    rcBar('barPitch',fcLinked?d.rcPitch:1500);
    rcBar('barThr',fcLinked?d.rcThrottle:1000);
    rcBar('barYaw',fcLinked?d.rcYaw:1500);
    setText('aux',fcLinked?('A1 '+num(d.rcAux1,0)+' / A2 '+num(d.rcAux2,0)+' / '+num(d.rcChannelCount,0)+'ch'):'--');
    setText('msp','TX '+num(d.fcMspTxFrames,0)+' / RX '+num(d.fcMspRxBytes,0));

    var tofOk=!!d.tofOnline;
    setText('tofMain',tofOk?meterMm(d.tofDist):'--');
    setText('tofStatus',tofOk?'VALID':'SEARCH');
    setText('tofAge',ms(d.tofAgeMs));
    setText('tofErrors',num(d.tofErrors,0));

    var gpsFix=!!d.gpsValid&&finite(d.gpsLat)&&finite(d.gpsLng);
    var lat=gpsFix?Number(d.gpsLat):CAMPUS.lat;
    var lng=gpsFix?Number(d.gpsLng):CAMPUS.lng;
    setText('gpsMain',lat.toFixed(6)+', '+lng.toFixed(6));
    setText('gpsName',gpsFix?'GNSS FIX':CAMPUS.name);
    setText('gpsSats',num(d.gpsSats,0));
    setText('gpsSpeed',finite(d.gpsSpeed)?Number(d.gpsSpeed).toFixed(1)+' km/h':'--');

    setText('batMain',fcLinked&&finite(d.batV)?Number(d.batV).toFixed(2)+' V':'--');
    setText('batCells',fcLinked&&d.batCells?num(d.batCells,0)+'S':'--');
    setText('altitude',fcLinked?cm(d.baroAlt):'--');
    setText('outputMode',d.fcRealOutputCompiled?'ASSIST BUILD':'TELEMETRY');

    setText('uptime',sec(d.uptime));
    setText('heap',bytes(d.freeHeap));
    setText('temp',finite(d.chipTemp)?Number(d.chipTemp).toFixed(1)+' C':'--');
    setText('errors',num(d.errorFlags,0));
  }
  async function poll(){
    try{
      var r=await fetch('/api/telemetry?_='+Date.now(),{cache:'no-store'});
      if(!r.ok) throw new Error('http '+r.status);
      render(await r.json());
    }catch(e){
      setChip('netChip','warn','RECONNECTING');
    }
  }
  function nextImage(delay){
    if(imgBusy) return;
    imgBusy=true;
    setTimeout(function(){
      var im=el('cam');
      var done=function(){imgBusy=false;nextImage(170);};
      im.onload=done;
      im.onerror=function(){imgBusy=false;nextImage(900);};
      im.src='/capture.jpg?ts='+Date.now();
    },delay||0);
  }
  el('retryCam').onclick=function(){
    fetch('/api/camera/retry?_='+Date.now(),{cache:'no-store'}).then(poll).catch(function(){});
  };
  poll();
  setInterval(poll,650);
  nextImage(120);
})();
</script>
</body>
</html>
)rawliteral";

#endif // INDEX_HTML_H
