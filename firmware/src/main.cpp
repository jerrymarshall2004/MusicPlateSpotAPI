// ============================================================
//  Music Plate — ESP32 firmware entry point
//
//  Boot logic:
//   * No Wi-Fi saved  -> start SoftAP "MusicPlate-XXXX" + captive portal.
//                        Phone connects, opens the setup app, enters creds.
//   * Wi-Fi saved     -> join the network, advertise musicplate.local,
//                        then sit in the read loop: tag on reader -> play.
// ============================================================
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <DNSServer.h>

#include "secrets.h"
#include "Config.h"
#include "NfcReader.h"
#include "SpotifyClient.h"
#include "WebPortal.h"

NfcReader     nfc;
SpotifyClient spotify;
WebPortal     portal;
DNSServer     dnsServer;

bool   apMode = false;
String lastUid;                 // debounce: only act when a NEW tag appears
uint32_t lastPoll = 0;

// ---- Derive a stable-ish AP name from the MAC ----
static String apName() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[20];
  snprintf(buf, sizeof(buf), "MusicPlate-%02X%02X", mac[4], mac[5]);
  return String(buf);
}

static void startApMode() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName().c_str());
  Serial.println("[wifi] SoftAP: " + apName() + " @ " + WiFi.softAPIP().toString());

  // Captive portal: resolve every hostname to us so the setup page pops up.
  dnsServer.start(53, "*", WiFi.softAPIP());
}

static bool startStaMode() {
  AppConfig& cfg = ConfigStore::get();
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MP_HOSTNAME);
  WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
  Serial.print("[wifi] joining " + cfg.wifiSsid);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[wifi] connect failed — falling back to setup AP");
    return false;
  }
  Serial.println("[wifi] online @ " + WiFi.localIP().toString());

  if (MDNS.begin(MP_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[mdns] http://%s.local\n", MP_HOSTNAME);
  }
  return true;
}

// ---- Handle a tag that just appeared on the reader ----
static void onTag(const String& uid) {
  WriteJob& job = portal.writeJob();

  // A write was armed from the app -> program this tag instead of playing.
  if (job.state == WriteJob::Waiting) {
    if (nfc.writeAlbumId(job.albumId)) {
      job.message = "written";
      job.state   = WriteJob::Done;
      Serial.println("[nfc] wrote album " + job.albumId);
    } else {
      job.message = "write failed — hold the plate steady";
      job.state   = WriteJob::Error;
    }
    return;
  }

  // Otherwise: read the album id and start playback.
  String albumId;
  if (!nfc.readAlbumId(albumId)) {
    Serial.println("[nfc] tag has no Music Plate data");
    return;
  }
  Serial.println("[nfc] plate -> album " + albumId);

  String err;
  if (!spotify.playAlbum(albumId, err)) {
    Serial.println("[spotify] play failed: " + err);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Music Plate ===");

  if (!LittleFS.begin(true)) Serial.println("[fs] LittleFS mount failed");
  ConfigStore::begin();

  if (!nfc.begin()) Serial.println("[nfc] reader init failed (continuing)");

  AppConfig& cfg = ConfigStore::get();
  bool online = false;
  if (cfg.hasWifi()) online = startStaMode();
  if (!online) startApMode();

  portal.begin(&spotify);
}

void loop() {
  // Reboot after the app saves new Wi-Fi credentials.
  if (portal.rebootRequested()) {
    delay(300);           // let the HTTP response flush
    ESP.restart();
  }

  if (apMode) dnsServer.processNextRequest();

  // Poll the NFC reader a few times a second.
  if (millis() - lastPoll > 250) {
    lastPoll = millis();
    String uid;
    if (nfc.tagPresent(uid)) {
      if (uid != lastUid) {         // new tag
        lastUid = uid;
        onTag(uid);
      }
    } else {
      lastUid = "";                 // tag removed -> allow re-trigger
    }
  }
}
