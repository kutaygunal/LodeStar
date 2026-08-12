// core/scenario/sbas/SbasMessage.cpp
// SBAS message parsing with 24-bit CRC and bit-field decoding (Item 6.1).

#include "core/scenario/sbas/SbasMessage.h"

#include <cstdint>

#include "core/scenario/ScenarioError.h"

namespace lodestar::scenario {

namespace {
// SBAS 24-bit CRC (RTCA DO-229, generator polynomial 0x1864CFB).
constexpr uint32_t kCrcPoly = 0x1864CFB;
constexpr int kTotalBits = 250;

uint32_t crc24(const std::vector<uint8_t>& bits) {
    uint32_t crc = 0;
    for (int i = 0; i < kTotalBits; ++i) {
        uint8_t bit = (i < static_cast<int>(bits.size())) ? (bits[i] & 1) : 0;
        crc <<= 1;
        if ((crc & 0x1000000) != 0) crc ^= kCrcPoly;
        if (bit) crc ^= 1;
    }
    return crc & 0xFFFFFF;
}

uint32_t readBits(const std::vector<uint8_t>& bits, int start, int length) {
    uint32_t v = 0;
    for (int i = 0; i < length; ++i) {
        int idx = start + i;
        if (idx < 0 || idx >= static_cast<int>(bits.size())) return v;
        v = (v << 1) | (bits[idx] & 1);
    }
    return v;
}
}  // namespace

Result<SbasMessage> SbasMessage::parse(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 32) {
        return Result<SbasMessage>::err("SbasMessage: message too short");
    }

    // Unpack bytes (MSB-first) into bits.
    std::vector<uint8_t> bits;
    bits.reserve(bytes.size() * 8);
    for (uint8_t b : bytes) {
        for (int i = 7; i >= 0; --i) bits.push_back((b >> i) & 1);
    }

    // Compute CRC over the first 226 bits (message), compare with last 24 bits.
    std::vector<uint8_t> msg(bits.begin(), bits.begin() + 226);
    uint32_t expectedCrc = readBits(bits, 226, 24);
    uint32_t calc = crc24(msg);
    if (calc != expectedCrc) {
        return Result<SbasMessage>::err("SbasMessage: CRC mismatch");
    }

    SbasMessage m;
    m.bits_ = bits;
    m.type_ = static_cast<int>(readBits(bits, 8, 6));
    return Result<SbasMessage>::ok(m);
}

uint32_t SbasMessage::getBits(int start, int length) const {
    return readBits(bits_, start, length);
}

uint32_t SbasMessage::fastCorrectionPrn() const {
    if (type_ != 2 && type_ != 3 && type_ != 4 && type_ != 6) return 0;
    // Slot number in bits 14-19 (DO-229 fast correction). PRN = slot + 120.
    uint32_t slot = getBits(14, 6);
    return slot + 120;
}

double SbasMessage::fastCorrectionPrc() const {
    if (type_ != 2 && type_ != 3 && type_ != 4 && type_ != 6) return 0.0;
    // PRC in bits 46-57, scale 0.125 m, signed 12-bit.
    int32_t raw = static_cast<int32_t>(getBits(46, 12));
    if (raw & 0x800) raw |= ~0xFFF;  // sign-extend
    return static_cast<double>(raw) * 0.125;
}

double SbasMessage::fastCorrectionUdre() const {
    if (type_ != 2 && type_ != 3 && type_ != 4 && type_ != 6) return 0.0;
    // UDREI in bits 60-63.
    uint32_t udrei = getBits(60, 4);
    // Map UDREI to UDRE (meters) per DO-229 Table.
    static const double udreMap[16] = {0.75, 1.0, 1.25, 1.75, 2.25, 3.0, 3.75,
                                       4.5, 5.25, 6.0, 7.5, 15.0, 100.0,
                                       100.0, 100.0, 100.0};
    return udreMap[udrei & 15];
}

bool SbasMessage::fastCorrectionValid() const {
    return (type_ == 2 || type_ == 3 || type_ == 4 || type_ == 6);
}

}  // namespace lodestar::scenario
