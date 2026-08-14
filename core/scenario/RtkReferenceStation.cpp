// core/scenario/RtkReferenceStation.cpp
// Gap-Fill ScenarioForge 5.2: RTK virtual reference station.

#include "core/scenario/RtkReferenceStation.h"

#include <cmath>
#include <cstdio>
#include <sstream>

namespace lodestar::scenario {

namespace {

// Build an RTCM 3.3 frame: 8-bit preamble + 6-bit reserved + 10-bit length +
// message payload + 24-bit CRC-24Q.
std::vector<std::uint8_t> makeFrame(std::uint16_t messageType,
                                    const std::vector<std::uint8_t>& payload) {
    // Header is 3 bytes: preamble 0xD3, then 6 reserved bits + 10-bit length.
    std::vector<std::uint8_t> frame;
    frame.push_back(0xD3);
    std::uint16_t len = static_cast<std::uint16_t>(payload.size());
    frame.push_back(static_cast<std::uint8_t>((len >> 8) & 0x03));
    frame.push_back(static_cast<std::uint8_t>(len & 0xFF));
    frame.insert(frame.end(), payload.begin(), payload.end());

    // CRC-24Q (simplified deterministic CRC for test fixtures). This is the
    // standard RTCM CRC-24Q polynomial; implemented over the header+payload.
    std::uint32_t crc = 0;
    for (std::size_t i = 0; i < frame.size(); ++i) {
        crc ^= (static_cast<std::uint32_t>(frame[i]) << 16);
        for (int bit = 0; bit < 8; ++bit) {
            crc <<= 1;
            if (crc & 0x1000000) crc ^= 0x1864CFB;
        }
    }
    frame.push_back(static_cast<std::uint8_t>((crc >> 16) & 0xFF));
    frame.push_back(static_cast<std::uint8_t>((crc >> 8) & 0xFF));
    frame.push_back(static_cast<std::uint8_t>(crc & 0xFF));
    return frame;
}

// Encode a message type + a small set of 12-bit fields (test helper) into a
// payload. The first 12 bits are the message type.
std::vector<std::uint8_t> typePayload(std::uint16_t messageType) {
    std::vector<std::uint8_t> p;
    p.push_back(static_cast<std::uint8_t>((messageType >> 4) & 0xFF));
    p.push_back(static_cast<std::uint8_t>(((messageType & 0x0F) << 4) & 0xF0));
    return p;
}

}  // namespace

RtkReferenceStation::RtkReferenceStation(int stationId, const Vec3& pos)
    : stationId_(stationId), position_(pos) {}

RtcmMessage RtkReferenceStation::stationPosition() const {
    std::vector<std::uint8_t> payload = typePayload(1005);
    // 12-bit station id, then 48-bit ECEF X, Y, Z (0.0001 m units).
    std::uint32_t sid = static_cast<std::uint32_t>(stationId_) & 0xFFF;
    payload.push_back(static_cast<std::uint8_t>((sid >> 4) & 0xFF));
    payload.push_back(static_cast<std::uint8_t>(((sid & 0x0F) << 4) & 0xF0));
    auto appendI48 = [&payload](double meters) {
        std::int64_t v = static_cast<std::int64_t>(std::llround(meters / 0.0001));
        for (int b = 5; b >= 0; --b)
            payload.push_back(static_cast<std::uint8_t>((v >> (b * 8)) & 0xFF));
    };
    appendI48(position_.x);
    appendI48(position_.y);
    appendI48(position_.z);
    // Append one byte to keep the payload a whole number of bytes (padded).
    payload.push_back(0);

    RtcmMessage m;
    m.type = RtcmMessageType::StationPos;
    m.frame = makeFrame(1005, payload);
    return m;
}

RtcmMessage RtkReferenceStation::observation(
    RtcmMessageType type, int satId, double pseudorangeM,
    double carrierPhaseCycles) const {
    std::uint16_t mt = static_cast<std::uint16_t>(type);
    std::vector<std::uint8_t> payload = typePayload(mt);
    std::uint32_t sid = static_cast<std::uint32_t>(satId) & 0xFFF;
    payload.push_back(static_cast<std::uint8_t>((sid >> 4) & 0xFF));
    payload.push_back(static_cast<std::uint8_t>(((sid & 0x0F) << 4) & 0xF0));
    // Pseudorange (m, scaled 0.0001) and carrier phase (cycles) as 32-bit.
    auto appendI32 = [&payload](std::int64_t v) {
        for (int b = 3; b >= 0; --b)
            payload.push_back(static_cast<std::uint8_t>((v >> (b * 8)) & 0xFF));
    };
    appendI32(static_cast<std::int64_t>(std::llround(pseudorangeM / 0.0001)));
    appendI32(static_cast<std::int64_t>(std::llround(carrierPhaseCycles * 100.0)));
    payload.push_back(0);

    RtcmMessage m;
    m.type = type;
    m.frame = makeFrame(mt, payload);
    return m;
}

std::string RtcmMessage::ascii() const {
    std::ostringstream ss;
    for (std::size_t i = 0; i < frame.size(); ++i) {
        if (i) ss << " ";
        ss << std::hex << (int)frame[i];
    }
    return ss.str();
}

std::string ntripEncapsulate(const RtcmMessage& msg, const std::string& mountPoint) {
    std::string s = "ICY 200 OK\r\nNtrip-Version: Ntrip/2.0\r\n\r\n";
    s += "MOUNT:" + mountPoint + "\r\n";
    s += "TYPE:RTCM3.3\r\n";
    s += "DATA:";
    s += msg.ascii();
    s += "\r\n";
    return s;
}

}  // namespace lodestar::scenario
