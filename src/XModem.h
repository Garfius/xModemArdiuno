/*
 * XModem.h - Library for communicating using the XModem protocol (half-duplex)
 *
 * Protocal Resources:
 * https://www.menie.org/georges/embedded/xmodem_specification.html
 * http://wiki.synchro.net/ref:xmodem
 * http://pauillac.inria.fr/~doligez/zmodem/ymodem.txt
 * 
 * TO-DO <--- ----> search (* to handle) at XModem.cpp, i've gotta go!
 */
#ifndef XModem_h
#define XModem_h
#include "Arduino.h"
#include <FS.h>
//XModem constants
#define SOH (byte) 0x01 //Start of Header
#define EOT (byte) 0x04 //End of Transmission
#define ACK (byte) 0x06 //Acknowledge
#define NAK (byte) 0x15 //Negative Acknowledge
#define CAN (byte) 0x18 //Cancel Transmission
#define SUB (byte) 0x1A //Padding
#define CRC_CHR (byte) 'C' //CRC mode request, sent by a receiver in place of NAK

#define XMODEM_RETRIES 5 //default retry_limit applied by begin()

/**
 * XModem/CRC-XModem half-duplex file transfer over a Stream (e.g. Serial), with an SD card as the storage backend.
 */
class XModem {
  public:
    /** Selects the checksum scheme and default handshake byte used by begin(). */
    enum ProtocolType {
      XMODEM,     ///< Classic XMODEM: 1-byte checksum, handshake with NAK.
      CRC_XMODEM  ///< XMODEM/CRC: 2-byte CRC-16/XMODEM checksum, handshake with 'C'.
    };

    XModem();

    /**
     * Configures the instance for a transfer. Must be called (with a callback already
     * registered via onXmodemUpdate()) before receiveFile()/sendFile()/send().
     * @param serial stream used for the transfer (e.g. &Serial).
     * @param type protocol variant; sets id/checksum/data block sizes and init byte.
     * @param fs filesystem the transfer reads from / writes to (e.g. LittleFS or SDFS).
     * @return false if no update callback has been registered yet.
     */
    bool begin(Stream *serial, XModem::ProtocolType type, fs::FS &fs);

    // SETTERS - override the defaults applied by begin() for non-standard peers
    void setIdSize(size_t size);              ///< Bytes per block id (default 1).
    void setChecksumSize(size_t size);        ///< Bytes per checksum (1=basic, 2=CRC).
    void setDataSize(size_t size);             ///< Bytes per data block (default 128).
    void setSendInitByte(byte b);              ///< Byte the receiver sends to request a start (NAK or CRC_CHR).
    void setRetryLimit(byte limit);            ///< Max retries before a step aborts as unrecoverable.
    void setSignalRetryDelay(unsigned long ms);///< Delay between polls while waiting for a signal byte.
    void allowNonSequentailBlocks(bool b);     ///< If true, accept any incoming block id as-is instead of enforcing +1 sequencing.
    void bufferPacketReads(bool b);            ///< If true, read a whole block into a buffer before validating it, instead of byte-by-byte.

    /** Creates any missing intermediate directories on the SD card for path (path must not start with '/'). */
    bool pathAssert(const char * path);

    /** Receives a file over the link and writes it to filePath on the SD card, deleting a partial file on failure. */
    bool receiveFile(String filePath,unsigned int size=-1, bool binary=true);

    /** Sends a single in-memory buffer as one block starting at start_id. */
    bool send(byte data[], size_t data_len, unsigned long start_id= 1);

    /** Sends the file at filePath from the SD card, chunked into blocks. */
    bool sendFile(String filePath);

    /** Sends an empty lookup request for id so the peer resolves data itself (see dummy_block_lookup). */
    bool lookup_send(unsigned long long id);

    /** Registers the callback used to report progress/errors and to ask for destructive-action confirmation. */
    void onXmodemUpdate(bool(*callback)(uint8_t code, uint8_t value));

    /**
     * Progress/error/confirmation callback. Must return false to abort the current
     * operation (e.g. deny overwrite) or true to continue.
     * code 0: block value transferred ok; code 1: file not found(1)/confirm overwrite(2);
     * code 2: SD card problem; code 3: internal signal/protocol event, see value for detail.
     */
    bool(*_onXmodemUpdateHandler)(uint8_t code, uint8_t value) = nullptr;

    /** Describes a set of packets to send in one pass; id_arr holds count block ids, each _id_bytes long, big-endian. */
    struct bulk_data {
      byte **data_arr; ///< Per-packet data pointers (unused when file == true).
      size_t *len_arr; ///< Per-packet data lengths (unused when file == true).
      byte *id_arr;    ///< Concatenated block ids, each _id_bytes long, big-endian.
      size_t count;    ///< Number of packets described by this container.
    };

    unsigned int sizeKnown=-1; ///< Expected total transfer size in bytes; -1 means unknown.

    fs::File workingFile; ///< File currently open for the in-progress send/receive.

  private:
    Stream *_serial;                 ///< Stream used for the transfer.
    fs::FS *_fs = nullptr;           ///< Filesystem backing the transfer (set by begin()).
    ProtocolType _protocol;          ///< Active protocol variant (may change during init_tx()/init_rx()).
    byte _rx_init_byte;              ///< Byte the receiver sends to request/start a transfer.
    size_t _id_bytes;                ///< Bytes per block id.
    size_t _chksum_bytes;            ///< Bytes per checksum (1=basic, 2=CRC-16).
    size_t _data_bytes;              ///< Bytes per data block.
    byte retry_limit;                ///< Max retries for handshake/signal/packet steps.
    unsigned long _signal_retry_delay_ms; ///< Delay between polls while waiting for a signal byte.
    bool _allow_nonsequential;       ///< If true, accept any incoming block id instead of enforcing sequencing.
    bool _buffer_packet_reads;       ///< If true, read/validate a whole block via a buffer instead of byte-by-byte.
    unsigned int sizeReceived=-1;    ///< Bytes written so far during the current receive.
    bool binary = false;             ///< If false, trailing SUB padding bytes are stripped from the last block.
    Stream * _userCli;               ///< Unused/reserved.

    void calc_chksum (byte *data, size_t dataSize, byte *chksum); ///< Dispatches to basic_chksum() or crc_16_chksum() per _protocol.
    bool send_bulk_data(struct bulk_data container,bool file = false); ///< Runs init_tx(), sends every packet in container (via tx() or txFile()), then close_tx().
    bool dummy_rx_block_handler(byte *blk_id, size_t idSize, byte *data, size_t dataSize); ///< Writes one validated received block to workingFile and reports progress.
    void dummy_block_lookup(void *blk_id, size_t idSize, byte *data, size_t dataSize); ///< Fills send_data for a lookup_send() packet (currently a stub: fills with 0x3A).
    void basic_chksum(byte *data, size_t dataSize, byte *chksum); ///< Classic XMODEM 8-bit sum.
    void crc_16_chksum(byte *data, size_t dataSize, byte *chksum); ///< CRC-16/XMODEM (poly 0x1021, init 0x0000, MSB first).

    /** Working view over one packet's id/checksum/data slices, which point into a single caller-owned buffer. */
    struct packet {
      byte *id;
      byte *chksum;
      byte *data;
      unsigned short crc;
    };

    bool openFiles(const char * filePath, bool write); ///< Opens workingFile for read or write; on write, confirms and removes an existing file first.
    void closeFiles();                                  ///< Closes workingFile.

    bool receive();     ///< Runs init_rx() + rx(); sends CAN x3 on unrecoverable failure.
    bool init_rx();     ///< Handshake: repeatedly sends _rx_init_byte until SOH is seen or retries are exhausted.
    bool find_header();  ///< Re-sync: sends NAK and waits for SOH, used after a bad block.
    bool rx();          ///< Main receive loop: reads blocks, validates id/checksum, dispatches to the file writer, and drives ACK/NAK/EOT signaling.
    bool read_block(struct packet *p, byte *buffer);           ///< Dispatches to the buffered or unbuffered block reader per _buffer_packet_reads.
    bool read_block_buffered(struct packet *p, byte *buffer);   ///< Reads a whole block into buffer, then validates id/complement/checksum.
    bool read_block_unbuffered(struct packet *p);                ///< Reads and validates a block field-by-field directly from the stream.
    bool fill_buffer(byte *buffer, size_t bytes);                ///< Reads exactly bytes bytes, retrying short reads; fails only on a fully empty read.

    bool init_tx();      ///< Handshake: waits for the receiver's NAK/CRC_CHR and adopts the matching protocol.
    bool find_init_signal(byte *received, byte timeout_secs); ///< Scans the stream for NAK or CRC_CHR within timeout_secs.
    bool tx(struct packet *p, byte *data, size_t data_len, byte *blk_id);   ///< Sends an in-memory buffer as one or more blocks, padding the final short block with SUB.
    bool txFile(struct packet *p,byte *blk_id);                             ///< Sends workingFile as a sequence of blocks, padding the final short block with 0x1A.
    void build_packet(struct packet *p, byte *id, byte *data, size_t data_len); ///< Fills a packet's id/data/checksum fields (data==NULL triggers dummy_block_lookup()).
    bool send_packet(struct packet *p);                                     ///< Writes one full packet (SOH, id+complement, data, checksum) and waits for ACK/NAK/CAN.
    bool close_tx();                                                        ///< Sends EOT until ACKed (or CAN'd) to end the transfer.

    void increment_id(byte *id, size_t length); ///< Increments a big-endian multi-byte block id in place, with carry.
    byte tx_signal(byte signal);                 ///< Sends signal and waits for a SOH/EOT/CAN/ACK/NAK reply, retrying on no response.
    byte rx_signal();                            ///< Waits for and returns an ACK/NAK/CAN reply (255 on timeout/unrecognized byte).
    bool find_byte_timed(byte b, byte timeout_secs); ///< Waits up to timeout_secs for byte b to appear on the stream.


  };

/** Shared singleton instance, referenced by user code via extern. */
extern XModem myXModem;

#endif
