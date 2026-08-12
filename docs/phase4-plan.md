# Phase 4 — ScenarioForge (Real GNSS Math) — Detailed Plan

> **Owner:** planner · **Status:** PLANNED · **Module:** `core/scenario`
> **Standard:** COMMERCIAL GRADE — production C++, real GNSS math, proper error handling.
> **Verification:** build + smoke run (no test agents in this loop, per `docs/working-rules.md`).

This document is the single source of truth for Phase 4. The scrum-master itemizes these
items into tickets; the engineer implements them in dependency order. **No code is written
in this phase by the planner** — this is the plan only.

---

## 0. Scope & guiding principles

- All math lives in `core/scenario` (ScenarioForge), a CMake static library target
  `lodestar_scenario` (already wired in `core/CMakeLists.txt`; currently `stub.cpp`).
- Deterministic, real-time-capable, thread-safe where the API is shared.
- Every public entry point returns a typed result that distinguishes **success** from
  **failure with a reason** (no silent NaN propagation).
- All physical constants are named, documented, and sourced (WGS-84, IERS, IS-GPS-200,
  RTCA DO-229, ICD-GPS-200).
- Units are explicit in every API (meters, seconds, radians, Hz). No implicit unit mixing.
- The module must compile with the existing `lodestar_add_module` helper (C++17, `/W4`
  on MSVC, `-Wall -Wextra -Wpedantic` on GCC/Clang) and link into `lodestar_core`.

### Suggested file layout under `core/scenario/`

```
core/scenario/
├── CMakeLists.txt            # (replace stub registration in core/CMakeLists.txt)
├── stub.cpp                  # keep module_version() for backward compat
├── frames/                   # Item 1.1
│   ├── TimeSystem.h/.cpp     # GPS time, UTC, Julian date, leap seconds
│   ├── Frames.h/.cpp         # ECEF <-> ECI rotation, WGS-84 datum
│   └── Geometry.h/.cpp       # ground-station/satellite geometry, elevation/azimuth
├── orbit/                    # Item 1.2, 1.3
│   ├── Keplerian.h/.cpp      # two-body propagation
│   ├── Sgp4.h/.cpp           # SGP4/SDP4 (Vallado reference implementation)
│   └── Tle.h/.cpp            # TLE parsing
├── rinex/                    # Item 2
│   ├── RinexNav.h/.cpp       # navigation (broadcast ephemeris) parser
│   ├── RinexObs.h/.cpp       # observation parser
│   └── RinexError.h/.cpp
├── nmea/                     # Item 3
│   ├── NmeaSentence.h/.cpp   # sentence builder + checksum
│   └── NmeaGenerator.h/.cpp  # GGA/RMC/GSA/GSV/ZDA emitters
├── pvt/                      # Item 4
│   ├── Pseudorange.h/.cpp
│   └── Doppler.h/.cpp
├── errors/                   # Item 5
│   ├── ClockModel.h/.cpp
│   ├── Ionosphere.h/.cpp     # Klobuchar
│   ├── Troposphere.h/.cpp    # Saastamoinen / Hopfield
│   └── ErrorModelConfig.h/.cpp
├── sbas/                     # Item 6
│   ├── SbasMessage.h/.cpp    # message 1-4, 6, 9, 10, 12, 17, 18, 25, 26, 27, 28
│   ├── SbasCorrections.h/.cpp
│   ├── Integrity.h/.cpp       # UDRE/GIVE, HPL/VPL
│   └── SbasConfig.h/.cpp
├── Scenario.h/.cpp           # top-level facade tying items together
└── ScenarioError.h/.cpp      # shared error type
```

---

## Item 1 — Orbit propagation

**Purpose:** Compute satellite positions (and velocities) in ECEF at arbitrary times so
downstream pseudorange/Doppler and geometry can be computed. Support both a fast
Keplerian two-body model (for GPS broadcast ephemeris) and SGP4/SDP4 (for TLE-based LEO
constellations). Provide the frame/time/geometry utilities everything else depends on.

**Dependencies:** none (foundation). **Prerequisite for:** Items 4, 5, 6.

### 1.1 Frame & time utilities

**Purpose:** Single source of truth for time systems and coordinate frames used by every
other item.

**Classes:**
- `TimeSystem` — GPS time, UTC, Julian Date (JD), Modified Julian Date (MJD), GPS week +
  seconds-of-week. Conversions with leap-second table.
- `Frames` — ECEF↔ECI rotation (GMST/GAST via IAU-1982/2000), WGS-84 datum constants.
- `Geometry` — ground-station/satellite geometry: elevation, azimuth, slant range,
  topocentric ENU conversion.

**Public API:**
```cpp
namespace lodestar::scenario {
struct GpsTime { int week; double sow; };            // seconds of week
struct JulianDate { double jd; double mjd; };
class TimeSystem {
public:
    static JulianDate gpsToJulian(const GpsTime& t);
    static GpsTime     julianToGps(const JulianDate& jd);
    static double      gpsToUtcOffsetSec(const GpsTime& t); // leap seconds
    static GpsTime     utcToGps(const std::tm& utc);
};
class Frames {
public:
    static constexpr double WGS84_A  = 6378137.0;      // semi-major axis, m
    static constexpr double WGS84_F  = 1.0/298.257223563; // flattening
    static constexpr double WGS84_E2 = ...;            // first eccentricity squared
    static constexpr double GM      = 3.986004418e14;  // Earth grav const, m^3/s^2
    static constexpr double OMEGA_E = 7.2921151467e-5; // Earth rotation rate, rad/s
    static constexpr double C       = 299792458.0;     // speed of light, m/s
    static Mat3 ecefToEciRotation(const JulianDate& jd); // GMST-based
    static Vec3 ecefToEci(const Vec3& ecef, const JulianDate& jd);
    static Vec3 eciToEcef(const Vec3& eci, const JulianDate& jd);
    static Vec3 geodeticToEcef(double lat, double lon, double h); // WGS-84
    static void  ecefToGeodetic(const Vec3& ecef, double& lat, double& lon, double& h);
};
class Geometry {
public:
    static double elevationRad(const Vec3& rxEcef, const Vec3& svEcef);
    static double azimuthRad(const Vec3& rxEcef, const Vec3& svEcef);
    static double slantRange(const Vec3& rxEcef, const Vec3& svEcef);
    static Vec3   topocentricEnu(const Vec3& rxEcef, const Vec3& svEcef);
};
}
```

**Real math / algorithms:**
- WGS-84 geodetic↔ECEF (Bowring / standard closed-form inverse).
- GMST from Julian date (IAU-1982 formula); optional GAST with equation of equinoxes.
- Elevation/azimuth via ENU rotation from the receiver geodetic frame.

**Error handling:** range-check inputs (latitude/longitude bounds, time sanity); throw
`ScenarioError` on invalid datum or out-of-range time. No silent NaN.

**Acceptance criteria:** a smoke path computes a known GPS epoch's GMST and converts a
known ECEF point to geodetic and back, matching reference values to within 1e-6 rad / 1e-3 m.

### 1.2 Keplerian two-body propagator (broadcast ephemeris)

**Purpose:** Propagate GPS/Galileo broadcast ephemeris (Keplerian elements) to satellite
ECEF position/velocity at a given time. This is the standard GPS algorithm (IS-GPS-200).

**Classes:**
- `Keplerian` — holds the 16 broadcast ephemeris parameters; `propagate()`.

**Public API:**
```cpp
struct BroadcastEphemeris {   // IS-GPS-200 / Galileo F/NAV fields
    double toe;               // time of ephemeris, s
    double sqrtA, e, i0, omega0, argp, M0, deltaN, idot, omegadot;
    double cuc, cus, crc, crs, cic, cis;
    double af0, af1, af2;      // clock polynomial
    int    iodc;              // issue of data
};
class Keplerian {
public:
    explicit Keplerian(const BroadcastEphemeris& eph);
    // Returns ECEF position (m) and velocity (m/s) at time t (GPS seconds).
    Result<SvState> propagate(double t) const;
};
struct SvState { Vec3 posEcef; Vec3 velEcef; double clockBias; double clockDrift; };
```

**Real math / algorithms:**
- Kepler's equation solved by Newton–Raphson iteration (converge to < 1e-12 rad).
- IS-GPS-200 Section 20.3.3.4.3: mean anomaly, eccentric anomaly, true anomaly,
  argument of latitude, harmonic corrections (Cuc/Cus/Crc/Crs/Cic/Cis), radius, inclination
  correction, corrected longitude of ascending node (with Earth rotation), ECEF position.
- Velocity via time derivatives of the same equations.
- Clock correction: `dt = af0 + af1*(t-toe) + af2*(t-toe)^2` plus relativistic term.

**Error handling:** reject invalid elements (eccentricity ≥ 1, sqrtA ≤ 0, |e| bounds);
detect non-convergence of Kepler iteration and return an error result.

**Acceptance criteria:** smoke path propagates a known GPS PRN ephemeris to `t=toe` and
compares ECEF position to a published reference within 1 m; clock bias matches reference.

### 1.3 SGP4 / SDP4 propagator (TLE-based LEO)

**Purpose:** Propagate TLE orbital elements for LEO satellites using the standard SGP4/SDP4
model (Vallado's reference implementation, AIAA 2006-6753). Needed for LEO constellations
and for scenarios where broadcast ephemeris is unavailable.

**Classes:**
- `Tle` — parse a two-line element set (name + 2 lines).
- `Sgp4` — Vallado SGP4/SDP4 propagator.

**Public API:**
```cpp
class Tle {
public:
    static Result<Tle> parse(const std::string& name,
                             const std::string& line1,
                             const std::string& line2);
    double epochJulian() const;   // TLE epoch
    // ... accessors for elements
};
class Sgp4 {
public:
    explicit Sgp4(const Tle& tle);
    // Propagate to minutes-since-epoch; returns TEME position/velocity.
    Result<TemeState> propagate(double minutesSinceEpoch) const;
};
struct TemeState { Vec3 posTeme; Vec3 velTeme; };
```

**Real math / algorithms:**
- Vallado SGP4 (near-Earth) and SDP4 (deep-space) with the 2006 corrections
  (AIAA 2006-6753). TEME frame output; convert TEME→ECEF via GMST for downstream use.
- Handle deep-space resonance and lunar/solar perturbations in SDP4.

**Error handling:** validate TLE checksums and field ranges; reject malformed TLEs with a
descriptive error; detect propagation divergence.

**Acceptance criteria:** smoke path propagates a published TLE (e.g. ISS) to a known epoch
and matches the reference TEME position within the SGP4 tolerance (typically < 1 km for
short arcs); malformed TLE returns a clear error.

### 1.4 Satellite geometry (constellation view)

**Purpose:** Aggregate per-satellite states into a constellation snapshot and compute
visibility from a ground station (elevation mask, azimuth, range).

**Classes:**
- `Constellation` — holds a set of satellites (PRN → propagator + ephemeris).
- `SatelliteView` — per-satellite geometry result.

**Public API:**
```cpp
class Constellation {
public:
    void addGps(const BroadcastEphemeris& eph, int prn);
    void addTle(const Tle& tle, int prn);
    Result<std::vector<SatelliteView>> computeViews(
        const Vec3& rxEcef, const GpsTime& t, double elevationMaskRad) const;
};
struct SatelliteView {
    int prn; double elevationRad; double azimuthRad;
    double slantRange; SvState state; bool visible;
};
```

**Real math / algorithms:** combine Items 1.1–1.3; apply elevation mask; sort by elevation.

**Error handling:** propagate per-satellite errors without aborting the whole constellation
(collect failures, report count).

**Acceptance criteria:** smoke path builds a small GPS constellation, computes views from a
known station, and confirms expected visible/occluded satellites.

---

## Item 2 — RINEX parser

**Purpose:** Parse RINEX 3.x navigation (broadcast ephemeris) and observation files so
real recorded data can drive scenarios and be cross-checked against computed values.
Robust tokenization, header/record handling, and precise error reporting.

**Dependencies:** Item 1.1 (time/frames) for epoch handling. **Prerequisite for:** Items 4, 5.

### 2.1 RINEX navigation parser

**Purpose:** Parse RINEX 3.x NAV files into `BroadcastEphemeris` records (Item 1.2).

**Classes:**
- `RinexNavParser` — streaming parser producing ephemeris records.

**Public API:**
```cpp
class RinexNavParser {
public:
    explicit RinexNavParser(std::istream& in);
    Result<RinexNavHeader> readHeader();
    Result<BroadcastEphemeris> nextEphemeris(); // EOF => Result with eof flag
    const std::vector<std::string>& warnings() const;
};
```

**Real math / algorithms:** RINEX 3.x fixed-width field parsing (columns), header
`END OF HEADER` detection, per-epoch record grouping, GPS/Galileo/GLONASS/BDS handling.

**Error handling:** line-length validation, field parse failures with line numbers,
missing required header fields, truncated records. Collect warnings, fail hard on
structural errors.

**Acceptance criteria:** smoke path parses a small RINEX 3 NAV file and produces
`BroadcastEphemeris` records whose `toe`/`sqrtA` match the file; a deliberately corrupted
file yields a descriptive error with line number.

### 2.2 RINEX observation parser

**Purpose:** Parse RINEX 3.x OBS files (pseudorange, carrier phase, Doppler, SNR) for
cross-checking computed pseudoranges.

**Classes:**
- `RinexObsParser` — streaming parser producing observation epochs.

**Public API:**
```cpp
class RinexObsParser {
public:
    explicit RinexObsParser(std::istream& in);
    Result<RinexObsHeader> readHeader();
    Result<ObsEpoch> nextEpoch(); // EOF => Result with eof flag
};
struct ObsEpoch { GpsTime time; std::vector<ObsRecord> records; };
struct ObsRecord { int prn; double pseudorange; double carrierPhase;
                   double doppler; double snr; bool hasPseudorange; /* ... */ };
```

**Real math / algorithms:** RINEX 3.x observation type mapping (C1C, L1C, D1C, S1C, etc.),
epoch header parsing, per-satellite record decoding.

**Error handling:** unknown observation codes → warning + skip; malformed epoch → error
with line number; missing header → error.

**Acceptance criteria:** smoke path parses a small RINEX 3 OBS file and reports the number
of epochs and satellites; a corrupted file yields a descriptive error.

---

## Item 3 — NMEA generator

**Purpose:** Emit standard NMEA-0183 sentences (GGA, RMC, GSA, GSV, ZDA) from computed PVT,
with correct checksums and field formatting, so the platform can feed standard receivers
and loggers.

**Dependencies:** Item 1 (geometry), Item 4 (PVT). **Prerequisite for:** none downstream
(consumed by adapters/UI).

### 3.1 Sentence builder + checksum

**Purpose:** Low-level NMEA framing: `$` prefix, comma-separated fields, `*` checksum
(XOR of chars between `$` and `*`), CR/LF terminator.

**Classes:**
- `NmeaSentence` — builder and checksum utility.

**Public API:**
```cpp
class NmeaSentence {
public:
    static std::string build(const std::string& talker, const std::string& type,
                             const std::vector<std::string>& fields);
    static std::string checksum(const std::string& body); // 2 hex chars
    static bool        verify(const std::string& sentence);
};
```

**Real math / algorithms:** NMEA-0183 checksum (XOR of all chars between `$` and `*`),
field formatting rules (fixed decimals, sign, leading zeros).

**Error handling:** reject empty/invalid fields; enforce field count per sentence type.

**Acceptance criteria:** smoke path builds a GGA sentence and verifies its checksum
round-trips.

### 3.2 Sentence emitters (GGA/RMC/GSA/GSV/ZDA)

**Purpose:** High-level emitters producing each sentence type from PVT + satellite views.

**Classes:**
- `NmeaGenerator` — produces a full NMEA stream for a PVT epoch.

**Public API:**
```cpp
class NmeaGenerator {
public:
    NmeaGenerator(const NmeaConfig& cfg); // talker id, fix mode, datum
    std::string gga(const PvtResult& pvt, const std::vector<SatelliteView>& views) const;
    std::string rmc(const PvtResult& pvt) const;
    std::string gsa(const PvtResult& pvt, const std::vector<SatelliteView>& views) const;
    std::string gsv(const std::vector<SatelliteView>& views, int msgPerSentence) const;
    std::string zda(const GpsTime& t) const;
    std::string stream(const PvtResult& pvt, const std::vector<SatelliteView>& views) const;
};
```

**Real math / algorithms:** NMEA-0183 field layouts for GGA (time, lat/lon, fix quality,
num sats, HDOP, altitude, geoid separation), RMC (date, speed, course), GSA (PDOP/HDOP/VDOP,
sat IDs), GSV (satellite azimuth/elevation/SNR), ZDA (UTC date/time, local zone).

**Error handling:** validate PVT validity before emitting; clamp out-of-range values.

**Acceptance criteria:** smoke path emits all five sentence types for a synthetic PVT and
each passes `NmeaSentence::verify`; field counts match the standard.

---

## Item 4 — Pseudorange & Doppler

**Purpose:** Compute pseudorange and Doppler from satellite position/velocity, receiver
position/velocity, clock offsets, and range-rate, including geometric range, signal
propagation, and relativistic corrections. This is the core measurement model.

**Dependencies:** Item 1 (orbit/geometry), Item 5 (clock/ionosphere/troposphere for
corrections). **Prerequisite for:** Item 3 (PVT), Item 6 (SBAS).

### 4.1 Pseudorange

**Purpose:** Compute the measured pseudorange including all modeled biases.

**Classes:**
- `Pseudorange` — measurement model.

**Public API:**
```cpp
class Pseudorange {
public:
    // Geometric range + clock + atmospheric + relativistic corrections.
    Result<double> compute(const SvState& sv, const Vec3& rxEcef,
                           double rxClockBias, const AtmosphericCorrections& atm) const;
    static double geometricRange(const Vec3& svEcef, const Vec3& rxEcef);
    static double relativisticCorrection(const SvState& sv); // IS-GPS-200
};
```

**Real math / algorithms:**
- Geometric range: `|svEcef - rxEcef|` (with Earth-rotation/Sagnac correction for
  signal propagation time).
- Pseudorange: `ρ = geometricRange + c*(rxClockBias - svClockBias) + iono + tropo + rel`.
- Relativistic correction per IS-GPS-200 (from eccentricity and mean anomaly).

**Error handling:** reject non-finite inputs; validate range sanity (0 < ρ < ~1e8 m).

**Acceptance criteria:** smoke path computes pseudorange for a known geometry and matches
a hand-computed reference within 1e-3 m (excluding modeled errors).

### 4.2 Doppler

**Purpose:** Compute Doppler shift from range-rate.

**Classes:**
- `Doppler` — measurement model.

**Public API:**
```cpp
class Doppler {
public:
    Result<double> compute(const SvState& sv, const Vec3& rxEcef,
                           const Vec3& rxVelEcef, double rxClockDrift) const;
    static double rangeRate(const SvState& sv, const Vec3& rxEcef, const Vec3& rxVelEcef);
};
```

**Real math / algorithms:**
- Range-rate from relative velocity projected onto the line-of-sight unit vector.
- Doppler: `f_d = -rangeRate / λ` (λ = c/f, e.g. L1 = 1575.42 MHz).

**Error handling:** reject non-finite inputs; validate Doppler within physical bounds.

**Acceptance criteria:** smoke path computes Doppler for a known geometry and matches a
hand-computed reference within 1e-3 Hz.

---

## Item 5 — Error models

**Purpose:** Model clock error (receiver + satellite), ionosphere (Klobuchar), and
troposphere (Saastamoinen / Hopfield), with configurable model selection. These corrections
feed pseudorange (Item 4) and SBAS (Item 6).

**Dependencies:** Item 1.1 (time/frames), Item 1.2 (satellite clock). **Prerequisite for:**
Items 4, 6.

### 5.1 Clock error model

**Purpose:** Model receiver and satellite clock bias/drift.

**Classes:**
- `ClockModel` — configurable receiver + satellite clock.

**Public API:**
```cpp
class ClockModel {
public:
    struct Config { double bias; double drift; double driftRate; };
    void setReceiver(const Config& c);
    void setSatellite(const Config& c);
    double receiverBias(double t) const;   // polynomial
    double satelliteBias(double t) const;
};
```

**Real math / algorithms:** polynomial clock model `bias + drift*t + driftRate*t^2`
(IS-GPS-200 af0/af1/af2).

**Error handling:** validate config ranges.

**Acceptance criteria:** smoke path applies a known clock polynomial and matches reference.

### 5.2 Ionosphere model (Klobuchar)

**Purpose:** Model ionospheric delay using the Klobuchar broadcast model (IS-GPS-200).

**Classes:**
- `Ionosphere` — Klobuchar model.

**Public API:**
```cpp
class Ionosphere {
public:
    struct KlobucharParams { double alpha0..3; double beta0..3; };
    void setKlobuchar(const KlobucharParams& p);
    Result<double> delay(const SvState& sv, const Vec3& rxEcef,
                         const GpsTime& t) const; // meters
};
```

**Real math / algorithms:** Klobuchar model (IS-GPS-200 Section 20.3.3.5.1): geomagnetic
latitude, local time, obliquity factor, day/night delay, alpha/beta polynomial fit.

**Error handling:** validate alpha/beta ranges; reject invalid geometry.

**Acceptance criteria:** smoke path computes Klobuchar delay for a known geometry and
matches a reference within 0.1 m.

### 5.3 Troposphere model (Saastamoinen / Hopfield)

**Purpose:** Model tropospheric delay with selectable Saastamoinen or Hopfield model.

**Classes:**
- `Troposphere` — configurable model.

**Public API:**
```cpp
class Troposphere {
public:
    enum class Model { Saastamoinen, Hopfield };
    void setModel(Model m);
    void setMeteo(double pressure_hPa, double temp_C, double humidity_pct);
    Result<double> delay(const SvState& sv, const Vec3& rxEcef) const; // meters
};
```

**Real math / algorithms:** Saastamoinen (dry + wet, mapping function 1/sin(elevation)),
Hopfield (dry/wet refractivity integrals). Standard formulas from RTCM/ICD references.

**Error handling:** validate meteo ranges; reject elevation ≤ 0.

**Acceptance criteria:** smoke path computes tropospheric delay for a known geometry and
matches a reference within 0.05 m.

### 5.4 Configurable model selection

**Purpose:** Central configuration to select which error models are active.

**Classes:**
- `ErrorModelConfig` — aggregates clock/iono/tropo model selection.

**Public API:**
```cpp
class ErrorModelConfig {
public:
    void enableClock(bool); void enableIonosphere(bool); void enableTroposphere(bool);
    void setIonosphereModel(Ionosphere::Model);
    void setTroposphereModel(Troposphere::Model);
    AtmosphericCorrections compute(const SvState& sv, const Vec3& rxEcef,
                                   const GpsTime& t) const;
};
struct AtmosphericCorrections { double iono; double tropo; double rel; };
```

**Real math / algorithms:** orchestration of the above models into a single correction
struct consumed by Item 4.

**Error handling:** validate configuration consistency.

**Acceptance criteria:** smoke path toggles models on/off and confirms corrections change
as expected.

---

## Item 6 — SBAS augmentation

**Purpose:** Handle SBAS (WAAS/EGNOS) messages and integrity parameters: fast corrections,
long-term corrections, ionospheric grid corrections, UDRE/GIVE, and protection levels
(HPL/VPL). This is the integrity-critical capability.

**Dependencies:** Item 1 (geometry), Item 4 (pseudorange), Item 5 (ionosphere).
**Prerequisite for:** none (top-level capability).

### 6.1 SBAS message parsing

**Purpose:** Parse SBAS message types (1–4, 6, 9, 10, 12, 17, 18, 25, 26, 27, 28) per
RTCA DO-229.

**Classes:**
- `SbasMessage` — raw message container + type dispatch.

**Public API:**
```cpp
class SbasMessage {
public:
    static Result<SbasMessage> parse(const std::vector<uint8_t>& bits250); // 250-bit
    int type() const;
    // typed accessors per message type
};
```

**Real math / algorithms:** SBAS 250-bit message framing, CRC (24-bit), bit-field decoding
per RTCA DO-229D.

**Error handling:** CRC failure → error; unknown type → warning + skip; truncated → error.

**Acceptance criteria:** smoke path parses a known SBAS message and matches the decoded
fields; a corrupted message fails CRC.

### 6.2 Fast & long-term corrections

**Purpose:** Apply fast corrections (PRC, UDRE) and long-term corrections (position/velocity
offsets, clock offsets) to satellite states.

**Classes:**
- `SbasCorrections` — applies corrections to `SvState`.

**Public API:**
```cpp
class SbasCorrections {
public:
    void applyFast(int prn, double prc, double udre, double t);
    void applyLongTerm(int prn, const LongTermCorrection& ltc);
    Result<SvState> correct(const SvState& sv, int prn, double t) const;
};
```

**Real math / algorithms:** RTCA DO-229 fast correction (PRC + range-rate correction),
long-term correction (position/velocity/clock offset applied in ECEF).

**Error handling:** missing correction for a PRN → error; stale correction (age) → warning.

**Acceptance criteria:** smoke path applies a fast + long-term correction and confirms the
state shifts as expected.

### 6.3 Ionospheric grid corrections

**Purpose:** Apply SBAS ionospheric grid point (IGP) corrections and GIVE.

**Classes:**
- `SbasIonoGrid` — IGP delay + GIVE interpolation.

**Public API:**
```cpp
class SbasIonoGrid {
public:
    void addIgp(const IgpData& igp);
    Result<double> delay(const Vec3& rxEcef, const SvState& sv,
                         const GpsTime& t) const; // meters
    Result<double> give(const Vec3& rxEcef, const SvState& sv) const;
};
```

**Real math / algorithms:** RTCA DO-229 ionospheric grid interpolation (bilinear over IGP
mesh), GIVE mapping to user ionospheric error.

**Error handling:** missing IGP → error; out-of-mesh → error.

**Acceptance criteria:** smoke path interpolates IGP delay for a known pierce point and
matches a reference.

### 6.4 Integrity: UDRE/GIVE and protection levels (HPL/VPL)

**Purpose:** Compute integrity parameters and horizontal/vertical protection levels.

**Classes:**
- `Integrity` — UDRE/GIVE aggregation and HPL/VPL.

**Public API:**
```cpp
class Integrity {
public:
    void setUdre(int prn, double udre);
    void setGive(double give);
    Result<ProtectionLevels> compute(const std::vector<SatelliteView>& views,
                                     const std::vector<double>& udrePerPrn,
                                     double give) const;
};
struct ProtectionLevels { double hpl; double vpl; };
```

**Real math / algorithms:** RTCA DO-229 protection level equations: HPL/VPL from geometry
matrix (H), weighted least-squares, UDRE/GIVE variance, and the k-factor (k_H, k_V) tables.

**Error handling:** insufficient satellites → error; non-invertible geometry → error.

**Acceptance criteria:** smoke path computes HPL/VPL for a known geometry and matches a
reference within tolerance; degraded geometry raises HPL as expected.

---

## Item 7 — Scenario facade (integration)

**Purpose:** Tie Items 1–6 into a single `Scenario` facade that a caller can drive to
produce a full PVT + NMEA + integrity output for an epoch. This is the smoke-path entry
point and the interface the UI/adapters will consume.

**Dependencies:** all items. **Prerequisite for:** none (integration).

**Classes:**
- `Scenario` — top-level facade.
- `ScenarioError` — shared error type (used by all items).

**Public API:**
```cpp
class Scenario {
public:
    Scenario(const ScenarioConfig& cfg);
    void addGps(const BroadcastEphemeris& eph, int prn);
    void addTle(const Tle& tle, int prn);
    void setReceiver(const Vec3& rxEcef, const Vec3& rxVelEcef);
    Result<ScenarioEpoch> step(const GpsTime& t);
};
struct ScenarioEpoch {
    std::vector<SatelliteView> views;
    Result<PvtResult> pvt;
    std::vector<std::string> nmea;   // GGA/RMC/GSA/GSV/ZDA
    ProtectionLevels protection;
};
```

**Real math / algorithms:** orchestration; PVT via weighted least-squares (WLS) over
pseudoranges with the modeled corrections.

**Error handling:** aggregate per-satellite failures; report a coherent epoch result.

**Acceptance criteria:** smoke path builds a small constellation, steps one epoch, and
produces a valid PVT, NMEA sentences (all checksum-verified), and protection levels.

---

## Dependency graph (for scrum-master ordering)

```
1.1 frames/time ──► 1.2 Keplerian ──► 1.4 geometry ──► 4 pseudorange/Doppler ──► 3 NMEA
        │              │                  │                │
        │              └──► 1.3 SGP4 ─────┘                │
        │                                                  ▼
        └──► 2 RINEX ──────────────────────────────► 5 error models ──► 6 SBAS
                                                          │
                                                          └──► 7 Scenario facade
```

**Recommended build order:**
1. Item 1.1 (frames/time) — foundation.
2. Item 1.2 (Keplerian) and Item 1.3 (SGP4) — parallelizable after 1.1.
3. Item 1.4 (geometry) — after 1.2/1.3.
4. Item 2 (RINEX) — after 1.1.
5. Item 5 (error models) — after 1.1/1.2.
6. Item 4 (pseudorange/Doppler) — after 1.x + 5.
7. Item 3 (NMEA) — after 1.x + 4.
8. Item 6 (SBAS) — after 1.x + 4 + 5.
9. Item 7 (Scenario facade) — after all.

---

## CMake integration

- Replace the stub registration in `core/CMakeLists.txt`:
  `lodestar_add_module(lodestar_scenario scenario/stub.cpp)` →
  `lodestar_add_module(lodestar_scenario <all new .cpp files>)`.
- Keep `stub.cpp`'s `module_version()` (or move it) so existing callers still link.
- Add `target_link_libraries(lodestar_scenario PUBLIC lodestar_common)` if the module uses
  `core/common` (logging/config). No new external dependencies required (all math is
  self-contained; no Eigen/Boost needed — use small internal `Vec3`/`Mat3` types).
- Extend `core/smoke/main.cpp` (or add `core/smoke/scenario_smoke.cpp`) with a Phase 4
  smoke path that exercises the Scenario facade and prints PASS/FAIL, exiting non-zero on
  failure. Wire it into the build.

## Acceptance criteria (phase-level)

1. `cmake -B build && cmake --build build` succeeds on the host platform with the new
   `core/scenario` sources.
2. The Phase 4 smoke path runs and prints PASS, exercising: frame/time conversion, Keplerian
   propagation, SGP4 propagation, RINEX parse, NMEA generation (checksum-verified), a
   pseudorange + Doppler computation, error-model corrections, and an SBAS protection-level
   computation.
3. No new external dependencies; all math self-contained in `core/scenario`.
4. `PLAN.md` Phase 4 row updated to `PLANNED` (this plan) and later to `DONE` by devops.
