// ============================================================
//  Spotify Web API client for ESP32.
//
//  Auth model: Authorization Code + PKCE (public client, no secret).
//   1. beginAuth()  -> generates a PKCE verifier/challenge, returns the
//                      full Spotify authorize URL for the browser to open.
//   2. exchangeCode() (called when the browser returns with ?code=...)
//                   -> swaps the code for access + refresh tokens.
//   3. The refresh token is stored in Config and used forever after;
//      access tokens are short-lived and refreshed on demand.
// ============================================================
#pragma once
#include <Arduino.h>

struct SpotifyDevice {
  String id;
  String name;
  String type;      // "Speaker", "Computer", "Smartphone", ...
  bool   active;
};

class SpotifyClient {
public:
  // Build the authorize URL and stash the PKCE verifier for the next step.
  String beginAuth();

  // Exchange the authorization code (from the redirect) for tokens.
  // On success, stores the refresh token into ConfigStore and saves.
  bool exchangeCode(const String& code, String& errOut);

  // Ensure we have a valid access token (refreshing if needed).
  bool ensureAccessToken();

  // Start playback of an album on the configured device.
  // deviceMode/fixedDeviceId come from ConfigStore.
  bool playAlbum(const String& albumId, String& errOut);

  // List the account's available Spotify Connect devices (JSON to `out`).
  bool getDevicesJson(String& out);

private:
  String  _verifier;              // current PKCE verifier (valid during setup)
  String  _accessToken;
  uint32_t _accessExpiresAt = 0;  // millis() deadline

  bool refreshAccessToken(String& errOut);
};
