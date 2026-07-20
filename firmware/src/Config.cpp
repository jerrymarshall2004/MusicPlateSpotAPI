#include "Config.h"
#include <Preferences.h>

namespace {
  Preferences prefs;
  AppConfig   cfg;
  const char* NS = "musicplate";
}

void ConfigStore::begin() {
  prefs.begin(NS, /*readOnly=*/true);
  cfg.playerName          = prefs.getString("name", "Music Plate");
  cfg.wifiSsid            = prefs.getString("ssid", "");
  cfg.wifiPass            = prefs.getString("pass", "");
  cfg.spotifyRefreshToken = prefs.getString("sp_rt", "");
  cfg.deviceMode          = prefs.getString("dev_mode", "active");
  cfg.fixedDeviceId       = prefs.getString("dev_id", "");
  prefs.end();
}

AppConfig& ConfigStore::get() { return cfg; }

void ConfigStore::save() {
  prefs.begin(NS, /*readOnly=*/false);
  prefs.putString("name",  cfg.playerName);
  prefs.putString("ssid",  cfg.wifiSsid);
  prefs.putString("pass",  cfg.wifiPass);
  prefs.putString("sp_rt", cfg.spotifyRefreshToken);
  prefs.putString("dev_mode", cfg.deviceMode);
  prefs.putString("dev_id",   cfg.fixedDeviceId);
  prefs.end();
}

void ConfigStore::factoryReset() {
  prefs.begin(NS, /*readOnly=*/false);
  prefs.clear();
  prefs.end();
  cfg = AppConfig{};
}
