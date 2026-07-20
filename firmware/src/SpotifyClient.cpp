#include "SpotifyClient.h"
#include "Config.h"
#include "secrets.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>

// ---- Spotify endpoints ----
static const char* TOKEN_URL   = "https://accounts.spotify.com/api/token";
static const char* AUTH_URL    = "https://accounts.spotify.com/authorize";
static const char* API_DEVICES = "https://api.spotify.com/v1/me/player/devices";
static const char* API_PLAY    = "https://api.spotify.com/v1/me/player/play";

// Scopes needed to read devices and control playback.
static const char* SCOPES = "user-read-playback-state user-modify-playback-state";

// ------------------------------------------------------------
//  Small helpers: PKCE (random verifier, SHA-256 -> base64url)
// ------------------------------------------------------------
static String randomVerifier(size_t n = 64) {
  static const char* charset =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
  String s;
  s.reserve(n);
  for (size_t i = 0; i < n; i++) s += charset[esp_random() % 66];
  return s;
}

static String base64url(const uint8_t* data, size_t len) {
  static const char* tbl =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  String out;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t n = data[i] << 16;
    if (i + 1 < len) n |= data[i + 1] << 8;
    if (i + 2 < len) n |= data[i + 2];
    out += tbl[(n >> 18) & 63];
    out += tbl[(n >> 12) & 63];
    if (i + 1 < len) out += tbl[(n >> 6) & 63];
    if (i + 2 < len) out += tbl[n & 63];
  }
  return out;  // base64url, no padding (as PKCE requires)
}

static String pkceChallenge(const String& verifier) {
  uint8_t hash[32];
  mbedtls_sha256((const uint8_t*)verifier.c_str(), verifier.length(), hash, /*is224=*/0);
  return base64url(hash, sizeof(hash));
}

static String urlEncode(const String& s) {
  String out;
  const char* hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      out += '%';
      out += hex[(c >> 4) & 0xF];
      out += hex[c & 0xF];
    }
  }
  return out;
}

// One place to configure TLS. For a hobby build we skip cert pinning;
// for production, load Spotify's root CA with client.setCACert(...).
static void configureTls(WiFiClientSecure& client) {
  client.setInsecure();  // TODO: pin the DigiCert root CA for production
}

// ------------------------------------------------------------
//  Auth
// ------------------------------------------------------------
String SpotifyClient::beginAuth() {
  _verifier = randomVerifier();
  String challenge = pkceChallenge(_verifier);

  String url = String(AUTH_URL) +
      "?response_type=code" +
      "&client_id="     + urlEncode(SPOTIFY_CLIENT_ID) +
      "&redirect_uri="  + urlEncode(SPOTIFY_REDIRECT_URI) +
      "&scope="         + urlEncode(SCOPES) +
      "&code_challenge_method=S256" +
      "&code_challenge=" + challenge;
  return url;
}

bool SpotifyClient::exchangeCode(const String& code, String& errOut) {
  if (_verifier.isEmpty()) { errOut = "no PKCE verifier — restart auth"; return false; }

  WiFiClientSecure client;
  configureTls(client);
  HTTPClient http;
  if (!http.begin(client, TOKEN_URL)) { errOut = "http.begin failed"; return false; }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body =
      "grant_type=authorization_code"
      "&code="          + urlEncode(code) +
      "&redirect_uri="  + urlEncode(SPOTIFY_REDIRECT_URI) +
      "&client_id="     + urlEncode(SPOTIFY_CLIENT_ID) +
      "&code_verifier=" + urlEncode(_verifier);

  int status = http.POST(body);
  String resp = http.getString();
  http.end();

  if (status != 200) { errOut = "token exchange " + String(status) + ": " + resp; return false; }

  JsonDocument doc;
  if (deserializeJson(doc, resp)) { errOut = "bad token JSON"; return false; }

  _accessToken     = doc["access_token"].as<String>();
  _accessExpiresAt = millis() + (uint32_t)doc["expires_in"].as<uint32_t>() * 1000UL - 30000UL;

  String rt = doc["refresh_token"].as<String>();
  if (rt.length()) {
    ConfigStore::get().spotifyRefreshToken = rt;
    ConfigStore::save();
  }
  _verifier = "";  // consumed
  return true;
}

bool SpotifyClient::refreshAccessToken(String& errOut) {
  AppConfig& cfg = ConfigStore::get();
  if (!cfg.hasSpotify()) { errOut = "not linked to Spotify"; return false; }

  WiFiClientSecure client;
  configureTls(client);
  HTTPClient http;
  if (!http.begin(client, TOKEN_URL)) { errOut = "http.begin failed"; return false; }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body =
      "grant_type=refresh_token"
      "&refresh_token=" + urlEncode(cfg.spotifyRefreshToken) +
      "&client_id="     + urlEncode(SPOTIFY_CLIENT_ID);

  int status = http.POST(body);
  String resp = http.getString();
  http.end();

  if (status != 200) { errOut = "refresh " + String(status) + ": " + resp; return false; }

  JsonDocument doc;
  if (deserializeJson(doc, resp)) { errOut = "bad refresh JSON"; return false; }

  _accessToken     = doc["access_token"].as<String>();
  _accessExpiresAt = millis() + (uint32_t)doc["expires_in"].as<uint32_t>() * 1000UL - 30000UL;

  // Spotify occasionally rotates the refresh token — persist if so.
  String rt = doc["refresh_token"].as<String>();
  if (rt.length() && rt != cfg.spotifyRefreshToken) {
    cfg.spotifyRefreshToken = rt;
    ConfigStore::save();
  }
  return true;
}

bool SpotifyClient::ensureAccessToken() {
  if (_accessToken.length() && (int32_t)(_accessExpiresAt - millis()) > 0) return true;
  String err;
  if (!refreshAccessToken(err)) { Serial.println("[spotify] " + err); return false; }
  return true;
}

// ------------------------------------------------------------
//  Playback
// ------------------------------------------------------------
bool SpotifyClient::playAlbum(const String& albumId, String& errOut) {
  if (!ensureAccessToken()) { errOut = "no access token"; return false; }
  AppConfig& cfg = ConfigStore::get();

  String url = API_PLAY;
  if (cfg.deviceMode == "fixed" && cfg.fixedDeviceId.length()) {
    url += "?device_id=" + urlEncode(cfg.fixedDeviceId);  // also transfers playback
  }

  WiFiClientSecure client;
  configureTls(client);
  HTTPClient http;
  if (!http.begin(client, url)) { errOut = "http.begin failed"; return false; }
  http.addHeader("Authorization", "Bearer " + _accessToken);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"context_uri\":\"spotify:album:" + albumId + "\"}";
  int status = http.sendRequest("PUT", (uint8_t*)payload.c_str(), payload.length());
  String resp = http.getString();
  http.end();

  // 204 = success. 404 usually means "no active device".
  if (status == 204 || status == 202) return true;
  errOut = "play " + String(status) + ": " + resp;
  return false;
}

bool SpotifyClient::getDevicesJson(String& out) {
  if (!ensureAccessToken()) return false;

  WiFiClientSecure client;
  configureTls(client);
  HTTPClient http;
  if (!http.begin(client, API_DEVICES)) return false;
  http.addHeader("Authorization", "Bearer " + _accessToken);

  int status = http.GET();
  out = http.getString();
  http.end();
  return status == 200;
}
