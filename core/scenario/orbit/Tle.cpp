// core/scenario/orbit/Tle.cpp
// TLE parsing with checksum validation (Item 1.3).

#include "core/scenario/orbit/Tle.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/scenario/ScenarioError.h"

namespace lodestar::scenario {

namespace {
// Compute the TLE line checksum: sum of digits (mod 10) over the first 68
// characters (the checksum digit at position 68 is excluded), '-' counts as 1.
int lineChecksum(const std::string& line) {
    int sum = 0;
    int n = static_cast<int>(line.size());
    if (n > 68) n = 68;
    for (int i = 0; i < n; ++i) {
        char c = line[i];
        if (c >= '0' && c <= '9') sum += (c - '0');
        else if (c == '-') sum += 1;
    }
    return sum % 10;
}

double parseImpliedDecimal(const std::string& s) {
    // Format: sign, digit, '.', digits, exponent (e.g. "-11606-4").
    if (s.size() < 5) return 0.0;
    char sign = s[0];
    double mantissa = 0.0;
    int i = 1;
    if (i < static_cast<int>(s.size()) && s[i] == '.') {
        // ".12345-4" style
        mantissa = std::atof(s.substr(1, 5).c_str());
        i = 6;
    } else {
        mantissa = std::atof(s.substr(1, 5).c_str());
        i = 6;
    }
    int exp = 0;
    if (i < static_cast<int>(s.size())) {
        exp = std::atoi(s.substr(i).c_str());
    }
    double v = mantissa * std::pow(10.0, exp);
    if (sign == '-') v = -v;
    return v;
}
}  // namespace

Result<Tle> Tle::parse(const std::string& name,
                       const std::string& line1,
                       const std::string& line2) {
    if (line1.size() < 69 || line2.size() < 69) {
        return Result<Tle>::err("Tle::parse: line too short");
    }
    if (line1[0] != '1' || line2[0] != '2') {
        return Result<Tle>::err("Tle::parse: expected line 1 to start with '1' and line 2 with '2'");
    }
    if (lineChecksum(line1) != (line1[68] - '0')) {
        return Result<Tle>::err("Tle::parse: line 1 checksum mismatch");
    }
    if (lineChecksum(line2) != (line2[68] - '0')) {
        return Result<Tle>::err("Tle::parse: line 2 checksum mismatch");
    }

    Tle t;
    t.name_ = name;

    // Line 1 fields.
    t.catNum_ = std::atoi(line1.substr(2, 5).c_str());
    t.epochYear_ = std::atoi(line1.substr(18, 2).c_str());
    if (t.epochYear_ < 57) t.epochYear_ += 2000;
    else t.epochYear_ += 1900;
    t.epochDay_ = std::atof(line1.substr(20, 12).c_str());
    t.bstar_ = parseImpliedDecimal(line1.substr(53, 8));

    // Line 2 fields.
    t.inclRad_ = std::atof(line2.substr(8, 8).c_str()) * 3.14159265358979323846 / 180.0;
    t.raanRad_ = std::atof(line2.substr(17, 8).c_str()) * 3.14159265358979323846 / 180.0;
    t.ecc_ = std::atof(("0." + line2.substr(26, 7)).c_str());
    t.argpRad_ = std::atof(line2.substr(34, 8).c_str()) * 3.14159265358979323846 / 180.0;
    t.meanAnomalyRad_ = std::atof(line2.substr(43, 8).c_str()) * 3.14159265358979323846 / 180.0;
    t.meanMotion_ = std::atof(line2.substr(52, 11).c_str());

    if (t.ecc_ < 0.0 || t.ecc_ >= 1.0) {
        return Result<Tle>::err("Tle::parse: eccentricity out of range");
    }
    if (t.meanMotion_ <= 0.0 || t.meanMotion_ > 20.0) {
        return Result<Tle>::err("Tle::parse: mean motion out of range");
    }

    // Epoch Julian date: Jan 0.0 of epoch year + day-of-year.
    int y = t.epochYear_;
    int a = y / 100;
    int b = 2 - a + a / 4;
    double jdJan0 = std::floor(365.25 * (y + 4716)) +
                    std::floor(30.6001 * (1 + 1)) + 0 + b - 1524.5;
    t.epochJd_ = jdJan0 + t.epochDay_;

    return Result<Tle>::ok(t);
}

}  // namespace lodestar::scenario
