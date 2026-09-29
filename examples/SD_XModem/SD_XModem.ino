/*
 * SD_XModem.ino - XModem file transfer example (SD backend)
 *
 * Storage library: the arduino-pico core SDFS filesystem, which exposes the
 * card as an fs::FS via the global SDFS object.
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

#include <SDFS.h>     // arduino-pico core SD filesystem (fs::FS)
#include <XModem.h>

#define serialConsole Serial1
#define serialXModem Serial2

const uint8_t SD_CS = 5;          // adjust to your shield / wiring
const unsigned long CONSOLE_BAUD = 115200;
const unsigned long LINK_BAUD = 115200;

// Progress / confirmation callback. Return false to abort (or to deny an
// overwrite); return true to continue.
bool onXmodemUpdate(uint8_t code, uint8_t value) {
  switch (code) {
    case 0:  // a data block crossed the link ok
      serialConsole.print(F("  block ok: "));
      serialConsole.println(value);
      break;
    case 1:
      if (value == 1) {
        serialConsole.println(F("  destination exists - overwriting"));
      } else {
        serialConsole.println(F("  file not found"));
      }
      break;
    case 2:
      serialConsole.println(F("  storage error"));
      break;
    case 3:  // protocol / signalling event, value carries the detail
      serialConsole.print(F("  protocol event: "));
      serialConsole.println(value);
      break;
  }
  return true;
}

char readChar() {
  while (serialConsole.available()) serialConsole.read();   // flush stale input
  while (!serialConsole.available()) {}
  return serialConsole.read();
}

String promptLine(const __FlashStringHelper *msg) {
  serialConsole.print(msg);
  while (serialConsole.available()) serialConsole.read();   // flush stale input
  String line = "";
  for (;;) {
    if (serialConsole.available()) {
      char c = serialConsole.read();
      if (c == '\n' || c == '\r') {
        if (line.length() > 0) break;
      } else {
        line += c;
      }
    }
  }
  serialConsole.println(line);
  return line;
}

// Prints every file (recursing into sub-directories) so the user can see what
// is available to send.
void listFiles(File dir, uint8_t depth) {
  for (;;) {
    File entry = dir.openNextFile();
    if (!entry) break;
    for (uint8_t i = 0; i <= depth; i++) serialConsole.print(F("  "));
    serialConsole.print(entry.name());
    if (entry.isDirectory()) {
      serialConsole.println('/');
      listFiles(entry, depth + 1);
    } else {
      serialConsole.print(F("\t"));
      serialConsole.println(entry.size());
    }
    entry.close();
  }
}

void setup() {
  serialConsole.begin(CONSOLE_BAUD);
  while (!serialConsole) {}                 // wait for the USB console
  serialXModem.begin(LINK_BAUD);          // XModem transfer link

  serialConsole.println(F("SD + XModem example"));

  SDFS.setConfig(SDFSConfig(SD_CS));
  if (!SDFS.begin()) {
    serialConsole.println(F("SD init failed - check the card and SD_CS pin"));
    while (true) {}
  }

  myXModem.onXmodemUpdate(onXmodemUpdate);
  if (!myXModem.begin(&serialXModem, XModem::CRC_XMODEM, SDFS)) {
    serialConsole.println(F("XModem begin() failed"));
    while (true) {}
  }
}

void loop() {
  serialConsole.println();
  serialConsole.println(F("available files:"));
  File root = SDFS.open("/", "r");
  listFiles(root, 0);
  root.close();

  serialConsole.println();
  serialConsole.println(F("Choose an action:"));
  serialConsole.println(F("  [S] send a file from the SD card"));
  serialConsole.println(F("  [R] receive a file onto the SD card"));
  serialConsole.print(F("> "));

  char choice = readChar();
  serialConsole.println(choice);

  if (choice == 's' || choice == 'S') {
    String path = promptLine(F("file to send: "));
    serialConsole.println(F("waiting for the receiver..."));
    serialConsole.println(myXModem.sendFile(path) ? F("send complete") : F("send failed"));
  } else if (choice == 'r' || choice == 'R') {
    String path = promptLine(F("save received file as: "));
    serialConsole.println(F("waiting for the sender..."));
    serialConsole.println(myXModem.receiveFile(path) ? F("receive complete") : F("receive failed"));
  } else {
    serialConsole.println(F("unknown option"));
  }
}
