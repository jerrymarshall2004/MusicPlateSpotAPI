#include "NfcReader.h"
#include <Wire.h>
#include <Adafruit_PN532.h>

// PN532 over I2C. Default ESP32 I2C pins: SDA=21, SCL=22.
// IRQ/RESET are optional for I2C; -1 = unused.
static Adafruit_PN532 pn532(/*irq=*/-1, /*reset=*/-1, &Wire);

static const uint8_t MP_MAGIC0 = 'M';
static const uint8_t MP_MAGIC1 = 'P';
static const uint8_t MP_VER    = 0x01;
static const uint8_t DATA_START = 4;   // first user page on NTAG21x

bool NfcReader::begin() {
  Wire.begin();                        // SDA=21, SCL=22 by default on ESP32
  pn532.begin();
  uint32_t version = pn532.getFirmwareVersion();
  if (!version) {
    Serial.println("[nfc] PN532 not found — check wiring / I2C address");
    return false;
  }
  Serial.printf("[nfc] PN532 firmware 0x%08X\n", version);
  pn532.SAMConfig();                   // configure to read RFID tags
  return true;
}

bool NfcReader::tagPresent(String& uidHex) {
  uint8_t uid[7] = {0};
  uint8_t uidLen = 0;
  // 50ms timeout keeps the main loop responsive.
  if (!pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 50)) {
    return false;
  }
  uidHex = "";
  for (uint8_t i = 0; i < uidLen; i++) {
    if (uid[i] < 0x10) uidHex += '0';
    uidHex += String(uid[i], HEX);
  }
  return true;
}

bool NfcReader::readPage(uint8_t page, uint8_t* buf4) {
  // Adafruit returns 4 bytes of the requested page into buf4.
  return pn532.mifareultralight_ReadPage(page, buf4);
}

bool NfcReader::writePage(uint8_t page, const uint8_t* buf4) {
  return pn532.mifareultralight_WritePage(page, const_cast<uint8_t*>(buf4));
}

bool NfcReader::readAlbumId(String& albumIdOut) {
  uint8_t hdr[4];
  if (!readPage(DATA_START, hdr)) return false;
  if (hdr[0] != MP_MAGIC0 || hdr[1] != MP_MAGIC1 || hdr[2] != MP_VER) {
    return false;                      // not one of our plates
  }
  uint8_t len = hdr[3];
  if (len == 0 || len > 40) return false;

  albumIdOut = "";
  uint8_t page = DATA_START + 1;
  uint8_t buf[4];
  while (albumIdOut.length() < len) {
    if (!readPage(page, buf)) return false;
    for (uint8_t i = 0; i < 4 && albumIdOut.length() < len; i++) {
      albumIdOut += (char)buf[i];
    }
    page++;
  }
  return true;
}

bool NfcReader::writeAlbumId(const String& albumId) {
  uint8_t len = albumId.length();
  if (len == 0 || len > 40) return false;

  // header page
  uint8_t hdr[4] = { MP_MAGIC0, MP_MAGIC1, MP_VER, len };
  if (!writePage(DATA_START, hdr)) return false;

  // data pages (zero-padded to a 4-byte boundary)
  uint8_t page = DATA_START + 1;
  for (uint8_t i = 0; i < len; i += 4) {
    uint8_t buf[4] = {0, 0, 0, 0};
    for (uint8_t j = 0; j < 4 && (i + j) < len; j++) {
      buf[j] = (uint8_t)albumId[i + j];
    }
    if (!writePage(page, buf)) return false;
    page++;
  }
  return true;
}
