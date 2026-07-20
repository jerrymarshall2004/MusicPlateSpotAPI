// ============================================================
//  HTTP server: serves the web UI from LittleFS and exposes the
//  REST API the app calls. Works in both AP (setup) and STA modes.
// ============================================================
#pragma once
#include <Arduino.h>
#include "SpotifyClient.h"

// A one-shot NFC write requested from the browser and carried out by
// the main loop the next time a tag is placed on the reader.
struct WriteJob {
  enum State { Idle, Waiting, Done, Error };
  volatile State state = Idle;
  String albumId;
  String message;
};

class WebPortal {
public:
  void begin(SpotifyClient* spotify);

  // Polled by loop(): true when a Wi-Fi change asks for a reboot into STA.
  bool rebootRequested() const { return _reboot; }

  // Shared NFC write job (main loop reads/writes this).
  WriteJob& writeJob() { return _job; }

private:
  SpotifyClient* _spotify = nullptr;
  bool _reboot = false;
  WriteJob _job;

  void routes();
};
