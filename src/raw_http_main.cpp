/**
 * @file    raw_http_main.cpp
 * @brief   ESP32-S3 AP + raw HTTP diagnostic firmware.
 *
 * This target intentionally avoids WebServer, WebSocket, DNS, sensors, and
 * frontend JS. It answers plain HTTP over WiFiServer so Android/AP routing
 * problems can be separated from the normal Web MVP stack.
 */

#include <Arduino.h>
#include <WiFi.h>

static const char* WIFI_AP_SSID = "NMC-RAW-HTTP";
static const char* WIFI_AP_PASSWORD = "12345678";

static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

static WiFiServer server(80);
static uint32_t startMs = 0;

static void sendPlain(WiFiClient& client, const char* body)
{
    client.print("HTTP/1.0 200 OK\r\n");
    client.print("Content-Type: text/plain; charset=utf-8\r\n");
    client.print("Cache-Control: no-store\r\n");
    client.print("Connection: close\r\n");
    client.print("Content-Length: ");
    client.print(strlen(body));
    client.print("\r\n\r\n");
    client.print(body);
    client.flush();
}

static void sendNotFound(WiFiClient& client)
{
    const char* body = "not found\n";
    client.print("HTTP/1.0 404 Not Found\r\n");
    client.print("Content-Type: text/plain; charset=utf-8\r\n");
    client.print("Connection: close\r\n");
    client.print("Content-Length: ");
    client.print(strlen(body));
    client.print("\r\n\r\n");
    client.print(body);
    client.flush();
}

static void handleClient(WiFiClient& client)
{
    client.setTimeout(1500);

    String requestLine = client.readStringUntil('\n');
    requestLine.trim();

    while (client.connected() && client.available())
    {
        String header = client.readStringUntil('\n');
        if (header == "\r" || header.length() == 0) break;
    }

    Serial.printf("[HTTP] %s from %s\n",
        requestLine.c_str(),
        client.remoteIP().toString().c_str());

    if (requestLine.startsWith("GET /ping "))
    {
        sendPlain(client, "pong\n");
    }
    else if (requestLine.startsWith("GET / ") ||
             requestLine.startsWith("GET /index.html ") ||
             requestLine.startsWith("GET /generate_204 ") ||
             requestLine.startsWith("GET /gen_204 ") ||
             requestLine.startsWith("GET /hotspot-detect.html ") ||
             requestLine.startsWith("GET /connecttest.txt "))
    {
        sendPlain(client,
            "NMC RAW HTTP OK\n"
            "SSID: NMC-RAW-HTTP\n"
            "IP: 192.168.4.1\n"
            "Try: http://192.168.4.1/ping\n");
    }
    else
    {
        sendNotFound(client);
    }

    delay(10);
    client.stop();
}

void setup()
{
    Serial.begin(115200);
    delay(800);

    Serial.println();
    Serial.println("==============================================");
    Serial.println("  NMC RAW HTTP DIAGNOSTIC");
    Serial.println("  WiFi.h + WiFiServer only");
    Serial.println("==============================================");

    WiFi.persistent(false);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(300);

    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);

    if (!WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET))
    {
        Serial.println("[FATAL] softAPConfig failed");
        while (true) delay(1000);
    }

    bool ok = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, 1, false, 4);
    if (!ok)
    {
        Serial.println("[FATAL] softAP failed");
        while (true) delay(1000);
    }

    server.begin();
    server.setNoDelay(true);
    startMs = millis();

    Serial.printf("[AP] SSID=%s password=%s\n", WIFI_AP_SSID, WIFI_AP_PASSWORD);
    Serial.printf("[AP] IP=%s gateway=%s subnet=%s channel=1\n",
        WiFi.softAPIP().toString().c_str(),
        AP_GATEWAY.toString().c_str(),
        AP_SUBNET.toString().c_str());
    Serial.println("[HTTP] Open http://192.168.4.1/ping");
}

void loop()
{
    WiFiClient client = server.available();
    if (client)
    {
        handleClient(client);
    }

    static uint32_t lastPrint = 0;
    uint32_t now = millis();
    if (now - lastPrint >= 5000)
    {
        lastPrint = now;
        Serial.printf("[SYS] uptime=%lus heap=%u stations=%u ip=%s\n",
            (now - startMs) / 1000,
            ESP.getFreeHeap(),
            WiFi.softAPgetStationNum(),
            WiFi.softAPIP().toString().c_str());
    }

    delay(5);
}
