// ============================================================
//  Copy this file to  src/secrets.h  and fill in your values.
//  secrets.h is git-ignored so your keys never get committed.
// ============================================================
#pragma once

// From https://developer.spotify.com/dashboard  -> your app -> Client ID.
// This is a PUBLIC client using PKCE, so there is NO client secret on device.
#define SPOTIFY_CLIENT_ID   "your_spotify_client_id_here"

// The HTTPS redirect page you host (see /oauth-helper/index.html in this repo,
// deployed to GitHub Pages or similar). Register this EXACT string as a
// Redirect URI in the Spotify dashboard.
#define SPOTIFY_REDIRECT_URI "https://YOURNAME.github.io/musicplate-oauth/"

// mDNS hostname -> the app will live at http://<this>.local
#define MP_HOSTNAME "musicplate"
