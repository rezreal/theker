#pragma once

#include <cstddef>
#include <cstdint>

// Hismith BLE protocol framing.
//
// Pure C++: no Arduino includes, so this builds and is tested on the host.

namespace hismith {

// GATT identifiers. This is the classic HM-10 BLE-serial layout, which is why
// the transport below is a byte stream rather than discrete packets.
//
// HM-10 style modules split the two directions across two services: ffe5/ffe9
// carries host -> device writes, ffe0/ffe4 carries device -> host notifies.
// Only the write side is needed to drive the relay, but a host doing service
// discovery may expect to find both.
constexpr char kServiceUuid[] = "0000ffe5-0000-1000-8000-00805f9b34fb";
constexpr char kWriteCharUuid[] = "0000ffe9-0000-1000-8000-00805f9b34fb";
constexpr char kNotifyServiceUuid[] = "0000ffe0-0000-1000-8000-00805f9b34fb";
constexpr char kNotifyCharUuid[] = "0000ffe4-0000-1000-8000-00805f9b34fb";

constexpr uint8_t kFrameHeader = 0xCC;
constexpr size_t kFrameLen = 4;

constexpr uint8_t kCmdLube = 0x0B;
constexpr uint8_t kArgSquirt = 0x01;

// Frame layout: 0xCC | cmd | arg | checksum
//
// The checksum is an additive one over cmd and arg. Only the lube squirt
// command (cc 0b 01 0c) is documented for the Piupiu, but the rule holds
// across every published Hismith sample -- e.g. cc 01 00 01 and cc 01 a1 a2
// from the S2 -- so we validate rather than compare against a magic string.
constexpr uint8_t checksum(uint8_t cmd, uint8_t arg) {
    return static_cast<uint8_t>(cmd + arg);
}

struct Frame {
    uint8_t cmd = 0;
    uint8_t arg = 0;

    bool isSquirt() const { return cmd == kCmdLube && arg == kArgSquirt; }
};

// Incremental parser over the write-characteristic byte stream.
//
// A host's writes are not guaranteed to align with frames: they may split one
// frame across writes or pack several into one. Feed every received byte here;
// the parser locks onto the 0xCC header and resynchronises after garbage.
class Parser {
  public:
    enum class Result : uint8_t {
        NeedMore,     // byte consumed, frame still incomplete
        Frame,        // `out` now holds a checksum-valid frame
        BadChecksum,  // four bytes formed a frame but the checksum failed
    };

    // Consumes one byte. Returns Frame exactly once per valid frame.
    Result push(uint8_t byte, Frame& out);

    void reset();

    uint32_t framesParsed() const { return frames_; }
    uint32_t badChecksums() const { return bad_; }

  private:
    uint8_t buf_[kFrameLen] = {};
    size_t len_ = 0;
    uint32_t frames_ = 0;
    uint32_t bad_ = 0;
};

}  // namespace hismith
