/*
 * SD_XModem.ino - XModem file transfer example (SD backend)
 *
 * Storage library: https://github.com/arduino-libraries/SD
 *
 * On boot this sketch mounts an SD card, then repeatedly asks over the USB
 * console (Serial) whether you want to send or receive a file. The actual
 * XModem transfer runs over a second UART (Serial1) so the binary protocol
 * never collides with the interactive menu.
 *
 * Wiring:
 *   - SD card on the SPI bus, chip-select on SD_CS (adjust below).
 *   - The XModem peer (e.g. a PC running a terminal in XModem mode) on Serial1.
 */

#include <XModem.h>   // pulls in <SD.h>

const uint8_t SD_CS = 4;          // adjust to your shield / wiring
const unsigned long CONSOLE_BAUD = 115200;
const unsigned long LINK_BAUD = 115200;

// Progress / confirmation callback. Return false to abort (or to deny an
// overwrite); return true to continue.
bool onXmodemUpdate(uint8_t code, uint8_t value) {
  switch (code) {
    case 0:  // a data block crossed the link ok
      Serial.print(F("  block ok: "));
      Serial.println(value);
      break;
    case 1:
      if (value == 1) {
        Serial.println(F("  destination exists - overwriting"));
      } else {
        Serial.println(F("  file not found"));
      }
      break;
    case 2:
      Serial.println(F("  storage error"));
      break;
    case 3:  // protocol / signalling event, value carries the detail
      Serial.print(F("  protocol event: "));
      Serial.println(value);
      break;
  }
  return true;
}

char readChar() {
  while (Serial.available()) Serial.read();   // flush stale input
  while (!Serial.available()) {}
  return Serial.read();
}

String promptLine(const __FlashStringHelper *msg) {
  Serial.print(msg);
  while (Serial.available()) Serial.read();   // flush stale input
  String line = "";
  for (;;) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        if (line.length() > 0) break;
      } else {
        line += c;
      }
    }
  }
  Serial.println(line);
  return line;
}

void setup() {
  Serial.begin(CONSOLE_BAUD);
  while (!Serial) {}                 // wait for the USB console
  Serial1.begin(LINK_BAUD);          // XModem transfer link

  Serial.println(F("SD + XModem example"));

  if (!SD.begin(SD_CS)) {
    Serial.println(F("SD init failed - check the card and SD_CS pin"));
    while (true) {}
  }

  myXModem.onXmodemUpdate(onXmodemUpdate);
  if (!myXModem.begin(&Serial1, XModem::CRC_XMODEM)) {
    Serial.println(F("XModem begin() failed"));
    while (true) {}
  }
}

void loop() {
  Serial.println();
  Serial.println(F("Choose an action:"));
  Serial.println(F("  [S] send a file from the SD card"));
  Serial.println(F("  [R] receive a file onto the SD card"));
  Serial.print(F("> "));

  char choice = readChar();
  Serial.println(choice);

  if (choice == 's' || choice == 'S') {
    String path = promptLine(F("file to send: "));
    Serial.println(F("waiting for the receiver..."));
    Serial.println(myXModem.sendFile(path) ? F("send complete") : F("send failed"));
  } else if (choice == 'r' || choice == 'R') {
    String path = promptLine(F("save received file as: "));
    Serial.println(F("waiting for the sender..."));
    Serial.println(myXModem.receiveFile(path) ? F("receive complete") : F("receive failed"));
  } else {
    Serial.println(F("unknown option"));
  }
}
