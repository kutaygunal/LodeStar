// core/scenario/nmea/NmeaSentence.cpp
// NMEA-0183 sentence building, checksum, and field formatting (Item 3.1).

#include "core/scenario/nmea/NmeaSentence.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/scenario/ScenarioError.h"

namespace lodestar::scenario {

std::string NmeaSentence::checksum(const std::string& body) {
    unsigned char csum = 0;
    for (char c : body) csum ^= static_cast<unsigned char>(c);
    char buf[3];
    std::snprintf(buf, sizeof(buf), "%02X", csum);
    return std::string(buf);
}

std::string NmeaSentence::build(const std::string& talker,
                                const std::string& type,
                                const std::vector<std::string>& fields) {
    if (talker.size() != 2) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "NmeaSentence: talker id must be 2 chars");
    }
    if (type.size() != 3) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "NmeaSentence: sentence type must be 3 chars");
    }
    std::string body = talker + type;
    for (const auto& f : fields) {
        body += "," + f;
    }
    return "$" + body + "*" + checksum(body) + "\r\n";
}

bool NmeaSentence::verify(const std::string& sentence) {
    if (sentence.size() < 7) return false;
    if (sentence[0] != '$') return false;
    size_t star = sentence.find('*');
    if (star == std::string::npos || star + 3 > sentence.size()) return false;
    std::string body = sentence.substr(1, star - 1);
    std::string expected = sentence.substr(star + 1, 2);
    std::string actual = checksum(body);
    if (actual != expected) return false;
    // Optional CR/LF terminator.
    std::string tail = sentence.substr(star + 3);
    if (!tail.empty() && tail != "\r" && tail != "\n" && tail != "\r\n") return false;
    return true;
}

std::string NmeaSentence::fmtDouble(double v, int decimals, int minWidth) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%*.*f", minWidth, decimals, v);
    return std::string(buf);
}

std::string NmeaSentence::fmtLatLon(double deg, bool isLat) {
    double abs = std::fabs(deg);
    int whole = static_cast<int>(abs);
    double minutes = (abs - whole) * 60.0;
    char buf[32];
    if (isLat) {
        std::snprintf(buf, sizeof(buf), "%02d%07.4f", whole, minutes);
    } else {
        std::snprintf(buf, sizeof(buf), "%03d%07.4f", whole, minutes);
    }
    return std::string(buf);
}

std::string NmeaSentence::fmtUtcTime(double secOfDay) {
    int h = static_cast<int>(secOfDay / 3600.0) % 24;
    int m = static_cast<int>(secOfDay / 60.0) % 60;
    double s = secOfDay - static_cast<int>(secOfDay / 60.0) * 60.0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d%02d%05.2f", h, m, s);
    return std::string(buf);
}

std::string NmeaSentence::fmtDate(int year, int month, int day) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d%02d%02d", day, month, year % 100);
    return std::string(buf);
}

}  // namespace lodestar::scenario
