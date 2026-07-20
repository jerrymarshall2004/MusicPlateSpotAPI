// ============================================================
//  PN532 NFC reader/writer.
//
//  Tags: NTAG213/215/216 (NFC Forum Type 2).
//  We store the 22-char Spotify album ID in a tiny custom format
//  in the tag's user memory (pages 4+), no NDEF parsing needed:
//
//    page 4:  'M' 'P' 0x01  <len>      (magic, version, id length)
//    page 5+: raw ASCII of the album id
//
//  22 chars -> 6 data pages -> fits NTAG213's 144 bytes easily.
// ============================================================
#pragma once
#include <Arduino.h>

class NfcReader {
public:
  bool begin();                       // init PN532 over I2C; false if not found

  // True while a tag is on the reader. Fills `uid` with a hex string.
  bool tagPresent(String& uidHex);

  // Read the stored album id from the current tag. Empty on failure/blank tag.
  bool readAlbumId(String& albumIdOut);

  // Write an album id to the current tag. Returns false on any write error.
  bool writeAlbumId(const String& albumId);

private:
  bool readPage(uint8_t page, uint8_t* buf4);
  bool writePage(uint8_t page, const uint8_t* buf4);
};
