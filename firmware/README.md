# Music Plate — ESP32 Firmware

Turns an ESP32 + PN532 NFC reader into a "digital vinyl" player: place a plate
(NFC tag) on the reader and it starts the album on your Spotify Connect device.
The reader hosts its own web app at **http://musicplate.local** — no cloud
backend, no database. Plates store only a Spotify album ID, so any plate works
on any reader.

## Hardware

| Part            | Notes                                                        |
|-----------------|-------------------------------------------------------------|
| ESP32 dev board | Classic ESP32 (4MB flash) works. **ESP32-S3 recommended** — more RAM for TLS, native USB. |
| PN532 module    | The common red "NFC Module V3" board. Set it to **I2C** mode via its DIP switches. |
| NFC tags        | NTAG213/215/216 (stickers or discs). ~144+ bytes is plenty. |

### PN532 → ESP32 wiring (I2C)

| PN532 | ESP32   |
|-------|---------|
| VCC   | 3V3     |
| GND   | GND     |
| SDA   | GPIO 21 |
| SCL   | GPIO 22 |

(Change pins in `NfcReader.cpp` if you wire it differently. The board's two DIP
switches must be set to I2C — usually `1=ON, 2=OFF`.)

## First-time setup

1. **Create a Spotify app** at <https://developer.spotify.com/dashboard>.
   Note the **Client ID** (this is a public PKCE client — there is no secret on
   the device).
2. **Deploy the OAuth helper.** Push `oauth-helper/index.html` to GitHub Pages
   (or any static HTTPS host). Its published URL is your **redirect URI**.
   - Add that exact URL to your Spotify app's **Redirect URIs**.
   - See the note below on *why* this indirection is needed.
3. **Configure secrets.** Copy `src/secrets.example.h` to `src/secrets.h` and
   fill in `SPOTIFY_CLIENT_ID` and `SPOTIFY_REDIRECT_URI`.
4. **Build & flash** (see below).
5. Power the reader. First boot has no Wi-Fi, so it starts a hotspot
   **`MusicPlate-XXXX`**. Join it with your phone; the setup app opens
   (captive portal). Enter Wi-Fi → the reader reboots onto your network.
6. Rejoin your home Wi-Fi, open **http://musicplate.local**, and finish setup:
   link Spotify, pick the playback device, and write your first plate.

## Build & flash (PlatformIO)

```bash
# firmware code
pio run -t upload

# web UI -> LittleFS (put gzipped index.html.gz / style.css.gz / app.js.gz in data/)
pio run -t uploadfs

# serial monitor
pio device monitor
```

VS Code: install the **PlatformIO IDE** extension, open the `firmware/` folder,
and use the checkmark (build) / arrow (upload) / plug (monitor) buttons.

## Why the OAuth helper page?

Spotify requires OAuth **redirect URIs to be HTTPS**, with the only exception
being loopback literal IPs (`127.0.0.1`). The reader lives at
`http://musicplate.local`, which Spotify rejects. The fix:

```
Browser → Spotify login → HTTPS helper page (GitHub Pages)
        → plain redirect → http://musicplate.local/spotify/callback?code=…
        → ESP32 exchanges the code for a refresh token (outbound TLS)
```

The helper is a static page that just forwards the `?code=` to the reader on
your LAN. Because it's a browser *navigation* (not a fetch), there's no CORS
issue, and the authorization code never leaves your devices except to Spotify.

The reader stores the **refresh token** and mints short-lived access tokens
itself, so the link is effectively permanent.

## REST API (what the web app calls)

| Method | Path                     | Purpose                                   |
|--------|--------------------------|-------------------------------------------|
| GET    | `/api/status`            | player name, mode, Spotify-linked, device |
| POST   | `/api/name`              | set player name (`name`)                   |
| GET    | `/api/wifi/scan`         | list nearby networks                       |
| POST   | `/api/wifi`              | save creds (`ssid`,`pass`), reboot to STA  |
| GET    | `/api/spotify/login`     | returns `{url}` to open for authorization  |
| GET    | `/spotify/callback`      | OAuth landing (from helper) → stores token |
| GET    | `/api/spotify/devices`   | list Spotify Connect devices               |
| POST   | `/api/config/device`     | set `mode` (`active`/`fixed`) + `deviceId` |
| POST   | `/api/write`             | arm an NFC write (`albumId`)               |
| GET    | `/api/write/status`      | poll write state (`waiting`/`done`/`error`)|

POST bodies are `application/x-www-form-urlencoded`.

## Wiring the mockup app to this firmware

The mockup's `app.js` has a `MockAPI` layer that stands in 1:1 for these
endpoints. To go live, replace those mock methods with `fetch()` calls:

```js
// before (mock)
async scanWifi() { await delay(1800); return [ /* fake */ ]; }

// after (real)
async scanWifi() {
  const r = await fetch("/api/wifi/scan");
  return r.json();
}
```

Since the app is served from the reader itself, all `fetch("/api/…")` calls are
same-origin — no base URL, no CORS. Then gzip the three files into `data/` and
`pio run -t uploadfs`.

## Notes & TODO

- **TLS:** `SpotifyClient` uses `client.setInsecure()` for simplicity. For
  production, pin Spotify's root CA with `client.setCACert(...)`.
- **No active device:** if `deviceMode` is `active` and nothing is currently
  playing, Spotify returns 404. Consider falling back to a saved device id.
- **Factory reset:** call `ConfigStore::factoryReset()` (wire it to a
  long-press on the BOOT button) to re-provision.
- **mDNS `.local`:** works natively on iOS/macOS and Android 12+. Windows needs
  Bonjour. As a fallback, the reader's IP is printed to serial and shown in the
  app's status.
