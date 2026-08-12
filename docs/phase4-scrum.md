# Phase 4 — ScenarioForge (Real GNSS Math) — Scrum Task List

> **Owner:** scrum-master · **Status:** IN PROGRESS · **Assigned to:** `senior-engineer-4`
> **Source of truth:** `docs/phase4-plan.md` (planner's detailed plan).
> **Standard:** COMMERCIAL GRADE — production C++, real GNSS math, proper error handling.
> **Verification:** build + smoke run (no test agents in this loop, per `docs/working-rules.md`).

## Assignment decision

The 7 items (20 child items) are **interdependent** (see dependency graph in the plan).
They share too much state (time/frame utilities, `Vec3`/`Mat3` types, `Result<>`/`ScenarioError`,
CMake wiring) to be split across parallel engineers. Therefore the **whole phase is assigned as
ONE engineering task to a single `senior-engineer-4` agent**, which implements the items in
dependency order. No parallel engineers.

## Review notes (gaps / ambiguities — do NOT block, engineer resolves)

- **R1 — Shared math types:** The plan references `Vec3`, `Mat3`, `Result<T>`, `SvState`,
  `TemeState`, `PvtResult`, `AtmosphericCorrections`, `ProtectionLevels`, `ScenarioConfig`,
  `ScenarioEpoch`, `NmeaConfig`, `LongTermCorrection`, `IgpData`, `RinexNavHeader`,
  `RinexObsHeader`, `ObsRecord`, `ObsEpoch`. These are not yet defined anywhere. The engineer
  must define them (suggest a `core/scenario/Types.h` / `ScenarioError.h` shared header) before
  the items that use them. **Acceptance:** a single shared types header compiles and is included
  by all items.
- **R2 — `Result<T>` semantics:** The plan uses `Result<T>` with `.failed()`, `.error()`,
  `.value()`, and an "eof flag" for RINEX parsers. The exact API (how EOF is signaled) is
  unspecified. Engineer must pick a consistent convention (e.g. a `Result<T, E>` with a
  `isEof()` accessor or a sentinel) and document it. **Acceptance:** all callers use one
  consistent `Result` API.
- **R3 — `PvtResult` / WLS PVT:** Item 7 requires a weighted least-squares PVT solver, but no
  child item defines it. The engineer must implement a WLS PVT solver inside the Scenario
  facade (or a small `pvt/Solver` helper) to produce `PvtResult`. **Acceptance:** smoke path
  produces a finite, sane PVT from ≥4 pseudoranges.
- **R4 — CMake wiring:** `core/CMakeLists.txt` currently registers `lodestar_scenario` with only
  `scenario/stub.cpp`. The engineer must replace that with the full source list, keep
  `module_version()` linkable, and wire the new smoke path into `lodestar_smoke`. **Acceptance:**
  `cmake -B build && cmake --build build` succeeds; `lodestar_smoke` links the scenario module.
- **R5 — Smoke path location:** Plan suggests `core/smoke/scenario_smoke.cpp` or extending
  `main.cpp`. Engineer chooses; must not break the existing Phase 3 smoke path. **Acceptance:**
  existing smoke still passes and the new Phase 4 smoke prints PASS.
- **R6 — Reference data:** Acceptance criteria reference "published reference" values (GMST,
  ECEF↔geodetic, Keplerian position, SGP4 TEME, Klobuchar, troposphere, HPL/VPL). The engineer
  must source or hand-compute these reference values (no test agents). **Acceptance:** each
  smoke check compares against a documented reference within the stated tolerance.
- **R7 — No external deps:** All math self-contained; no Eigen/Boost. Use internal `Vec3`/`Mat3`.
  **Acceptance:** no new `find_package`/`target_link_libraries` to external math libs.
- **R8 — Error handling:** Every public entry point must return a typed result distinguishing
  success from failure-with-reason; no silent NaN. **Acceptance:** smoke paths exercise at least
  one failure path per item (invalid input / corrupted file / CRC failure / insufficient sats).

---

## Itemized task list

### Item 1 — Orbit propagation (foundation)

**P4-1.1 — Frame & time utilities** — *Effort: M*
- **Scope:** `core/scenario/frames/TimeSystem.h/.cpp`, `Frames.h/.cpp`, `Geometry.h/.cpp`;
  shared `Types.h` (Vec3/Mat3) and `ScenarioError.h`.
- **Dependencies:** none (foundation).
- **Acceptance:** smoke path computes a known GPS epoch's GMST and converts a known ECEF point
  to geodetic and back, matching reference within 1e-6 rad / 1e-3 m. Out-of-range inputs throw
  `ScenarioError` (no NaN).

**P4-1.2 — Keplerian two-body propagator (broadcast ephemeris)** — *Effort: M*
- **Scope:** `core/scenario/orbit/Keplerian.h/.cpp` (`BroadcastEphemeris`, `Keplerian`, `SvState`).
- **Dependencies:** P4-1.1.
- **Acceptance:** smoke path propagates a known GPS PRN ephemeris to `t=toe`; ECEF position
  matches published reference within 1 m; clock bias matches reference. Invalid elements
  (e≥1, sqrtA≤0) and Kepler non-convergence return an error result.

**P4-1.3 — SGP4/SDP4 propagator (TLE-based LEO)** — *Effort: L*
- **Scope:** `core/scenario/orbit/Tle.h/.cpp`, `Sgp4.h/.cpp` (`Tle`, `Sgp4`, `TemeState`).
- **Dependencies:** P4-1.1.
- **Acceptance:** smoke path propagates a published TLE (e.g. ISS) to a known epoch; TEME
  position matches reference within SGP4 tolerance (< 1 km short arc). Malformed TLE (bad
  checksum/fields) returns a clear error.

**P4-1.4 — Satellite geometry (constellation view)** — *Effort: M*
- **Scope:** `core/scenario/orbit/Constellation.h/.cpp` (`Constellation`, `SatelliteView`).
- **Dependencies:** P4-1.1, P4-1.2, P4-1.3.
- **Acceptance:** smoke path builds a small GPS constellation, computes views from a known
  station, and confirms expected visible/occluded satellites under an elevation mask. Per-sat
  propagation failures are collected, not fatal.

### Item 2 — RINEX parser

**P4-2.1 — RINEX navigation parser** — *Effort: M*
- **Scope:** `core/scenario/rinex/RinexNav.h/.cpp`, `RinexError.h/.cpp` (`RinexNavParser`,
  `RinexNavHeader`).
- **Dependencies:** P4-1.1 (time/frames for epoch handling).
- **Acceptance:** smoke path parses a small RINEX 3 NAV file; `toe`/`sqrtA` match the file. A
  deliberately corrupted file yields a descriptive error with line number.

**P4-2.2 — RINEX observation parser** — *Effort: M*
- **Scope:** `core/scenario/rinex/RinexObs.h/.cpp` (`RinexObsParser`, `ObsEpoch`, `ObsRecord`).
- **Dependencies:** P4-1.1.
- **Acceptance:** smoke path parses a small RINEX 3 OBS file and reports epoch/satellite counts.
  Unknown observation codes → warning + skip; malformed epoch → error with line number.

### Item 3 — NMEA generator

**P4-3.1 — Sentence builder + checksum** — *Effort: S*
- **Scope:** `core/scenario/nmea/NmeaSentence.h/.cpp` (`NmeaSentence`).
- **Dependencies:** none (self-contained).
- **Acceptance:** smoke path builds a GGA sentence and verifies its checksum round-trips
  (`verify` returns true). Invalid/empty fields rejected.

**P4-3.2 — Sentence emitters (GGA/RMC/GSA/GSV/ZDA)** — *Effort: M*
- **Scope:** `core/scenario/nmea/NmeaGenerator.h/.cpp` (`NmeaGenerator`, `NmeaConfig`).
- **Dependencies:** P4-1.x (geometry), P4-4 (PVT), P4-3.1.
- **Acceptance:** smoke path emits all five sentence types for a synthetic PVT; each passes
  `NmeaSentence::verify`; field counts match the standard. Invalid PVT rejected/clamped.

### Item 4 — Pseudorange & Doppler

**P4-4.1 — Pseudorange** — *Effort: M*
- **Scope:** `core/scenario/pvt/Pseudorange.h/.cpp` (`Pseudorange`).
- **Dependencies:** P4-1.x, P4-5 (error models for corrections).
- **Acceptance:** smoke path computes pseudorange for a known geometry; matches hand-computed
  reference within 1e-3 m (excluding modeled errors). Non-finite inputs rejected; range sanity
  checked (0 < ρ < ~1e8 m).

**P4-4.2 — Doppler** — *Effort: S*
- **Scope:** `core/scenario/pvt/Doppler.h/.cpp` (`Doppler`).
- **Dependencies:** P4-1.x.
- **Acceptance:** smoke path computes Doppler for a known geometry; matches hand-computed
  reference within 1e-3 Hz. Non-finite inputs rejected; physical bounds checked.

### Item 5 — Error models

**P4-5.1 — Clock error model** — *Effort: S*
- **Scope:** `core/scenario/errors/ClockModel.h/.cpp` (`ClockModel`).
- **Dependencies:** P4-1.1.
- **Acceptance:** smoke path applies a known clock polynomial and matches reference. Config
  ranges validated.

**P4-5.2 — Ionosphere model (Klobuchar)** — *Effort: M*
- **Scope:** `core/scenario/errors/Ionosphere.h/.cpp` (`Ionosphere`).
- **Dependencies:** P4-1.1, P4-1.2.
- **Acceptance:** smoke path computes Klobuchar delay for a known geometry; matches reference
  within 0.1 m. Alpha/beta ranges validated; invalid geometry rejected.

**P4-5.3 — Troposphere model (Saastamoinen / Hopfield)** — *Effort: M*
- **Scope:** `core/scenario/errors/Troposphere.h/.cpp` (`Troposphere`).
- **Dependencies:** P4-1.1.
- **Acceptance:** smoke path computes tropospheric delay for a known geometry; matches reference
  within 0.05 m. Meteo ranges validated; elevation ≤ 0 rejected.

**P4-5.4 — Configurable model selection** — *Effort: S*
- **Scope:** `core/scenario/errors/ErrorModelConfig.h/.cpp` (`ErrorModelConfig`,
  `AtmosphericCorrections`).
- **Dependencies:** P4-5.1, P4-5.2, P4-5.3.
- **Acceptance:** smoke path toggles models on/off and confirms corrections change as expected.

### Item 6 — SBAS augmentation

**P4-6.1 — SBAS message parsing** — *Effort: M*
- **Scope:** `core/scenario/sbas/SbasMessage.h/.cpp` (`SbasMessage`).
- **Dependencies:** P4-1.1.
- **Acceptance:** smoke path parses a known SBAS message and matches decoded fields; a corrupted
  message fails CRC. Unknown type → warning + skip; truncated → error.

**P4-6.2 — Fast & long-term corrections** — *Effort: M*
- **Scope:** `core/scenario/sbas/SbasCorrections.h/.cpp` (`SbasCorrections`, `LongTermCorrection`).
- **Dependencies:** P4-1.x, P4-4.
- **Acceptance:** smoke path applies a fast + long-term correction and confirms the state shifts
  as expected. Missing correction for a PRN → error; stale correction → warning.

**P4-6.3 — Ionospheric grid corrections** — *Effort: M*
- **Scope:** `core/scenario/sbas/SbasCorrections.h/.cpp` (`SbasIonoGrid`, `IgpData`).
- **Dependencies:** P4-1.x, P4-5.
- **Acceptance:** smoke path interpolates IGP delay for a known pierce point; matches reference.
  Missing IGP / out-of-mesh → error.

**P4-6.4 — Integrity: UDRE/GIVE and protection levels (HPL/VPL)** — *Effort: M*
- **Scope:** `core/scenario/sbas/Integrity.h/.cpp` (`Integrity`, `ProtectionLevels`).
- **Dependencies:** P4-1.x, P4-4, P4-5.
- **Acceptance:** smoke path computes HPL/VPL for a known geometry; matches reference within
  tolerance; degraded geometry raises HPL. Insufficient satellites / non-invertible geometry →
  error.

### Item 7 — Scenario facade (integration)

**P4-7 — Scenario facade + WLS PVT + smoke path** — *Effort: L*
- **Scope:** `core/scenario/Scenario.h/.cpp`, `ScenarioError.h/.cpp`; WLS PVT solver (inside
  facade or `pvt/Solver`); CMake wiring in `core/CMakeLists.txt`; smoke path
  (`core/smoke/scenario_smoke.cpp` or extended `main.cpp`).
- **Dependencies:** all of P4-1 … P4-6.
- **Acceptance:** smoke path builds a small constellation, steps one epoch, and produces a valid
  PVT, NMEA sentences (all checksum-verified), and protection levels. `cmake -B build &&
  cmake --build build` succeeds; Phase 4 smoke prints PASS and exits 0; existing Phase 3 smoke
  still passes. `module_version()` remains linkable.

---

## Build order (dependency-ordered, single engineer)

1. P4-1.1 → 2. P4-1.2 & P4-1.3 → 3. P4-1.4 → 4. P4-2.1 & P4-2.2 → 5. P4-5.1/5.2/5.3/5.4 →
   6. P4-4.1 & P4-4.2 → 7. P4-3.1 & P4-3.2 → 8. P4-6.1/6.2/6.3/6.4 → 9. P4-7.

## Phase-level acceptance (from plan)

1. `cmake -B build && cmake --build build` succeeds with the new `core/scenario` sources.
2. Phase 4 smoke path runs and prints PASS, exercising: frame/time conversion, Keplerian
   propagation, SGP4 propagation, RINEX parse, NMEA generation (checksum-verified), pseudorange
   + Doppler, error-model corrections, and an SBAS protection-level computation.
3. No new external dependencies; all math self-contained in `core/scenario`.
4. `PLAN.md` Phase 4 row updated to `IN PROGRESS` (this scrum) and later to `DONE` by devops.
