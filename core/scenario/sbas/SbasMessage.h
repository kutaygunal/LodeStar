// core/scenario/sbas/SbasMessage.h
// SBAS message parsing per RTCA DO-229 (Item 6.1).

#pragma once

#include <cstdint>
#include <vector>

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class SbasMessage {
public:
    // Parse a 250-bit SBAS message (bytes are MSB-first). Validates the 24-bit
    // CRC and extracts the 6-bit message type.
    static Result<SbasMessage> parse(const std::vector<uint8_t>& bytes);

    int type() const { return type_; }

    // Raw decoded bits (250 bits, MSB-first).
    const std::vector<uint8_t>& bits() const { return bits_; }

    // Bit accessor helpers (MSB-first). Returns 0 on out-of-range.
    uint32_t getBits(int start, int length) const;

    // Convenience: satellite slot PRN (for fast correction messages).
    uint32_t fastCorrectionPrn() const;
    // Fast correction: pseudorange correction (m) and UDRE.
    double fastCorrectionPrc() const;
    double fastCorrectionUdre() const;
    bool fastCorrectionValid() const;

private:
    int type_ = 0;
    std::vector<uint8_t> bits_;
};

}  // namespace lodestar::scenario
