/**
 * @file index_html.h
 * @brief Unified diagnostic dashboard.
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
<title>NMC 智能伞仪表盘</title>
<style>
:root{
  color-scheme:dark;
  --bg:#070b12;
  --panel:#111827;
  --panel2:#0f172a;
  --line:#263244;
  --text:#e5e7eb;
  --muted:#94a3b8;
  --ok:#22c55e;
  --warn:#f59e0b;
  --bad:#ef4444;
  --blue:#38bdf8;
}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font-family:Arial,sans-serif;line-height:1.42}
.wrap{width:min(1180px,100%);margin:0 auto;padding:14px}
header{display:flex;align-items:flex-start;justify-content:space-between;gap:12px;margin-bottom:12px}
h1{font-size:22px;margin:0 0 3px}
.sub{color:var(--muted);font-size:13px}
.pill{border:1px solid var(--line);border-radius:999px;padding:7px 10px;font-size:13px;color:var(--muted);white-space:nowrap}
.pill.ok{border-color:var(--ok);color:var(--ok)}
.pill.warn{border-color:var(--warn);color:var(--warn)}
.layout{display:grid;grid-template-columns:minmax(280px,1.25fr) minmax(300px,.95fr);gap:12px}
.camera,.panel,.warnbox{border:1px solid var(--line);background:var(--panel);border-radius:8px}
.warnbox{padding:10px;margin-bottom:12px;color:#fed7aa;background:#32190b;border-color:#7c2d12;font-size:13px}
.camera{padding:10px}
.camera-head{display:flex;justify-content:space-between;gap:8px;align-items:center;margin-bottom:8px}
.title{font-weight:700;font-size:15px}
.frame{aspect-ratio:4/3;background:#020617;border:1px solid #1f2937;border-radius:6px;display:flex;align-items:center;justify-content:center;overflow:hidden}
.frame img{width:100%;height:100%;object-fit:contain;display:none}
.empty{color:var(--muted);font-size:13px;padding:14px;text-align:center}
.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:8px}
.stat{background:var(--panel2);border:1px solid var(--line);border-radius:6px;padding:8px;min-height:58px}
.label{font-size:12px;color:var(--muted);margin-bottom:3px}
.value{font-size:20px;font-weight:700;word-break:break-word}
.unit{font-size:12px;color:var(--muted);font-weight:400;margin-left:2px}
.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}.blue{color:var(--blue)}
.panel{padding:10px}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px}
.wide{grid-column:1/-1}
.tag{display:inline-flex;align-items:center;height:20px;padding:0 7px;border-radius:999px;background:#334155;color:var(--muted);font-size:12px}
.tag.ok{background:#064e3b;color:#bbf7d0}
.tag.warn{background:#713f12;color:#fde68a}
.tag.bad{background:#7f1d1d;color:#fecaca}
.actions{display:flex;align-items:center;gap:8px;margin-top:8px;flex-wrap:wrap}
button{border:1px solid var(--line);background:#172033;color:var(--text);border-radius:6px;padding:8px 10px;font-size:13px}
button:active{transform:translateY(1px)}
.dirwarn{margin:8px 0;padding:8px 10px;border-radius:6px;border:1px solid #7f1d1d;background:#2a1414;color:#fca5a5;font-size:12px}
.dirwarn.unlocked{border-color:#166534;background:#132a19;color:#86efac}
.dirpad{display:flex;gap:18px;flex-wrap:wrap;margin:10px 0}
.joybox{flex:1;min-width:150px}
.joylabel{font-size:12px;color:var(--muted,#94a3b8);margin-bottom:6px}
.pad{position:relative;width:140px;height:140px;border-radius:10px;border:1px solid var(--line);background:var(--panel2);touch-action:none;margin:0 auto}
.knob{position:absolute;left:50%;top:50%;width:34px;height:34px;margin:-17px 0 0 -17px;border-radius:50%;background:#2563eb;border:1px solid #3b82f6}
.dirbtns{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:8px}
.dbtn{width:100%}
.dbtn.active{background:#2563eb;border-color:#3b82f6}
.msg{font-size:12px;color:var(--muted)}
pre{white-space:pre-wrap;word-break:break-word;margin:10px 0 0;padding:10px;border-radius:6px;border:1px solid var(--line);background:#020617;color:var(--muted);font-size:11px;max-height:220px;overflow:auto}
@media (max-width:820px){
  .layout{grid-template-columns:1fr}
  header{display:block}
  .pill{display:inline-block;margin-top:8px}
}
@media (max-width:520px){
  .wrap{padding:10px}
  .stats,.grid{grid-template-columns:1fr}
  h1{font-size:19px}
  .value{font-size:18px}
}
</style>
</head>
<body>
<div class="wrap">
  <div id="safetyBanner" class="warnbox">默认安全固件：只显示传感器、相机和仿真飞控；不会解锁、上锁、驱动电机，也不会发送 MSP 控制写入。</div>
  <header>
    <div>
      <h1>NMC 智能伞仪表盘</h1>
      <div class="sub">相机、ToF、GPS、飞控状态和系统遥测集中显示在同一个 AP 页面。</div>
    </div>
    <div id="net" class="pill warn">连接中</div>
  </header>

  <main class="layout">
    <section class="camera">
      <div class="camera-head">
        <div class="title">相机</div>
        <span id="camTag" class="tag warn">启动中</span>
      </div>
      <div class="frame">
        <img id="cam" alt="相机画面">
        <div id="camEmpty" class="empty">暂无画面</div>
      </div>
      <div class="stats">
        <div class="stat"><div class="label">帧数</div><div id="camFrames" class="value">--</div></div>
        <div class="stat"><div class="label">延迟</div><div id="camAge" class="value">--<span class="unit">ms</span></div></div>
        <div class="stat"><div class="label">错误</div><div id="camErr" class="value">--</div></div>
      </div>
      <div class="actions">
        <button id="retryCam" type="button">重试相机</button>
        <span id="retryMsg" class="msg"></span>
      </div>
    </section>

    <section class="panel">
      <div class="grid">
        <div class="stat">
          <div class="label">ToF <span id="tofTag" class="tag warn">关闭</span></div>
          <div id="tofDist" class="value">--<span class="unit">mm</span></div>
          <div id="tofDiag" class="sub"></div>
        </div>
        <div class="stat">
          <div class="label">GPS <span id="gpsTag" class="tag warn">关闭</span></div>
          <div id="gpsMain" class="value">--</div>
          <div id="gpsDiag" class="sub"></div>
        </div>
        <div class="stat">
          <div class="label">飞控 <span id="fcTag" class="tag bad">离线</span></div>
          <div id="fcMain" class="value warn">暂停</div>
          <div id="fcDiag" class="sub">SIM 未启用</div>
          <div class="actions">
            <button class="fcSimBtn" data-scenario="idle" type="button">待机</button>
            <button class="fcSimBtn" data-scenario="hover" type="button">悬停</button>
            <button class="fcSimBtn" data-scenario="follow" type="button">跟随</button>
            <button class="fcSimBtn" data-scenario="low_battery" type="button">低电量</button>
            <button class="fcSimBtn" data-scenario="failsafe" type="button">失控保护</button>
          </div>
          <div id="fcSimMsg" class="msg"></div>
        </div>
        <div class="stat">
          <div class="label">系统</div>
          <div id="sysMain" class="value ok">正常</div>
          <div id="sysDiag" class="sub"></div>
        </div>
        <div class="stat">
          <div class="label">剩余内存</div>
          <div id="heap" class="value">--<span class="unit">B</span></div>
        </div>
        <div class="stat">
          <div class="label">运行时间</div>
          <div id="uptime" class="value">--<span class="unit">s</span></div>
        </div>
        <div class="stat wide">
          <div class="label">位置</div>
          <div id="position" class="value blue">未定位</div>
        </div>
        <div class="stat wide">
          <div class="label">RC 输入</div>
          <div id="rcMain" class="value blue">--</div>
        </div>
        <div class="stat wide">
          <div class="label">FC-ready 门槛</div>
          <div id="fcGateDetail" class="value warn">未编译真实输出</div>
          <div id="fcOutDetail" class="sub"></div>
          <div id="fcMspDetail" class="sub"></div>
        </div>
        <div class="stat wide">
          <div class="label">MC6C 接收机检查</div>
          <div id="rxReady" class="value warn">等待真实 MSP</div>
          <div id="rxDetail" class="sub"></div>
          <div class="sub">方向自检必须无桨，建议只接 USB、不接动力电池；若 CH5 是 ARM，只用于确认 Receiver 变化。</div>
          <div class="actions">
            <button id="rxWizardStart" type="button">开始方向自检</button>
            <button id="rxWizardRecord" type="button">记录当前步骤</button>
            <button id="rxWizardReset" type="button">重置自检</button>
          </div>
          <div id="rxWizardStep" class="msg">等待开始</div>
          <div id="rxWizardResult" class="sub"></div>
        </div>
      </div>
      <pre id="raw">（等待数据）</pre>
    </section>

    <section class="panel">
      <div class="title">方向控制（仿真）</div>
      <div class="sub">按方向按钮或推动摇杆，飞机在仿真中按该方向移动。FC-ready 真机辅助只采用横滚、俯仰、偏航；油门始终由 MC6C 控制。</div>
      <div class="dirwarn" id="dirGate">真机输出：已锁定（仅仿真）。飞控修复并通过安全检查前不可解锁。</div>
      <div class="dirpad">
        <div class="joybox">
          <div class="joylabel">前后 / 左右</div>
          <div id="padMove" class="pad"><div id="knobMove" class="knob"></div></div>
          <div class="dirbtns">
            <button class="dbtn" data-move="fwd" type="button">前 ↑</button>
            <button class="dbtn" data-move="back" type="button">后 ↓</button>
            <button class="dbtn" data-move="left" type="button">左 ←</button>
            <button class="dbtn" data-move="right" type="button">右 →</button>
          </div>
        </div>
        <div class="joybox">
          <div class="joylabel">偏航 / 油门仿真</div>
          <div id="padYaw" class="pad"><div id="knobYaw" class="knob"></div></div>
          <div class="dirbtns">
            <button class="dbtn" data-move="yawl" type="button">左转 ⟲</button>
            <button class="dbtn" data-move="yawr" type="button">右转 ⟳</button>
            <button class="dbtn" data-move="up" type="button">升仿真 +</button>
            <button class="dbtn" data-move="down" type="button">降仿真 −</button>
          </div>
        </div>
      </div>
      <div class="actions">
        <button id="stopBtn" type="button">悬停/归中</button>
        <button id="takeoverBtn" type="button">暂停网页控制</button>
        <span id="dirMsg" class="msg"></span>
      </div>
      <div class="grid">
        <div class="stat"><div class="label">仿真位移</div><div id="simPos" class="value blue">--</div></div>
        <div class="stat"><div class="label">航向</div><div id="simHdg" class="value blue">--<span class="unit">°</span></div></div>
        <div class="stat"><div class="label">输出 RC</div><div id="simRc" class="value blue">--</div></div>
        <div class="stat"><div class="label">状态</div><div id="dirState" class="value">中位保持</div></div>
      </div>
    </section>
  </main>
</div>

<script>
(function(){
  function el(id){return document.getElementById(id);}
  function tag(id,state,text){
    var n=el(id);
    n.textContent=text;
    n.className='tag '+state;
  }
  function num(v,d){
    if(v===undefined||v===null||isNaN(Number(v))) return '--';
    return Number(v).toFixed(d||0);
  }
  function sec(ms){return Math.floor((Number(ms)||0)/1000);}
  function onlineText(ok){return ok?'在线':'离线';}
  function scenarioText(name){
    var map={
      idle:'待机',
      hover:'悬停',
      follow:'跟随',
      low_battery:'低电量',
      failsafe:'失控保护',
      real_fc:'真实飞控'
    };
    return map[name]||name||'模拟';
  }
  function responseText(t){
    t=(t||'').trim();
    if(t==='ok') return '已设置';
    if(t==='sent') return '已发送';
    if(t==='bad scenario') return '场景无效';
    if(t==='missing scenario') return '缺少场景';
    if(t==='unsupported') return '当前固件不支持';
    return t||'已发送';
  }
  function yn(ok){return ok?'OK':'未过';}
  function rcVal(key){return Number(latest[key]||0);}
  function rxSnapshot(){
    return {
      rcRoll:rcVal('rcRoll'),
      rcPitch:rcVal('rcPitch'),
      rcThrottle:rcVal('rcThrottle'),
      rcYaw:rcVal('rcYaw'),
      rcAux1:rcVal('rcAux1'),
      rcAux2:rcVal('rcAux2')
    };
  }
  function renderRxWizard(){
    if(!rxWizard.active){
      el('rxWizardStep').textContent='等待开始';
      if(!rxWizard.results.length) el('rxWizardResult').textContent='';
      return;
    }
    var step=rxSteps[rxWizard.step];
    el('rxWizardStep').textContent='步骤 '+(rxWizard.step+1)+'/'+rxSteps.length+'：'+step.name+'。'+step.text;
  }
  function finishRxWizard(){
    var ok=rxWizard.results.every(function(r){return r.ok;});
    var lines=rxWizard.results.map(function(r){
      return r.name+': '+(r.ok?'通过':'未过')+'（'+r.detail+'）';
    });
    lines.push(ok?'结论：双向方向自检通过，可继续无桨台架检查。':'结论：先不要继续。优先在 Betaflight Receiver/Channel Map 或 MC6C 舵机反向拨码里修正。');
    lines.push('提醒：这不是装桨或起飞许可；CH5 若用于 ARM，本自检只能在无桨、最好无动力电池状态下做 Receiver 变化确认。');
    el('rxWizardResult').textContent=lines.join('\n');
    el('rxWizardStep').textContent='自检完成';
    rxWizard.active=false;
  }
  function recordRxStep(){
    if(!rxWizard.active) return;
    if(!latest.fcRealOnline){
      el('rxWizardResult').textContent='真实 MSP 未在线，不能做 MC6C 方向自检。';
      return;
    }
    var step=rxSteps[rxWizard.step];
    var snap=rxSnapshot();
    if(step.kind==='baseline'){
      var centerOk=(snap.rcRoll>=1400&&snap.rcRoll<=1600&&snap.rcPitch>=1400&&snap.rcPitch<=1600&&snap.rcYaw>=1400&&snap.rcYaw<=1600);
      var throttleLow=snap.rcThrottle<=1200;
      var auxLow=(snap.rcAux1<=1300&&snap.rcAux2<=1300);
      var ok=centerOk&&throttleLow&&auxLow;
      rxWizard.base=snap;
      rxWizard.results.push({name:step.name,ok:ok,detail:'横滚 '+snap.rcRoll+', 俯仰 '+snap.rcPitch+', 油门 '+snap.rcThrottle+', 偏航 '+snap.rcYaw+', AUX1 '+snap.rcAux1+', AUX2 '+snap.rcAux2});
    }else{
      if(!rxWizard.base){
        el('rxWizardResult').textContent='请先记录基准。';
        return;
      }
      var v=snap[step.key];
      var b=rxWizard.base[step.key];
      var delta=v-b;
      var ok=false;
      if(step.kind==='switch') ok=(b<=1300&&v>=1700);
      else if(step.expect==='any') ok=Math.abs(delta)>=250;
      else ok=(step.expect==='up'?delta>=250:delta<=-250);
      var dirText=delta>0?'增大':(delta<0?'减小':'无变化');
      var extra=step.expect==='any'?'；方向以 Betaflight 模型预览确认':'';
      rxWizard.results.push({name:step.name,ok:ok,detail:'基准 '+b+' -> 当前 '+v+'，变化 '+delta+'（'+dirText+'）'+extra});
    }
    rxWizard.step++;
    if(rxWizard.step>=rxSteps.length) finishRxWizard();
    else renderRxWizard();
  }

  var latest = {};
  var camBusy = false;
  var camSeq = 0;
  var camFail = 0;
  var retryBusy = false;
  var rxWizard = {
    active:false,
    step:0,
    base:null,
    results:[]
  };
  var rxSteps = [
    {kind:'baseline', name:'基准', text:'无桨，建议只接 USB/不接动力电池。松开横滚/俯仰/偏航，油门最低，CH5/CH6 低位，然后点记录。'},
    {kind:'axis', name:'AIL 右', key:'rcRoll', expect:'up', text:'横滚杆向右打到底并保持，然后点记录。期望 CH1 明显增大。'},
    {kind:'axis', name:'AIL 左', key:'rcRoll', expect:'down', text:'横滚杆向左打到底并保持，然后点记录。期望 CH1 明显减小。'},
    {kind:'axis', name:'ELE 前', key:'rcPitch', expect:'any', text:'俯仰杆向前/上打到底并保持，然后点记录。这里先确认 CH2 有明显变化；正反方向必须再看 Betaflight 模型预览。'},
    {kind:'axis', name:'ELE 后', key:'rcPitch', expect:'any', text:'俯仰杆向后/下打到底并保持，然后点记录。这里先确认 CH2 有明显反向动作；正反方向必须再看 Betaflight 模型预览。'},
    {kind:'axis', name:'THR 上', key:'rcThrottle', expect:'up', text:'无桨，最好不接动力电池。把油门推到高位并保持，然后点记录。期望油门值明显增大。'},
    {kind:'axis', name:'RUD 右', key:'rcYaw', expect:'up', text:'偏航杆向右打到底并保持，然后点记录。期望偏航值明显增大。'},
    {kind:'axis', name:'RUD 左', key:'rcYaw', expect:'down', text:'偏航杆向左打到底并保持，然后点记录。期望偏航值明显减小。'},
    {kind:'switch', name:'CH5 高位', key:'rcAux1', text:'确认无桨。若 CH5 是 ARM，只在 Receiver 页确认数值变化；把 CH5/AUX1 拨到高位并保持，然后点记录。'},
    {kind:'switch', name:'CH6 高位', key:'rcAux2', text:'把 CH6/AUX2 拨到高位并保持，然后点记录。'}
  ];

  function update(d){
    latest=d||{};
    el('raw').textContent=JSON.stringify(d,null,2);
    el('net').textContent='在线';
    el('net').className='pill ok';

    var camOk=!!(d.camOnline&&d.camValid);
    tag('camTag',camOk?'ok':'warn',onlineText(camOk));
    el('camFrames').textContent=num(d.camFrames,0);
    el('camAge').innerHTML=num(d.camAgeMs,0)+'<span class="unit">ms</span>';
    el('camErr').textContent=num(d.camErrors,0);
    el('camEmpty').textContent=camOk?'暂无画面':(d.camOnline?'等待新画面':'相机离线，请检查接线和供电；保持本页面连接时可重试相机。');

    var tofOk=!!d.tofOnline;
    tag('tofTag',tofOk?'ok':'warn',onlineText(tofOk));
    el('tofDist').innerHTML=num(d.tofDist,0)+'<span class="unit">mm</span>';
    el('tofDiag').textContent='状态 '+num(d.tofStatus,0)+' / 错误 '+num(d.tofErrors,0)+' / 延迟 '+num(d.tofAgeMs,0)+'ms';

    var gpsOnline=!!d.gpsOnline;
    var gpsValid=!!d.gpsValid;
    tag('gpsTag',gpsValid?'ok':(gpsOnline?'warn':'warn'),gpsValid?'已定位':(gpsOnline?'搜星中':'离线'));
    el('gpsMain').textContent=(d.gpsSats||0)+' 星';
    el('gpsDiag').textContent='字符 '+num(d.gpsChars,0)+' / 校验失败 '+num(d.gpsFailedChecksum,0)+' / 延迟 '+num(d.gpsAgeMs,0)+'ms';
    el('position').textContent=gpsValid ? (Number(d.gpsLat).toFixed(6)+', '+Number(d.gpsLng).toFixed(6)) : '未定位';

    var fcOnline=!!d.fcOnline;
    var fcSim=!!d.fcSimulated;
    var fcBad=!!d.fcFailsafe;
    if(d.fcRealOutputCompiled){
      el('safetyBanner').textContent='FC-ready 专用构建：只在真实 MSP 在线、Betaflight ARM 位已解锁、MC6C CH6/AUX2 高位、Web 方向心跳新鲜且 Betaflight MSP Override 已配置时，才可能发送受限 MSP_SET_RAW_RC；仅用于无桨台架验证，不是第一次手动飞行固件。';
    }else{
      el('safetyBanner').textContent='默认安全固件：只显示传感器、相机和仿真飞控；不会解锁、上锁、驱动电机，也不会发送 MSP 控制写入。';
    }
    tag('fcTag',fcBad?'bad':(fcOnline?'ok':'bad'),fcSim?'SIM':onlineText(fcOnline));
    el('fcMain').textContent=fcOnline?(scenarioText(d.fcScenarioName)+' / '+(d.armed?'已解锁':'未解锁')):'暂停';
    el('fcMain').className='value '+(fcBad?'bad':(fcOnline?'ok':'warn'));
    el('fcDiag').textContent='模式 '+num(d.flightMode,0)+' / 周期 '+num(d.fcCycleTimeUs,0)+'us / 负载 '+num(d.fcCpuLoad,0)+'% / 链路 '+num(d.fcLinkQuality,0)+'% / 垂速 '+num(d.fcVario,0)+'cm/s';
    el('rcMain').textContent='横滚 '+num(d.rcRoll,0)+' / 俯仰 '+num(d.rcPitch,0)+' / 油门 '+num(d.rcThrottle,0)+' / 偏航 '+num(d.rcYaw,0)+' / AUX1 '+num(d.rcAux1,0)+' / AUX2 '+num(d.rcAux2,0)+' / MSP共 '+num(d.rcChannelCount,0)+' 路';
    if(d.fcRealOutputCompiled){
      var gate=d.fcAssistGateOpen;
      el('fcGateDetail').textContent='真实MSP '+(d.fcRealOnline?'在线':'离线')+' / Betaflight ARM位 '+(d.armed?'已解锁':'未解锁')+' / CH6许可 '+(d.fcAssistSwitch?'高位':'低位')+' / 输出 '+(gate?'可放行':'锁定');
      el('fcGateDetail').className='value '+(gate?'ok':'warn');
      el('fcOutDetail').textContent='输出诊断：请求 '+num(d.fcOutOverrideRequests,0)+' / 发送OK '+num(d.fcOutRawOk,0)+' / 发送失败 '+num(d.fcOutRawFail,0)+' / 门槛拦截 '+num(d.fcOutGateBlocks,0)+' / 超时归中 '+num(d.fcOutStaleBlocks,0)+' / 原因 '+(d.fcOutReason||'n/a');
      el('fcMspDetail').textContent='MSP诊断：TX帧 '+num(d.fcMspTxFrames,0)+' / TX字节 '+num(d.fcMspTxBytes,0)+' / RX字节 '+num(d.fcMspRxBytes,0)+' / 超时 '+num(d.fcMspTimeouts,0)+' / 最后错误 '+(d.fcMspLastError||'none')+' / 最后RC R/P/Y/T/A1/A2 '+num(d.fcOutRoll,0)+'/'+num(d.fcOutPitch,0)+'/'+num(d.fcOutYaw,0)+'/'+num(d.fcOutThrottle,0)+'/'+num(d.fcOutAux1,0)+'/'+num(d.fcOutAux2,0);
    el('dirGate').textContent=gate?'真机输出：门槛已打开（FC在线、Betaflight ARM位已解锁、CH6高位）。只输出横滚/俯仰/偏航；网页升降只影响仿真，真实油门由 MC6C 控制。':'真机输出：锁定。需要真实MSP在线、Betaflight ARM位已解锁、MC6C CH6/AUX2 高位、Betaflight MSP Override 已配置，并持续收到方向心跳；真实油门仍由 MC6C 控制。';
      el('dirGate').className='dirwarn '+(gate?'unlocked':'');
    }else{
      el('fcGateDetail').textContent='默认安全固件：未编译真实输出，只显示仿真/传感器';
      el('fcGateDetail').className='value warn';
      el('fcOutDetail').textContent='';
      el('fcMspDetail').textContent='';
      el('dirGate').textContent='真机输出：未编译（仅仿真）。飞控修复并通过安全检查前不可启用。';
      el('dirGate').className='dirwarn';
    }
    if(d.fcRealOnline){
      el('rxReady').textContent=d.rxBenchReady?'可继续无桨台架检查':'暂不继续';
      el('rxReady').className='value '+(d.rxBenchReady?'ok':'warn');
      el('rxDetail').textContent='6路 '+yn(d.rxHasSixChannels)+' / 横滚俯仰偏航中位 '+yn(d.rxCenterOk)+' / 油门低位 '+yn(d.rxThrottleLow)+' / CH5低位 '+yn(d.rxAux1Low)+' / CH6低位 '+yn(d.rxAux2Low)+' / 开关档位清楚 '+yn(d.rxAux1Valid&&d.rxAux2Valid);
    }else if(d.fcRealOutputCompiled){
      el('rxReady').textContent='等待真实 MSP';
      el('rxReady').className='value warn';
      el('rxDetail').textContent='新飞控接好 UART3 MSP 后，这里会检查 MC6C CH1-CH6、油门低位和 AUX 开关。';
    }else{
      el('rxReady').textContent='默认固件不检查真实接收机';
      el('rxReady').className='value warn';
      el('rxDetail').textContent='需要未来 FC-ready 固件和新飞控 MSP 在线后才显示真实 MC6C 检查。';
    }

    el('heap').innerHTML=num(d.freeHeap,0)+'<span class="unit">B</span>';
    el('uptime').innerHTML=sec(d.uptime)+'<span class="unit">s</span>';
    el('sysDiag').textContent='客户端 '+num(d.clients,0)+' / 连接设备 '+num(d.apStations,0)+' / 温度 '+num(d.chipTemp,1)+'C';

    // 方向控制仿真显示 (Manual 场景)
    if(d.fcScenarioName==='manual'){
      el('simPos').textContent='前后 '+(Number(d.posX)/1000).toFixed(2)+'m / 左右 '+(Number(d.posY)/1000).toFixed(2)+'m';
      el('simHdg').innerHTML=num(d.yaw,0)+'<span class="unit">°</span>';
      el('simRc').textContent='横滚 '+num(d.rcRoll,0)+' / 俯仰 '+num(d.rcPitch,0)+' / 偏航 '+num(d.rcYaw,0)+' / 油门 '+num(d.rcThrottle,0);
      var moving=(Number(d.rcRoll)!==1500||Number(d.rcPitch)!==1500||Number(d.rcYaw)!==1500);
      if(!takeover) el('dirState').textContent = moving ? '移动中' : '中位保持';
    }
  }

  function poll(){
    return fetch('/api/telemetry?_='+Date.now(),{cache:'no-store'})
      .then(function(r){return r.json();})
      .then(update)
      .catch(function(e){
        el('net').textContent='重连中';
        el('net').className='pill warn';
        el('raw').textContent='遥测错误：'+e.message;
      });
  }

  function scheduleCam(ms){setTimeout(refreshCam,ms);}
  function refreshCam(){
    if(camBusy) return;
    camBusy=true;
    var img=el('cam');
    var empty=el('camEmpty');
    var done=false;
    var guard=setTimeout(function(){
      if(done) return;
      done=true;
      camBusy=false;
      camFail++;
      if(camFail>2){img.style.display='none';empty.style.display='block';}
      scheduleCam(camFail>3?1500:800);
    },1800);
    img.onload=function(){
      if(done) return;
      done=true;
      clearTimeout(guard);
      camBusy=false;
      camFail=0;
      img.style.display='block';
      empty.style.display='none';
      scheduleCam((latest.camOnline&&latest.camValid)?260:800);
    };
    img.onerror=function(){
      if(done) return;
      done=true;
      clearTimeout(guard);
      camBusy=false;
      camFail++;
      if(camFail>2){img.style.display='none';empty.style.display='block';}
      scheduleCam(camFail>3?1500:800);
    };
    img.src='/capture.jpg?_='+(++camSeq)+'_'+Date.now();
  }

  el('retryCam').onclick=function(){
    if(retryBusy) return;
    retryBusy=true;
    el('retryMsg').textContent='已排队';
    fetch('/api/camera/retry?_='+Date.now(),{cache:'no-store'})
      .then(function(r){return r.text();})
      .then(function(t){el('retryMsg').textContent=responseText(t);})
      .catch(function(e){el('retryMsg').textContent='重试错误：'+e.message;})
      .finally(function(){
        setTimeout(function(){retryBusy=false;},2500);
      });
  };

  el('rxWizardStart').onclick=function(){
    if(!latest.fcRealOnline){
      el('rxWizardResult').textContent='真实 MSP 未在线。先完成 UART3 MSP 验证并确认页面显示“真实MSP 在线”。';
      return;
    }
    rxWizard={active:true,step:0,base:null,results:[]};
    el('rxWizardResult').textContent='';
    renderRxWizard();
  };
  el('rxWizardRecord').onclick=function(){
    if(!rxWizard.active){
      el('rxWizardResult').textContent='请先点击“开始方向自检”。';
      return;
    }
    poll().then(function(){ recordRxStep(); });
  };
  el('rxWizardReset').onclick=function(){
    rxWizard={active:false,step:0,base:null,results:[]};
    renderRxWizard();
    el('rxWizardResult').textContent='已重置。';
  };

  Array.prototype.forEach.call(document.querySelectorAll('.fcSimBtn'),function(btn){
    btn.onclick=function(){
      var scenario=btn.getAttribute('data-scenario');
      el('fcSimMsg').textContent='正在切换到'+scenarioText(scenario);
      fetch('/api/fc/sim?scenario='+encodeURIComponent(scenario)+'&_='+Date.now(),{cache:'no-store'})
        .then(function(r){return r.text();})
        .then(function(t){el('fcSimMsg').textContent=responseText(t);poll();})
        .catch(function(e){el('fcSimMsg').textContent='模拟飞控错误：'+e.message;});
    };
  });

  // ── 方向控制 ──
  var dir={fwd:0,right:0,yaw:0,thr:0};
  var takeover=false;
  var dirDirty=false;

  function sendDir(){
    var q='/api/dir?fwd='+dir.fwd.toFixed(2)+'&right='+dir.right.toFixed(2)+
          '&yaw='+dir.yaw.toFixed(2)+'&thr='+dir.thr.toFixed(2)+
          '&takeover='+(takeover?1:0)+'&_='+Date.now();
    fetch(q,{cache:'no-store'})
      .then(function(r){return r.text();})
      .then(function(t){
        var s=(t||'').trim();
        el('dirMsg').textContent = s==='ok' ? '' : (s==='gated'?'真机输出已锁定（仿真运行中）':responseText(t));
      })
      .catch(function(e){el('dirMsg').textContent='方向指令错误：'+e.message;});
  }

  // 按钮: 按下给方向, 松开归零 (需持续按住才移动, 松手即悬停)
  function bindHold(btn){
    var m=btn.getAttribute('data-move');
    function apply(on){
      var v=on?1:0;
      if(m==='fwd')  dir.fwd= on? 1:0;
      if(m==='back') dir.fwd= on?-1:0;
      if(m==='right')dir.right= on? 1:0;
      if(m==='left') dir.right= on?-1:0;
      if(m==='yawr') dir.yaw= on? 1:0;
      if(m==='yawl') dir.yaw= on?-1:0;
      if(m==='up')   dir.thr= on? 1:0;
      if(m==='down') dir.thr= on?-1:0;
      btn.classList.toggle('active',on);
      sendDir();
    }
    btn.addEventListener('mousedown',function(){apply(true);});
    btn.addEventListener('mouseup',function(){apply(false);});
    btn.addEventListener('mouseleave',function(){if(btn.classList.contains('active'))apply(false);});
    btn.addEventListener('touchstart',function(e){e.preventDefault();apply(true);});
    btn.addEventListener('touchend',function(e){e.preventDefault();apply(false);});
  }
  Array.prototype.forEach.call(document.querySelectorAll('.dbtn'),bindHold);

  // 摇杆: 拖动映射 -1~+1; padMove→前后/左右, padYaw→偏航/油门
  function bindPad(padId,knobId,axisX,axisY){
    var pad=el(padId),knob=el(knobId),active=false;
    function set(cx,cy){
      var r=pad.getBoundingClientRect();
      var x=(cx-r.left)/r.width*2-1, y=(cy-r.top)/r.height*2-1;
      x=Math.max(-1,Math.min(1,x)); y=Math.max(-1,Math.min(1,y));
      knob.style.left=((x+1)/2*100)+'%'; knob.style.top=((y+1)/2*100)+'%';
      dir[axisX]=x; dir[axisY]=-y;   // 屏幕下为正, 取反使"上=前/上升"
      dirDirty=true;
    }
    function reset(){
      knob.style.left='50%';knob.style.top='50%';
      dir[axisX]=0;dir[axisY]=0;dirDirty=true;
    }
    function pt(e){return e.touches?e.touches[0]:e;}
    pad.addEventListener('mousedown',function(e){active=true;set(e.clientX,e.clientY);});
    window.addEventListener('mousemove',function(e){if(active)set(e.clientX,e.clientY);});
    window.addEventListener('mouseup',function(){if(active){active=false;reset();}});
    pad.addEventListener('touchstart',function(e){e.preventDefault();active=true;var p=pt(e);set(p.clientX,p.clientY);});
    pad.addEventListener('touchmove',function(e){e.preventDefault();if(active){var p=pt(e);set(p.clientX,p.clientY);}});
    pad.addEventListener('touchend',function(e){e.preventDefault();active=false;reset();});
  }
  bindPad('padMove','knobMove','right','fwd');
  bindPad('padYaw','knobYaw','yaw','thr');

  // 摇杆节流发送 (~10Hz), 避免请求过密
  setInterval(function(){if(dirDirty){dirDirty=false;sendDir();}},100);
  // 心跳: 持续告知在线, 否则固件端 500ms 超时自动归中 (失控兜底)
  setInterval(function(){sendDir();},300);

  el('stopBtn').onclick=function(){
    dir={fwd:0,right:0,yaw:0,thr:0};
    el('knobMove').style.left='50%';el('knobMove').style.top='50%';
    el('knobYaw').style.left='50%';el('knobYaw').style.top='50%';
    Array.prototype.forEach.call(document.querySelectorAll('.dbtn'),function(b){b.classList.remove('active');});
    sendDir();
  };
  el('takeoverBtn').onclick=function(){
    takeover=!takeover;
    el('takeoverBtn').classList.toggle('active',takeover);
    el('takeoverBtn').textContent=takeover?'网页已暂停（点此恢复）':'暂停网页控制';
    el('dirState').textContent=takeover?'网页暂停，交还遥控':'中位保持';
    sendDir();
  };

  poll();
  setInterval(poll,1000);
  refreshCam();
})();
</script>
</body>
</html>
)rawliteral";

#endif
