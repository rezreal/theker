#include "hismith_protocol.h"

#include <cstring>

namespace hismith {

void Parser::reset() {
    len_ = 0;
}

Parser::Result Parser::push(uint8_t byte, Frame& out) {
    if (len_ == 0) {
        // Hunting for a header: anything that is not 0xCC is noise.
        if (byte != kFrameHeader) {
            return Result::NeedMore;
        }
        buf_[len_++] = byte;
        return Result::NeedMore;
    }

    buf_[len_++] = byte;
    if (len_ < kFrameLen) {
        return Result::NeedMore;
    }

    const uint8_t cmd = buf_[1];
    const uint8_t arg = buf_[2];
    if (buf_[3] == checksum(cmd, arg)) {
        out.cmd = cmd;
        out.arg = arg;
        len_ = 0;
        ++frames_;
        return Result::Frame;
    }

    // Checksum failed, so the 0xCC we locked onto was probably payload or
    // noise rather than a real header. Discard it and resynchronise from the
    // next 0xCC already in the buffer, so a header that arrived mid-garbage is
    // not thrown away with it.
    ++bad_;
    size_t next = 1;
    while (next < kFrameLen && buf_[next] != kFrameHeader) {
        ++next;
    }
    len_ = kFrameLen - next;
    if (len_ > 0) {
        std::memmove(buf_, buf_ + next, len_);
    }
    return Result::BadChecksum;
}

}  // namespace hismith
