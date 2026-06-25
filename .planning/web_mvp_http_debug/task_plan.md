# Web MVP HTTP debug task plan

Goal: Fix the ESP32-S3 Web MVP case where a phone can connect to the AP but `http://192.168.4.1` keeps loading.

## Phases

| Phase | Status | Notes |
| --- | --- | --- |
| Inspect current Web MVP implementation | complete | Reviewed `platformio.ini`, `src/web_main.cpp`, `src/web/server.*`, and `src/web/index_html.h`. |
| Identify likely AP/HTTP failure points | complete | Focused on AP IP/DHCP setup, WiFi mode, HTTP diagnostics, captive portal endpoints, and request visibility. |
| Apply minimal diagnostic fix | complete | Kept hardware modules untouched. |
| Verify build if PlatformIO is available | complete | Both `esp32-s3-web-mvp` and `esp32-s3-devkitc-1` compile successfully. |
| Add raw HTTP diagnostic target | complete | Created and compiled a WiFiServer-only firmware to isolate Android/AP routing from WebServer/WebSocket issues. |
| Port Web MVP HTTP path to WiFiServer | complete | Raw HTTP worked on Android, so replaced WebServer/DNS HTTP handling with explicit WiFiServer responses while keeping WebServerManager API. |
| Disable WebSocket for HTTP-only MVP test | complete | Formal Web MVP still loaded forever, so removed remaining WebSocket listener/polling from the MVP target and served a tiny page. |
| Add HTTP polling telemetry panel | complete | HTTP-only page opens on Android; added `/api/telemetry` and a small polling dashboard without WebSocket. |

## Errors Encountered

| Error | Attempt | Resolution |
| --- | --- | --- |
| `pio` and `platformio` not found in PATH | `Get-Command` | Need either bundled PlatformIO path or report that local compile verification could not run. |
| PlatformIO cache lock denied in sandbox | First compile attempt | Re-ran with escalated permission because PlatformIO needs `C:\Users\yyk\.platformio`. |
| Serial port enumeration denied | `Get-CimInstance Win32_SerialPort` | Cannot identify upload port from this environment; user can upload from PlatformIO or provide port. |
