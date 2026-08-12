// core/scenario/nmea/NmeaSentence.h
// Low-level NMEA-0183 framing: checksum, sentence building/verification (Item 3.1).

#pragma once

#include <string>
#include <vector>

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class NmeaSentence {
public:
    // Build a full sentence "$<talker><type>,<fields>*<checksum>\r\n".
    static std::string build(const std::string& talker,
                             const std::string& type,
                             const std::vector<std::string>& fields);

    // XOR checksum of the body (chars between '$' and '*').
    static std::string checksum(const std::string& body);

    // Verify a full sentence's structure and checksum.
    static bool verify(const std::string& sentence);

    // Format a double with a fixed number of decimals and sign handling.
    static std::string fmtDouble(double v, int decimals, int minWidth = 0);
    static std::string fmtLatLon(double deg, bool isLat);  // ddmm.mmmm or dddmm.mmmm
    static std::string fmtUtcTime(double secOfDay);        // hhmmss.ss
    static std::string fmtDate(int year, int month, int day); // ddmmyy
};

}  // namespace lodestar::scenario
