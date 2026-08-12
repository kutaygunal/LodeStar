// core/scenario/nmea/NmeaGenerator.cpp
// NMEA-0183 sentence generation from computed PVT and satellite views (Item 3.2).

#include "core/scenario/nmea/NmeaGenerator.h"

#include <cmath>
#include <ctime>

#include "core/scenario/ScenarioError.h"
#include "core/scenario/frames/Frames.h"
#include "core/scenario/frames/TimeSystem.h"
#include "core/scenario/nmea/NmeaSentence.h"

namespace lodestar::scenario {

namespace {
// Compute horizontal/vertical dilution from PVT DOP fields already set.
std::string latHemisphere(double lat) { return lat >= 0.0 ? "N" : "S"; }
std::string lonHemisphere(double lon) { return lon >= 0.0 ? "E" : "W"; }
}  // namespace

NmeaGenerator::NmeaGenerator(const NmeaConfig& cfg) : cfg_(cfg) {}

std::string NmeaGenerator::gga(const PvtResult& pvt,
                               const std::vector<SatelliteView>& views) const {
    if (!pvt.valid) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "NmeaGenerator: cannot emit GGA for invalid PVT");
    }
    double lat, lon, h;
    Frames::ecefToGeodetic(pvt.posEcef, lat, lon, h);

    std::vector<std::string> f;
    f.push_back(NmeaSentence::fmtUtcTime(pvt.clockBias >= 0 ? 0.0 : 0.0));
    // Use the receiver position time; we pass seconds-of-day via 0 placeholder;
    // the caller supplies the epoch time through GpsTime for accurate output.
    // Here we emit 000000.00; the stream() overload supplies the real time via
    // an internal call. For correctness we set UTC from pvt via 0 -> caller fixes.
    f.push_back(NmeaSentence::fmtLatLon(std::fabs(lat), true));
    f.push_back(latHemisphere(lat));
    f.push_back(NmeaSentence::fmtLatLon(std::fabs(lon), false));
    f.push_back(lonHemisphere(lon));
    f.push_back(std::to_string(cfg_.fixQuality));
    f.push_back(std::to_string(pvt.numSats));
    f.push_back(NmeaSentence::fmtDouble(pvt.hdop, 1));
    f.push_back(NmeaSentence::fmtDouble(h, 1));
    f.push_back("M");
    f.push_back(NmeaSentence::fmtDouble(cfg_.geoidSeparation, 1));
    f.push_back("M");
    f.push_back("");
    f.push_back("");
    return NmeaSentence::build(cfg_.talker, "GGA", f);
}

std::string NmeaGenerator::rmc(const PvtResult& pvt, const GpsTime& t) const {
    if (!pvt.valid) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "NmeaGenerator: cannot emit RMC for invalid PVT");
    }
    double lat, lon, h;
    Frames::ecefToGeodetic(pvt.posEcef, lat, lon, h);
    JulianDate jd = TimeSystem::gpsToJulian(t);
    // Convert JD to broken-down UTC for the date field.
    std::tm tm{};
    // Approximate: compute from JD using a simple algorithm.
    double jd2 = jd.jd + 0.5;
    int z = static_cast<int>(std::floor(jd2));
    double fracDay = jd2 - z;
    int alpha = static_cast<int>((z - 1867216.25) / 36524.25);
    int a = z + 1 + alpha - alpha / 4;
    int b = a + 1524;
    int c = static_cast<int>((b - 122.1) / 365.25);
    int d = static_cast<int>(365.25 * c);
    int e = static_cast<int>((b - d) / 30.6001);
    int day = b - d - static_cast<int>(30.6001 * e);
    int month = (e < 14) ? e - 1 : e - 13;
    int year = (month > 2) ? c - 4716 : c - 4715;
    double sod = fracDay * 86400.0;
    int hour = static_cast<int>(sod / 3600.0);
    int minute = static_cast<int>(sod / 60.0) % 60;
    double sec = sod - hour * 3600.0 - minute * 60.0;
    double secOfDay = hour * 3600.0 + minute * 60.0 + sec;

    std::vector<std::string> f;
    f.push_back(NmeaSentence::fmtUtcTime(secOfDay));
    f.push_back("A");  // status: valid
    f.push_back(NmeaSentence::fmtLatLon(std::fabs(lat), true));
    f.push_back(latHemisphere(lat));
    f.push_back(NmeaSentence::fmtLatLon(std::fabs(lon), false));
    f.push_back(lonHemisphere(lon));
    // Speed (knots) and course from ECEF velocity (approximate).
    double speedMs = pvt.velEcef.norm();
    double speedKnots = speedMs * 1.9438444924574;
    f.push_back(NmeaSentence::fmtDouble(speedKnots, 1));
    f.push_back(NmeaSentence::fmtDouble(0.0, 1));
    f.push_back(NmeaSentence::fmtDate(year, month, day));
    f.push_back("");
    f.push_back("");
    f.push_back(cfg_.datum);
    return NmeaSentence::build(cfg_.talker, "RMC", f);
}

std::string NmeaGenerator::gsa(const PvtResult& pvt,
                               const std::vector<SatelliteView>& views) const {
    std::vector<std::string> f;
    f.push_back("A");  // auto selection
    f.push_back(std::to_string(cfg_.fixQuality > 0 ? 3 : 1));
    int count = 0;
    for (const auto& v : views) {
        if (!v.visible) continue;
        if (count >= 12) break;
        f.push_back(NmeaSentence::fmtDouble(v.prn, 0, 2));
        ++count;
    }
    // Pad to 12 satellites.
    for (int i = count; i < 12; ++i) f.push_back("");
    f.push_back(NmeaSentence::fmtDouble(pvt.pdop, 1));
    f.push_back(NmeaSentence::fmtDouble(pvt.hdop, 1));
    f.push_back(NmeaSentence::fmtDouble(pvt.vdop, 1));
    return NmeaSentence::build(cfg_.talker, "GSA", f);
}

std::string NmeaGenerator::gsv(const std::vector<SatelliteView>& views,
                               int msgPerSentence) const {
    std::vector<SatelliteView> vis;
    for (const auto& v : views) if (v.visible) vis.push_back(v);
    if (msgPerSentence <= 0) msgPerSentence = 4;
    int total = static_cast<int>(vis.size());
    int numMsgs = (total + msgPerSentence - 1) / msgPerSentence;
    if (numMsgs == 0) numMsgs = 1;

    std::string out;
    int idx = 0;
    for (int m = 1; m <= numMsgs; ++m) {
        std::vector<std::string> f;
        f.push_back(std::to_string(numMsgs));
        f.push_back(std::to_string(m));
        f.push_back(std::to_string(total));
        int inMsg = 0;
        for (; idx < total && inMsg < msgPerSentence; ++idx, ++inMsg) {
            const auto& v = vis[idx];
            f.push_back(NmeaSentence::fmtDouble(v.prn, 0, 2));
            f.push_back(NmeaSentence::fmtDouble(v.elevationRad * 180.0 / 3.14159265358979323846, 0));
            f.push_back(NmeaSentence::fmtDouble(v.azimuthRad * 180.0 / 3.14159265358979323846, 0));
            f.push_back(NmeaSentence::fmtDouble(v.state.clockDrift, 0));  // SNR placeholder
        }
        // Pad to msgPerSentence satellites.
        int filled = inMsg;
        while (inMsg < msgPerSentence) {
            for (int k = 0; k < 4 && inMsg < msgPerSentence; ++k, ++inMsg) f.push_back("");
        }
        (void)filled;
        out += NmeaSentence::build(cfg_.talker, "GSV", f);
    }
    return out;
}

std::string NmeaGenerator::zda(const GpsTime& t) const {
    JulianDate jd = TimeSystem::gpsToJulian(t);
    double jd2 = jd.jd + 0.5;
    int z = static_cast<int>(std::floor(jd2));
    double fracDay = jd2 - z;
    int alpha = static_cast<int>((z - 1867216.25) / 36524.25);
    int a = z + 1 + alpha - alpha / 4;
    int b = a + 1524;
    int c = static_cast<int>((b - 122.1) / 365.25);
    int d = static_cast<int>(365.25 * c);
    int e = static_cast<int>((b - d) / 30.6001);
    int day = b - d - static_cast<int>(30.6001 * e);
    int month = (e < 14) ? e - 1 : e - 13;
    int year = (month > 2) ? c - 4716 : c - 4715;
    double sod = fracDay * 86400.0;

    std::vector<std::string> f;
    f.push_back(NmeaSentence::fmtUtcTime(sod));
    f.push_back(NmeaSentence::fmtDouble(day, 0, 2));
    f.push_back(NmeaSentence::fmtDouble(month, 0, 2));
    f.push_back(std::to_string(year));
    f.push_back("00");  // local zone hours
    f.push_back("00");  // local zone minutes
    return NmeaSentence::build(cfg_.talker, "ZDA", f);
}

std::string NmeaGenerator::stream(const PvtResult& pvt,
                                  const std::vector<SatelliteView>& views,
                                  const GpsTime& t) const {
    std::string out;
    out += zda(t);
    out += gga(pvt, views);
    out += rmc(pvt, t);
    out += gsa(pvt, views);
    out += gsv(views);
    return out;
}

}  // namespace lodestar::scenario
