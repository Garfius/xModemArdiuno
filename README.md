# XModem

An Arduino library for half-duplex file transfer using the **XModem** and
**XModem/CRC** protocols over any `Stream` (for example, a hardware `Serial`
port). Data is read from and written to any `fs::FS` filesystem you pass to
`begin()` — for example `LittleFS` or `SDFS`.

Created and tested using [Arduino-Pico by earlephilhower/maxgerhardt](https://github.com/earlephilhower/arduino-pico) on [Platformio](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide)

## Features

- Classic **XMODEM** (128-byte blocks, 8-bit checksum) and **XMODEM/CRC**
  (CRC-16/XMODEM) variants.
- Send and receive whole files to/from any `fs::FS` filesystem (`LittleFS`,
  `SDFS`, ...).
- Progress, error, and overwrite-confirmation callback.
- Configurable block id size, checksum size, data size, retry limit, and
  signal-retry delay for talking to non-standard peers.
- Optional non-sequential block ids and buffered/unbuffered packet reads.

## Installation

1. Copy this repository into your Arduino `libraries` folder (or install it as
   a `.zip` library from the Arduino IDE).
2. Make sure a filesystem that implements `fs::FS` is available (for example the
   RP2040/ESP core's `LittleFS` or `SDFS`); `XModem.h` only includes `<FS.h>`.
3. Restart the Arduino IDE.

## Dependencies

- An `fs::FS`-based filesystem such as [`LittleFS`](https://github.com/earlephilhower/arduino-pico/tree/master/libraries/LittleFS)
  or `SDFS` (provided by the RP2040/ESP core). `XModem.h` includes `<FS.h>`
  only; you include and mount the concrete filesystem in your sketch.

## Quick start

```cpp
#include <SDFS.h>     // arduino-pico core SD filesystem (fs::FS)
#include <XModem.h>

const uint8_t SD_CS = 4;

// Return false to abort (e.g. deny an overwrite); return true to continue.
bool onXmodemUpdate(uint8_t code, uint8_t value) {
  switch (code) {
    case 0: /* block `value` transferred ok        */ break;
    case 1: /* value 1: overwriting, else not found */ break;
    case 2: /* storage error                        */ break;
    case 3: /* protocol event, see `value`          */ break;
  }
  return true;
}

void setup() {
  Serial.begin(115200);     // console
  Serial1.begin(115200);    // XModem transfer link

  SDFS.setConfig(SDFSConfig(SD_CS));
  if (!SDFS.begin()) {
    Serial.println("SD init failed");
    while (true) {}
  }

  myXModem.onXmodemUpdate(onXmodemUpdate);
  myXModem.begin(&Serial1, XModem::CRC_XMODEM, SDFS);
}

void loop() {
  // Send a file from the filesystem:
  myXModem.sendFile("data.bin");

  // Or receive a file onto the filesystem:
  myXModem.receiveFile("incoming.bin");
}
```

> **Tip:** run the interactive menu over `Serial` (USB) and the XModem transfer
> over a separate UART (`Serial1`) so the binary protocol never collides with
> console output.

## API overview

### Setup

| Method | Description |
| --- | --- |
| `onXmodemUpdate(callback)` | Register the progress/error/confirmation callback. Must be set before `begin()`. |
| `begin(stream, type, fs)` | Configure the transfer stream, protocol (`XModem::XMODEM` or `XModem::CRC_XMODEM`), and the `fs::FS` filesystem to store to / read from (e.g. `LittleFS`, `SDFS`). Returns `false` if no callback is registered. |

### Transfers

| Method | Description |
| --- | --- |
| `receiveFile(path, size, binary)` | Receive a file and write it to `path` on the filesystem. |
| `sendFile(path)` | Send the file at `path` from the filesystem. |
| `send(data, len, start_id)` | Send a single in-memory buffer as one block. |
| `pathAssert(path)` | Create any missing intermediate directories on the filesystem. |

> **⚠️ Important — the `size` argument of `receiveFile()`:** XModem always
> transfers data in fixed 128-byte blocks and pads the final block with `SUB`
> (`0x1A`) filler bytes. If you leave `size` at its default (`-1`, unknown), the
> file written to disk is stored verbatim and its size is therefore **rounded up
> to the next multiple of 128 bytes** (the padding is kept). To recover the
> *exact* original file size, you must pass the real byte count as `size`;
> `receiveFile()` then truncates the last block so the stored file matches the
> original length precisely.
>
> Because the classic XModem protocol does **not** transmit the file size in-band,
> the sender cannot tell the receiver how large the file is. You must obtain the
> length through a **separate, out-of-band method** (for example, agree on it
> beforehand, or send it over another channel / a preceding message) and hand it
> to `receiveFile(path, size)`. Without that value the receiver can only produce a
> 128-byte-aligned file, not the exact original size.

### Tuning (override the defaults from `begin()`)

| Method | Default | Description |
| --- | --- | --- |
| `setIdSize(n)` | `1` | Bytes per block id. |
| `setChecksumSize(n)` | `1` / `2` | Bytes per checksum (`1` = basic, `2` = CRC). |
| `setDataSize(n)` | `128` | Bytes per data block. |
| `setSendInitByte(b)` | `NAK` / `'C'` | Byte the receiver sends to request a start. |
| `setRetryLimit(n)` | `5` | Max retries before a step aborts. |
| `setSignalRetryDelay(ms)` | `100` | Delay between polls while waiting for a signal byte. |
| `allowNonSequentailBlocks(b)` | `false` | Accept any incoming block id instead of enforcing `+1` sequencing. |
| `bufferPacketReads(b)` | `true` | Read a whole block into a buffer before validating. |

### Callback codes

| `code` | Meaning |
| --- | --- |
| `0` | Block `value` transferred ok. |
| `1` | `value == 2`: confirm overwrite; else file not found. |
| `2` | SD card / storage problem. |
| `3` | Internal signal / protocol event (see `value`). |

## Examples

- [`examples/SD_XModem`](examples/SD_XModem/SD_XModem.ino) — send/receive files
  using an SD card.
- [`examples/LittleFS_XModem`](examples/LittleFS_XModem/LittleFS_XModem.ino) —
  interactive menu example with a LittleFS-based board.

## Protocol references

- <https://www.menie.org/georges/embedded/xmodem_specification.html>
- <http://wiki.synchro.net/ref:xmodem>

## License

See the repository for license details.
