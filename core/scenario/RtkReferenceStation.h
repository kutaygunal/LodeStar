// core/scenario/RtkReferenceStation.h
// Gap-Fill ScenarioForge 5.2: RTK virtual reference station.
//
// An RTK base-station model that emits RTCM 3.3 corrections (real-time
// kinematic), streamable over LAN via the NTRIP transport. Wires to the
// ScenarioForge position configuration. Deterministic message generation for
// unit tests against reference message sets.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/scenario/Types.h"

namespace lodestar::scenario {

// RTCM 3.3 message types used by the VRS.
enum class RtcmMessageType {
    GpsEph        = 1019,
    GlonassEph    = 1020,
    StationPos    = 1005,
    GpsObs        = 1074,  // MSM4 GPS
    GlonassObs    = 1084,  // MSM4 GLONASS
    NavCorr       = 1044,  // QZSS
};

// One RTCM message: type + the raw frame bytes.
struct RtcmMessage {
    RtcmMessageType type;
    std::vector<std::uint8_t> frame;  // complete RTCM 3.3 frame (with CRC)
    std::string ascii() const;        // printable hex for logs/verification
};

// An RTK reference station.
class RtkReferenceStation {
public:
    // stationId is the RTCM reference station id (1..4095).
    explicit RtkReferenceStation(int stationId, const Vec3& positionEcef);

    // Emit an RTCM 3.3 station-position message (type 1005), 48-bit ECEF
    // coordinates in the standard units.
    RtcmMessage stationPosition() const;

    // Emit an MSM4 observation message for a satellite at the given carrier
    // phase count (deterministic test input). type selects the constellation.
    RtcmMessage observation(RtcmMessageType type, int satId,
                            double pseudorangeM, double carrierPhaseCycles) const;

    // The station's position (m, ECEF).
    Vec3 position() const { return position_; }

private:
    int stationId_;
    Vec3 position_;
};

// NTRIP transport: encodes RTCM messages into a minimal NTRIP streaming
// (SOURCETABLE / GGA / data) for LAN streaming. Deterministic.
std::string ntripEncapsulate(const RtcmMessage& msg, const std::string& mountPoint);

}  // namespace lodestar::scenario
