/*
 * LittleFS_XModem.ino - XModem file transfer example (LittleFS backend)
 *
 * Storage library: https://github.com/littlefs-project/littlefs
 * (via an Arduino wrapper such as the ESP or RP2040 core's <LittleFS.h>)
 *
 * On boot this sketch mounts LittleFS, then repeatedly asks over the USB
 * console (Serial) whether you want to send or receive a file. The actual
 * XModem transfer runs over a second UART (Serial1) so the binary protocol
 * never collides with the interactive menu.
 *
 * The XModem library is filesystem-agnostic: begin() takes the fs::FS to use,
 * so passing LittleFS makes every send/receive read and write LittleFS
 * directly - the same filesystem listFiles() enumerates below.
 */

#include <LittleFS.h>
#include <XModem.h>
#define serialConsole Serial1
#define serialXModem Serial2

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
// is available to send. The RP2040/ESP LittleFS wrapper iterates directories
// through openDir()/fs::Dir rather than the SD-style openNextFile().
void listFiles(const char *path, uint8_t depth) {
  fs::Dir dir = LittleFS.openDir(path);
  while (dir.next()) {
    for (uint8_t i = 0; i <= depth; i++) serialConsole.print(F("  "));
    serialConsole.print(dir.fileName());
    if (dir.isDirectory()) {
      serialConsole.println('/');
      String sub = String(path);
      if (!sub.endsWith("/")) sub += '/';
      sub += dir.fileName();
      listFiles(sub.c_str(), depth + 1);
    } else {
      serialConsole.print(F("\t"));
      serialConsole.println(dir.fileSize());
    }
  }
}

void setup() {
  serialConsole.begin(CONSOLE_BAUD);
  while (!serialConsole) {}                 // wait for the USB console
  serialXModem.begin(LINK_BAUD);          // XModem transfer link

  serialConsole.println(F("LittleFS + XModem example"));

  if (!LittleFS.begin()) {
    serialConsole.println(F("LittleFS mount failed - format the flash and retry"));
    while (true) {}
  }

  myXModem.onXmodemUpdate(onXmodemUpdate);
  if (!myXModem.begin(&serialXModem, XModem::CRC_XMODEM, LittleFS)) {
    serialConsole.println(F("XModem begin() failed"));
    while (true) {}
  }
}

void loop() {
  serialConsole.println();
  serialConsole.println(F("available files:"));
  listFiles("/", 0);

  serialConsole.println();
  serialConsole.println(F("Choose an action:"));
  serialConsole.println(F("  [S] send a file"));
  serialConsole.println(F("  [R] receive a file"));
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
