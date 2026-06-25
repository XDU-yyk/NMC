# Web MVP HTTP debug findings

## Current implementation

- `src/web_main.cpp` starts `webServer.begin("NMC-SmartUmbrella", "12345678", true)`.
- `src/web/server.cpp` calls `WiFi.softAP(ssid, password)` but does not explicitly set `WIFI_AP` mode, AP IP, gateway, subnet, channel, or max clients.
- The HTTP server has routes for `/`, `/generate_204`, `/hotspot-detect.html`, and `onNotFound`.
- There is no per-request serial logging, so it is hard to know whether the phone browser reaches the ESP32.
- There is no simple text endpoint like `/ping` to separate browser HTML behavior from raw HTTP reachability.

## Likely failure modes

- The phone may connect to the SSID but not receive the expected AP network/DHCP route.
- ESP32 may be in a stale WiFi mode from previous firmware state unless `WiFi.mode(WIFI_AP)` and `WiFi.softAPConfig()` are set explicitly.
- Android captive portal probes can behave oddly if `/generate_204` returns 204 while the user expects a local no-internet AP. Returning the panel or redirecting locally is easier for MVP debugging.
- Without serial request logs, "browser keeps loading" cannot be distinguished from no request, wrong route, or slow response.

## Follow-up after Android still loads forever

- If Android still cannot load after AP config and request logging, the next useful split is to bypass Arduino `WebServer`, WebSocket, and DNS entirely.
- A raw `WiFiServer` target can answer fixed `HTTP/1.0` text responses with explicit `Content-Length` and `Connection: close`.
- If raw HTTP works, the bug is in the normal Web MVP stack. If raw HTTP also fails, the likely cause is phone routing/no-internet behavior, ESP32 AP compatibility, upload target mismatch, or board/flash configuration rather than frontend code.

## Raw HTTP result

- User confirmed `http://192.168.4.1/ping` displays `pong` on Android with the `esp32-s3-raw-http` firmware.
- Therefore ESP32 AP mode, Android routing to `192.168.4.1`, and basic TCP/HTTP responses are working.
- The remaining failure is likely in the normal Web MVP's Arduino `WebServer`/DNS/WebSocket stack or its response behavior, not in WiFi AP reachability.

## Follow-up after WiFiServer port still loads forever

- If the formal Web MVP still hangs after moving HTTP to `WiFiServer`, remaining differences from raw HTTP include the ArduinoWebsockets server, WebSocket polling/listening, and the HTML payload.
- The next diagnostic step is an HTTP-only Web MVP build flag: keep the normal AP/HTTP entry but do not start WebSocket and serve a tiny HTML page.

## HTTP-only MVP result

- User confirmed the formal `esp32-s3-web-mvp` target now opens the HTTP-only page on Android.
- This validates the corrected build flag placement and the WiFiServer-based HTTP path.
- The next safe feature step is HTTP polling telemetry via `/api/telemetry`; keep WebSocket disabled until the polling panel works reliably.
