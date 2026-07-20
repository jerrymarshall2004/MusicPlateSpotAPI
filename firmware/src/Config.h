// ============================================================
//  Persistent device configuration (stored in NVS via Preferences).
//  This is the exact set of fields the setup web app collects.
// ============================================================
#pragma once
#include <Arduino.h>

struct AppConfig {
  String playerName;            // e.g. "Living Room"
  String wifiSsid;
  String wifiPass;
  String spotifyRefreshToken;   // long-lived; ESP32 refreshes access tokens itself
  String deviceMode = "active"; // "active" (current device) or "fixed"
  String fixedDeviceId;         // Spotify device id when deviceMode == "fixed"

  bool hasWifi()    const { return wifiSsid.length() > 0; }
  bool hasSpotify() const { return spotifyRefreshToken.length() > 0; }
};

namespace ConfigStore {
  void       begin();        // load from NVS
  AppConfig& get();          // mutable reference to the live config
  void       save();         // persist current values to NVS
  void       factoryReset(); // wipe everything (e.g. long-press a button)
}
