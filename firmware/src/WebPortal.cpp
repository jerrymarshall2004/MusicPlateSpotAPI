#include "WebPortal.h"
#include "Config.h"

#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

static AsyncWebServer server(80);

// Helper: read a POST form field (application/x-www-form-urlencoded).
static String formArg(AsyncWebServerRequest* req, const char* name) {
  if (req->hasParam(name, /*post=*/true)) return req->getParam(name, true)->value();
  return String();
}

void WebPortal::begin(SpotifyClient* spotify) {
  _spotify = spotify;
  routes();
  server.begin();
  Serial.println("[web] server started on :80");
}

void WebPortal::routes() {
  // ---- Static web UI (gzipped files live in /data -> LittleFS) ----
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // ---- Status ----
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    AppConfig& cfg = ConfigStore::get();
    JsonDocument doc;
    doc["name"]          = cfg.playerName;
    doc["wifi"]          = cfg.wifiSsid;
    doc["mode"]          = WiFi.getMode() == WIFI_AP ? "setup" : "online";
    doc["ip"]            = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString()
                                                     : WiFi.localIP().toString();
    doc["spotifyLinked"] = cfg.hasSpotify();
    doc["deviceMode"]    = cfg.deviceMode;
    doc["fixedDeviceId"] = cfg.fixedDeviceId;
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  // ---- Player name ----
  server.on("/api/name", HTTP_POST, [](AsyncWebServerRequest* req) {
    ConfigStore::get().playerName = formArg(req, "name");
    ConfigStore::save();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // ---- Wi-Fi scan ----
  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest* req) {
    int n = WiFi.scanNetworks();
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < n; i++) {
      JsonObject o = arr.add<JsonObject>();
      o["ssid"]     = WiFi.SSID(i);
      o["rssi"]     = WiFi.RSSI(i);
      o["secured"]  = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  // ---- Save Wi-Fi and reboot into station mode ----
  server.on("/api/wifi", HTTP_POST, [this](AsyncWebServerRequest* req) {
    AppConfig& cfg = ConfigStore::get();
    cfg.wifiSsid = formArg(req, "ssid");
    cfg.wifiPass = formArg(req, "pass");
    ConfigStore::save();
    req->send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
    _reboot = true;   // main loop reboots after the response flushes
  });

  // ---- Spotify: get the authorize URL ----
  server.on("/api/spotify/login", HTTP_GET, [this](AsyncWebServerRequest* req) {
    String url = _spotify->beginAuth();
    JsonDocument doc; doc["url"] = url;
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  // ---- Spotify: OAuth redirect lands here (via the hosted HTTPS helper) ----
  server.on("/spotify/callback", HTTP_GET, [this](AsyncWebServerRequest* req) {
    if (!req->hasParam("code")) { req->send(400, "text/plain", "missing code"); return; }
    String code = req->getParam("code")->value();
    String err;
    if (_spotify->exchangeCode(code, err)) {
      req->redirect("/#linked");
    } else {
      Serial.println("[web] spotify link failed: " + err);
      req->redirect("/#linkfailed");
    }
  });

  // ---- Spotify: list Connect devices ----
  server.on("/api/spotify/devices", HTTP_GET, [this](AsyncWebServerRequest* req) {
    String json;
    if (_spotify->getDevicesJson(json)) req->send(200, "application/json", json);
    else req->send(502, "application/json", "{\"error\":\"spotify unavailable\"}");
  });

  // ---- Save which device to play on ----
  server.on("/api/config/device", HTTP_POST, [](AsyncWebServerRequest* req) {
    AppConfig& cfg = ConfigStore::get();
    cfg.deviceMode    = formArg(req, "mode");                 // "active" | "fixed"
    cfg.fixedDeviceId = cfg.deviceMode == "fixed" ? formArg(req, "deviceId") : "";
    ConfigStore::save();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // ---- Arm an NFC write (main loop performs it on next tag) ----
  server.on("/api/write", HTTP_POST, [this](AsyncWebServerRequest* req) {
    _job.albumId = formArg(req, "albumId");
    _job.message = "";
    _job.state   = _job.albumId.length() ? WriteJob::Waiting : WriteJob::Error;
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // ---- Poll NFC write status ----
  server.on("/api/write/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
    const char* s = "idle";
    switch (_job.state) {
      case WriteJob::Waiting: s = "waiting"; break;
      case WriteJob::Done:    s = "done";    break;
      case WriteJob::Error:   s = "error";   break;
      default:                s = "idle";    break;
    }
    JsonDocument doc; doc["state"] = s; doc["message"] = _job.message;
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  // ---- Fallback: send the SPA index for unknown GETs ----
  server.onNotFound([](AsyncWebServerRequest* req) {
    if (req->method() == HTTP_GET && LittleFS.exists("/index.html")) {
      req->send(LittleFS, "/index.html", "text/html");
    } else {
      req->send(404, "text/plain", "not found");
    }
  });
}
