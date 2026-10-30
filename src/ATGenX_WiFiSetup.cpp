/**
 * @file    ATGenX_WiFiSetup.cpp
 * @brief   ATGenX_WiFiSetup implementation (ESP32 and ESP8266)
 * @version 2.6.0
 */

#include "ATGenX_WiFiSetup.h"

#include <DNSServer.h>
#include <ArduinoJson.h>

#if defined(ESP32)
#   include <WiFi.h>
#   include <WebServer.h>
#   include <Preferences.h>
using SetupServer = WebServer;
#elif defined(ESP8266)
#   include <ESP8266WiFi.h>
#   include <ESP8266WebServer.h>
#   include <EEPROM.h>
using SetupServer = ESP8266WebServer;
#endif

#ifndef ATGENX_HUB_VERSION
#   define ATGENX_HUB_VERSION "2.7.0"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// The saved network
// ═══════════════════════════════════════════════════════════════════════════

#if defined(ESP8266)
// EEPROM: "ATGX" + 33 bytes of name + 65 of password
static constexpr size_t EE_SIZE = 4 + 33 + 65;
#endif

bool ATGenX_WiFiSetup::load(String& ssid, String& pass) {
#if defined(ESP32)
    Preferences p;
    if (!p.begin("atgx_wifi", true)) return false;
    ssid = p.getString("ssid", "");
    pass = p.getString("pass", "");
    p.end();
#elif defined(ESP8266)
    EEPROM.begin(EE_SIZE);
    const bool ok = EEPROM.read(0) == 'A' && EEPROM.read(1) == 'T' && EEPROM.read(2) == 'G' && EEPROM.read(3) == 'X';
    char s[33] = {0}, w[65] = {0};
    if (ok) {
        for (size_t i = 0; i < 32; ++i) s[i] = char(EEPROM.read(4 + i));
        for (size_t i = 0; i < 64; ++i) w[i] = char(EEPROM.read(4 + 33 + i));
    }
    EEPROM.end();
    ssid = ok ? String(s) : String();
    pass = ok ? String(w) : String();
#endif
    return ssid.length() > 0;
}

void ATGenX_WiFiSetup::save(const String& ssid, const String& pass) {
#if defined(ESP32)
    Preferences p;
    p.begin("atgx_wifi", false);
    p.putString("ssid", ssid);
    p.putString("pass", pass);
    p.end();
#elif defined(ESP8266)
    EEPROM.begin(EE_SIZE);
    const char magic[] = {'A', 'T', 'G', 'X'};
    for (size_t i = 0; i < 4; ++i) EEPROM.write(i, magic[i]);
    for (size_t i = 0; i < 33; ++i) EEPROM.write(4 + i, i < ssid.length() ? ssid[i] : 0);
    for (size_t i = 0; i < 65; ++i) EEPROM.write(4 + 33 + i, i < pass.length() ? pass[i] : 0);
    EEPROM.commit();
    EEPROM.end();
#endif
}

void ATGenX_WiFiSetup::clear() {
#if defined(ESP32)
    Preferences p;
    p.begin("atgx_wifi", false);
    p.clear();
    p.end();
#elif defined(ESP8266)
    EEPROM.begin(EE_SIZE);
    for (size_t i = 0; i < EE_SIZE; ++i) EEPROM.write(i, 0);
    EEPROM.commit();
    EEPROM.end();
#endif
}

String ATGenX_WiFiSetup::apName() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char name[16];
    snprintf(name, sizeof(name), "ATGenX-%02X%02X", mac[4], mac[5]);
    return String(name);
}

// ═══════════════════════════════════════════════════════════════════════════
// Setup mode
// ═══════════════════════════════════════════════════════════════════════════

// The page for a phone without the app: the site's gold, Arabic and English, the board's networks.
static const char SETUP_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="ar" dir="rtl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>ATGenX</title><style>
body{margin:0;font-family:system-ui,Tahoma,sans-serif;background:#171511;color:#f5f1e8;display:flex;justify-content:center}
main{width:100%;max-width:420px;padding:28px 20px}h1{margin:0 0 4px;font-size:22px;color:#fabd23}p{color:#b4ac9c;margin:0 0 18px}
label{display:block;margin:14px 0 6px;font-weight:700}select,input{width:100%;box-sizing:border-box;padding:13px;border-radius:12px;border:1px solid #3a352b;background:#23201a;color:#fff;font-size:16px}
button{margin-top:22px;width:100%;padding:14px;border:0;border-radius:12px;background:#fabd23;color:#000;font-weight:800;font-size:16px}
#m{margin-top:16px;font-weight:700}</style></head><body><main>
<h1>ATGenX</h1><p>اختر شبكة بيتك واكتب كلمة السر · Choose your home Wi-Fi</p>
<label>الشبكة · Network</label><select id="s"><option>…</option></select>
<label>كلمة السر · Password</label><input id="p" type="password" autocomplete="off">
<button onclick="go()">اتصل · Connect</button><div id="m"></div></main><script>
fetch('/scan').then(r=>r.json()).then(l=>{s.innerHTML=l.map(n=>'<option>'+n.ssid.replace(/</g,'&lt;')+'</option>').join('')});
function go(){m.textContent='…';fetch('/wifi',{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify({ssid:s.value,password:p.value})})
.then(r=>r.json()).then(j=>{m.textContent=j.ok?'تم ✓ اللوحة تتصل الآن · Done, the board is joining':(j.error||'خطأ')})}
</script></body></html>)HTML";

[[noreturn]] void ATGenX_WiFiSetup::run(const char* boardType, uint32_t retryAfterMs) {
    const String name = apName();
    Serial.println();
    Serial.println(F("──────── ATGenX setup mode ────────"));
    Serial.print(F("  Wi-Fi : ")); Serial.println(name);
    Serial.print(F("  Pass  : ")); Serial.println(F(ATGENX_SETUP_PASSWORD));
    Serial.println(F("  Open the ATGenX app → Add a device"));
    Serial.println(F("───────────────────────────────────"));

    WiFi.disconnect();
    WiFi.mode(WIFI_AP_STA);   // STA too: the board scans the networks it can see
    WiFi.softAP(name.c_str(), ATGENX_SETUP_PASSWORD);
    delay(200);

    DNSServer dns;
    dns.start(53, "*", WiFi.softAPIP());   // any address → this page (the phone's "sign in to network")
    SetupServer server(80);

    String scanJson = "[]";
    uint32_t scannedAt = 0;
    auto rescan = [&]() {
        const int n = WiFi.scanNetworks();
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < n; ++i) {
            if (WiFi.SSID(i).length() == 0) continue;
            JsonObject o = arr.add<JsonObject>();
            o["ssid"] = WiFi.SSID(i);
            o["rssi"] = WiFi.RSSI(i);
#if defined(ESP32)
            o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
#else
            o["secure"] = WiFi.encryptionType(i) != ENC_TYPE_NONE;
#endif
        }
        WiFi.scanDelete();
        scanJson = "";
        serializeJson(doc, scanJson);
        scannedAt = millis();
    };
    rescan();

    uint32_t restartAt = 0;
    server.on("/info", HTTP_GET, [&]() {
        JsonDocument doc;
        doc["device"] = name;
        doc["board"] = boardType;
        doc["fw"] = ATGENX_HUB_VERSION;
        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    });
    server.on("/scan", HTTP_GET, [&]() {
        if (millis() - scannedAt > 15000) rescan();
        server.send(200, "application/json", scanJson);
    });
    server.on("/wifi", HTTP_POST, [&]() {
        String ssid, pass;
        JsonDocument doc;
        if (server.hasArg("plain") && !deserializeJson(doc, server.arg("plain"))) {
            ssid = doc["ssid"] | "";
            pass = doc["password"] | "";
        } else {
            ssid = server.arg("ssid");
            pass = server.arg("password");
        }
        if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 64 || (pass.length() > 0 && pass.length() < 8)) {
            server.send(400, "application/json", F("{\"ok\":false,\"error\":\"Check the network name and password (8+ characters)\"}"));
            return;
        }
        save(ssid, pass);
        Serial.print(F("[Setup] Saved \"")); Serial.print(ssid); Serial.println(F("\" — restarting to join it"));
        server.send(200, "application/json", F("{\"ok\":true}"));
        restartAt = millis() + 1200;   // let the answer reach the phone first
    });
    server.onNotFound([&]() { server.send_P(200, "text/html; charset=utf-8", SETUP_PAGE); });
    server.begin();

    if (ATGENX_SETUP_LED >= 0) pinMode(ATGENX_SETUP_LED, OUTPUT);
    const uint32_t started = millis();
    for (;;) {
        dns.processNextRequest();
        server.handleClient();
        if (ATGENX_SETUP_LED >= 0) digitalWrite(ATGENX_SETUP_LED, (millis() / 250) % 2);
        if (restartAt && (int32_t)(millis() - restartAt) > 0) ESP.restart();
        // the saved network may just have been down: try it again when nobody is setting the board up
        if (retryAfterMs && millis() - started > retryAfterMs && WiFi.softAPgetStationNum() == 0) ESP.restart();
        delay(2);
    }
}

void ATGenX_WiFiSetup::watchButton() {
#if ATGENX_SETUP_BUTTON >= 0
    static uint32_t downSince = 0;
    static bool ready = false;
    if (!ready) {
        pinMode(ATGENX_SETUP_BUTTON, INPUT_PULLUP);
        ready = true;
    }
    if (digitalRead(ATGENX_SETUP_BUTTON) == LOW) {
        if (!downSince) downSince = millis();
        else if (millis() - downSince > 5000) {
            Serial.println(F("[Setup] Button held — forgetting the network, back to setup mode"));
            clear();
            delay(100);
            ESP.restart();
        }
    } else {
        downSince = 0;
    }
#endif
}
