/**
 * @file    index_html.h
 * @brief   极简测试页 — 验证 HTTP 服务是否正常
 */

#ifndef INDEX_HTML_H
#define INDEX_HTML_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>NMC Web MVP</title>
<style>
body{font-family:sans-serif;background:#0a0e17;color:#e2e8f0;padding:20px;text-align:center;}
h1{color:#3b82f6;}
.status{display:inline-block;width:12px;height:12px;border-radius:50%;background:#10b981;margin-right:8px;}
.card{background:#111827;border:1px solid #1e293b;border-radius:12px;padding:16px;margin:16px auto;max-width:400px;}
a{color:#3b82f6;}
</style>
</head>
<body>
<h1><span class="status"></span>NMC 智能无人机伞</h1>
<p>Web MVP · ESP32-S3</p>
<div class="card">
<p>✅ HTTP 服务器工作正常</p>
<p>IP: 192.168.4.1 · 端口: 80</p>
<p>WebSocket: ws://192.168.4.1:81</p>
</div>
<div class="card">
<p>📡 模拟数据每 200ms 推送</p>
<p>🔧 参数调节 · 控制按钮</p>
<p>📋 服务器日志</p>
</div>
<p style="color:#94a3b8;font-size:0.85rem;">NMC Smart Umbrella — Embedded Competition</p>
</body>
</html>
)rawliteral";

#endif // INDEX_HTML_H
