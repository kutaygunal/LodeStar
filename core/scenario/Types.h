// core/scenario/Types.h
// Shared math, state, and configuration types for ScenarioForge (Phase 4).
//
// This header is the single source of truth for the shared types referenced by
// every item in the Phase 4 plan (R1). All math is self-contained: Vec3/Mat3 are
// small internal types (no Eigen/Boost, R7). Units are explicit in every field.
//
// Result<T> (R2): every public entry point returns a typed result that
// distinguishes success from failure-with-reason. RINEX parsers signal
// end-of-file via Result::eof() (isEof() == true). No silent NaN propagation.

#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace lodestar::scenario {

// ---------------------------------------------------------------------------
// Result<T> — success / failure-with-reason / EOF (R2)
// ---------------------------------------------------------------------------
template <typename T>
class Result {
public:
    static Result ok(T value) { return Result(std::move(value)); }
    static Result err(std::string message) { return Result(std::move(message)); }
    // Signals end-of-file for streaming parsers (RINEX). isEof() == true.
    static Result eof() { return Result(std::string("__eof__"), true); }

    bool isOk() const { return std::holds_alternative<T>(data_); }
    bool failed() const { return !isOk(); }
    bool isEof() const { return eof_; }

    T& value() { return std::get<T>(data_); }
    const T& value() const { return std::get<T>(data_); }
    const std::string& error() const { return std::get<std::string>(data_); }

private:
    explicit Result(T value) : data_(std::move(value)), eof_(false) {}
    explicit Result(std::string message, bool eof = false)
        : data_(std::move(message)), eof_(eof) {}

    std::variant<T, std::string> data_;
    bool eof_;
};

// ---------------------------------------------------------------------------
// Vec3 / Mat3 — small self-contained linear algebra (R7)
// ---------------------------------------------------------------------------
struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;

    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    double norm() const { return std::sqrt(x * x + y * y + z * z); }
    double normSq() const { return x * x + y * y + z * z; }
    Vec3 normalized() const {
        double n = norm();
        if (n <= 0.0) return Vec3(0, 0, 0);
        return Vec3(x / n, y / n, z / n);
    }
    bool isFinite() const {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(double s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(double s) const { return Vec3(x / s, y / s, z / s); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
};

inline double dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

// Row-major 3x3 matrix.
struct Mat3 {
    double m[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

    static Mat3 identity() {
        Mat3 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = 1.0;
        return r;
    }
    // Rotation about Z axis by angle (radians).
    static Mat3 rotZ(double a) {
        Mat3 r;
        double c = std::cos(a), s = std::sin(a);
        r.m[0][0] = c;  r.m[0][1] = -s;
        r.m[1][0] = s;  r.m[1][1] = c;
        r.m[2][2] = 1.0;
        return r;
    }
    Vec3 operator*(const Vec3& v) const {
        return Vec3(m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
                    m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
                    m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z);
    }
    Mat3 operator*(const Mat3& o) const {
        Mat3 r;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                double s = 0.0;
                for (int k = 0; k < 3; ++k) s += m[i][k] * o.m[k][j];
                r.m[i][j] = s;
            }
        return r;
    }
    Mat3 transposed() const {
        Mat3 r;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) r.m[i][j] = m[j][i];
        return r;
    }
};

// ---------------------------------------------------------------------------
// Time types
// ---------------------------------------------------------------------------
struct GpsTime {
    int week = 0;        // GPS week number
    double sow = 0.0;    // seconds of week [0, 604800)
};

struct JulianDate {
    double jd = 0.0;     // Julian Date
    double mjd = 0.0;    // Modified Julian Date
};

// ---------------------------------------------------------------------------
// Orbit / state types
// ---------------------------------------------------------------------------
struct SvState {
    Vec3 posEcef;        // m
    Vec3 velEcef;        // m/s
    double clockBias = 0.0;   // s
    double clockDrift = 0.0; // s/s
};

struct TemeState {
    Vec3 posTeme;        // km (SGP4 native units)
    Vec3 velTeme;        // km/s
};

// IS-GPS-200 / Galileo F/NAV broadcast ephemeris fields.
struct BroadcastEphemeris {
    double toe = 0.0;    // time of ephemeris, s (GPS seconds of week)
    double sqrtA = 0.0;
    double e = 0.0;
    double i0 = 0.0;
    double omega0 = 0.0; // longitude of ascending node at toe, rad
    double argp = 0.0;   // argument of perigee, rad
    double M0 = 0.0;     // mean anomaly at toe, rad
    double deltaN = 0.0; // mean motion correction, rad/s
    double idot = 0.0;   // rate of inclination, rad/s
    double omegadot = 0.0; // rate of ascending node, rad/s
    double cuc = 0.0, cus = 0.0, crc = 0.0, crs = 0.0, cic = 0.0, cis = 0.0;
    double af0 = 0.0, af1 = 0.0, af2 = 0.0; // clock polynomial
    int iodc = 0;        // issue of data
    int prn = 0;         // satellite PRN (0 if unknown)
};

// ---------------------------------------------------------------------------
// PVT / measurement types
// ---------------------------------------------------------------------------
struct PvtResult {
    Vec3 posEcef;        // m
    Vec3 velEcef;        // m/s
    double clockBias = 0.0;   // s
    double clockDrift = 0.0;  // s/s
    double pdop = 0.0;
    double hdop = 0.0;
    double vdop = 0.0;
    int numSats = 0;
    bool valid = false;
};

struct AtmosphericCorrections {
    double iono = 0.0;   // m
    double tropo = 0.0;  // m
    double rel = 0.0;    // m (relativistic)
};

// ---------------------------------------------------------------------------
// Geometry / constellation types
// ---------------------------------------------------------------------------
struct SatelliteView {
    int prn = 0;
    double elevationRad = 0.0;
    double azimuthRad = 0.0;
    double slantRange = 0.0;  // m
    SvState state;
    bool visible = false;
};

// ---------------------------------------------------------------------------
// NMEA types
// ---------------------------------------------------------------------------
struct NmeaConfig {
    std::string talker = "GP";
    int fixQuality = 1;      // 0=no fix,1=GPS,2=DGPS,...
    std::string datum = "W84";
    double geoidSeparation = 0.0; // m
};

// ---------------------------------------------------------------------------
// Error-model types
// ---------------------------------------------------------------------------
struct LongTermCorrection {
    Vec3 posOffset;        // m (ECEF)
    Vec3 velOffset;        // m/s
    double clockOffset = 0.0; // s
    double clockDrift = 0.0;  // s/s
    double iodf = 0.0;     // issue of data fast
    double iodp = 0.0;     // issue of data prev
    double t0 = 0.0;       // time of applicability, s
};

struct IgpData {
    double latDeg = 0.0;
    double lonDeg = 0.0;
    double delay = 0.0;    // m
    double give = 0.0;     // m (GIVE)
    double t = 0.0;        // time of applicability, s
};

struct ProtectionLevels {
    double hpl = 0.0;      // m
    double vpl = 0.0;      // m
    bool valid = false;
};

// ---------------------------------------------------------------------------
// RINEX types
// ---------------------------------------------------------------------------
struct RinexNavHeader {
    std::string version = "3.00";
    std::string fileType = "N";
    std::string system = "G";
    std::string program;
    std::string runBy;
    std::string date;
    double ionoAlpha[4] = {0, 0, 0, 0};
    double ionoBeta[4] = {0, 0, 0, 0};
    double utcA0 = 0.0, utcA1 = 0.0, utcTot = 0.0, utcWt = 0.0;
};

struct RinexObsHeader {
    std::string version = "3.00";
    std::string fileType = "O";
    std::string system = "G";
    std::string program;
    std::string runBy;
    std::string date;
    std::vector<std::string> obsTypes;  // e.g. C1C, L1C, D1C, S1C
};

struct ObsRecord {
    int prn = 0;
    double pseudorange = 0.0;
    double carrierPhase = 0.0;
    double doppler = 0.0;
    double snr = 0.0;
    bool hasPseudorange = false;
    bool hasCarrierPhase = false;
    bool hasDoppler = false;
    bool hasSnr = false;
};

struct ObsEpoch {
    GpsTime time;
    std::vector<ObsRecord> records;
};

// ---------------------------------------------------------------------------
// Scenario facade types
// ---------------------------------------------------------------------------
struct ScenarioConfig {
    double elevationMaskRad = 0.0;   // default 0 (no mask)
    bool enableIonosphere = true;
    bool enableTroposphere = true;
    bool enableClock = true;
    bool enableSbas = true;
    NmeaConfig nmea;
};

struct ScenarioEpoch {
    std::vector<SatelliteView> views;
    Result<PvtResult> pvt = Result<PvtResult>::err("not computed");
    std::vector<std::string> nmea;   // GGA/RMC/GSA/GSV/ZDA
    ProtectionLevels protection;
};

}  // namespace lodestar::scenario
