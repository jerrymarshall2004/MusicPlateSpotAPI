// ============================================================
//  Standalone NFC read/write test  (build env: nfctest)
//
//  Purpose: prove out the PN532 wiring and the tag format with
//  nothing else in the way — no Wi-Fi, Spotify, or secrets.h.
//  Uses the SAME NfcReader as the real firmware, so tags written
//  here read back identically once the full firmware is flashed.
//
//  Build & run:
//    pio run -e nfctest -t upload
//    pio device monitor        (115200 baud)
//
//  How to use it (over the serial monitor):
//    * Place a tag on the reader  -> it prints the album id stored on it.
//    * Type an album id + Enter    -> the NEXT tag you place gets that id
//                                     written to it, then verified.
// ============================================================
#include <Arduino.h>
#include "NfcReader.h"

NfcReader nfc;
String    pendingWrite = "";
String    lastUid      = "";
bool      readerOk     = false;

static void printHelp() {
  Serial.println();
  Serial.println("=========================================");
  Serial.println("  Music Plate - NFC read/write test");
  Serial.println("=========================================");
  Serial.println("READ : place a tag on the reader.");
  Serial.println("WRITE: type an album id + Enter, then place a tag.");
  Serial.println();
  Serial.println("Sample ids to try:");
  Serial.println("  4LH4d3cOWNNsVw41Gqt2kv   Pink Floyd - Dark Side of the Moon");
  Serial.println("  78bpIziExqiI9qztvNFlQu   Arctic Monkeys - AM");
  Serial.println("-----------------------------------------");
}

void setup() {
  Serial.begin(115200);
  delay(400);

  readerOk = nfc.begin();
  if (!readerOk) {
    Serial.println();
    Serial.println("[nfc] PN532 NOT found. Check:");
    Serial.println("      - SDA->GPIO21, SCL->GPIO22, VCC->3V3, GND->GND");
    Serial.println("      - module DIP switches set to I2C (usually 1=ON, 2=OFF)");
    Serial.println("      - solid 3.3V power to the module");
    Serial.println("      Fix wiring and press the board's RESET button.");
    return;
  }
  printHelp();
}

void loop() {
  if (!readerOk) { delay(1000); return; }

  // Typed line -> arm a write for the next tag.
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length()) {
      pendingWrite = line;
      Serial.println("[armed] will WRITE \"" + pendingWrite + "\" to the next tag. Place it now.");
    }
  }

  String uid;
  if (nfc.tagPresent(uid)) {
    if (uid != lastUid) {              // only act once per tag placement
      lastUid = uid;
      Serial.println();
      Serial.println("[tag] detected, UID " + uid);

      if (pendingWrite.length()) {
        // ---- WRITE path ----
        if (nfc.writeAlbumId(pendingWrite)) {
          Serial.println("[write] OK -> " + pendingWrite);
          String back;
          if (nfc.readAlbumId(back)) {
            Serial.println("[verify] read back -> " + back +
                           (back == pendingWrite ? "   MATCH" : "   *** MISMATCH ***"));
          } else {
            Serial.println("[verify] could not read the tag back");
          }
        } else {
          Serial.println("[write] FAILED - hold the tag flat and steady on the reader, try again");
        }
        pendingWrite = "";
      } else {
        // ---- READ path ----
        String albumId;
        if (nfc.readAlbumId(albumId)) {
          Serial.println("[read] album id -> " + albumId);
        } else {
          Serial.println("[read] blank tag or not a Music Plate tag (type an id to write one)");
        }
      }
    }
  } else {
    lastUid = "";                      // tag removed -> allow re-trigger
  }

  delay(150);
}
