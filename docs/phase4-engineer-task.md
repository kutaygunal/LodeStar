# Senior Engineer Task — Phase 4 (ScenarioForge: real GNSS math)

You are **senior-engineer-4b** for the Lodestar GNSS/SBAS platform. You continue and complete Phase 4. A previous engineer turn was truncated mid-work; you pick up where it left off.

## Context

- Repo: `C:/Users/kutay/Desktop/Projects/Lodestar`
- Read these files FIRST:
  - `docs/phase4-plan.md` — the detailed plan (7 items, 20 child items, math, APIs, acceptance).
  - `docs/phase4-scrum.md` — the itemized task list (P4-1.1 … P4-7) with review notes R1–R8.
  - `docs/architecture.md` Section 4 (Real GNSS math) and Section 5 (module layout).
  - `core/CMakeLists.txt` and `core/smoke/main.cpp` (existing build + smoke wiring).
- Phases 1-3 are DONE. `core/scenario` currently contains only `stub.cpp`. You build the real
  module.

## Current state (already implemented — review, then continue)

A previous engineer already created these files (review them for correctness, fix if needed):
- `core/scenario/Types.h`, `core/scenario/ScenarioError.h`
- `core/scenario/frames/` (TimeSystem, Frames, Geometry)
- `core/scenario/orbit/Keplerian.h/.cpp`, `core/scenario/orbit/Tle.h/.cpp`

**Still missing (you must implement):**
- `core/scenario/orbit/Sgp4.h/.cpp` (SGP4/SDP4 propagator)
- `core/scenario/orbit/Constellation.h/.cpp` (Item 1.4)
- `core/scenario/rinex/` (Items 2.1, 2.2)
- `core/scenario/nmea/` (Items 3.1, 3.2)
- `core/scenario/pvt/` (Items 4.1, 4.2 + WLS solver)
- `core/scenario/errors/` (Items 5.1-5.4)
- `core/scenario/sbas/` (Items 6.1-6.4)
- `core/scenario/Scenario.h/.cpp` (Item 7 facade)
- CMake wiring in `core/CMakeLists.txt` (replace stub registration, keep `module_version()` linkable)
- Smoke path (`core/smoke/scenario_smoke.cpp` or extend `main.cpp`)

## Your job

Implement **all** remaining Phase 4 in `core/scenario` per the plan and scrum task list, in the
dependency order given in `docs/phase4-scrum.md` (build order section). This is a
**COMMERCIAL GRADE** deliverable: production-quality C++, proper error handling, real GNSS
math — NOT a toy.

Key requirements (from review notes R1–R8):
- **R1:** Define the shared types (`Vec3`, `Mat3`, `Result<T>`, `SvState`, `TemeState`,
  `PvtResult`, `AtmosphericCorrections`, `ProtectionLevels`, `ScenarioConfig`, `ScenarioEpoch`,
  `NmeaConfig`, `LongTermCorrection`, `IgpData`, `RinexNavHeader`, `RinexObsHeader`,
  `ObsRecord`, `ObsEpoch`) in a shared header (e.g. `core/scenario/Types.h`) plus
  `ScenarioError.h`.
- **R2:** Use ONE consistent `Result<T>` API (with `.failed()`, `.error()`, `.value()`, and an
  EOF signaling convention for RINEX parsers). Document it.
- **R3:** Implement a weighted least-squares (WLS) PVT solver (inside the Scenario facade or a
  small `pvt/Solver`) to produce `PvtResult` from ≥4 pseudoranges.
- **R4:** Replace the stub registration in `core/CMakeLists.txt` with the full source list; keep
  `module_version()` linkable; wire the new smoke path into `lodestar_smoke`.
- **R5:** Add the Phase 4 smoke path (`core/smoke/scenario_smoke.cpp` or extend `main.cpp`)
  WITHOUT breaking the existing Phase 3 smoke path.
- **R6:** Source or hand-compute reference values for each smoke check (GMST, ECEF↔geodetic,
  Keplerian position, SGP4 TEME, Klobuchar, troposphere, HPL/VPL). Document the reference and
  tolerance in the smoke code.
- **R7:** No new external dependencies. All math self-contained (internal `Vec3`/`Mat3`; no
  Eigen/Boost). No new `find_package`/`target_link_libraries` to external math libs.
- **R8:** Every public entry point returns a typed result distinguishing success from
  failure-with-reason; no silent NaN. Smoke paths exercise at least one failure path per item.

## Self-verification (no test agents)

Build and run the smoke path with HARD TIMEOUTS (per `docs/working-rules.md`):
- `cmake -B build && cmake --build build` (with a timeout).
- Run the smoke binary (one at a time, with a timeout). It must print PASS and exit 0.
- Confirm the existing Phase 3 smoke still passes.

## Constraints

- Do NOT commit or push (that is the devops agent's job).
- Do NOT run `find /` or full-filesystem scans. Use bounded `ls`/`grep`.
- Do NOT spawn long-running background processes.

## Report

When done, reply `DONE senior-engineer-4` and summarize: what you built, the build result,
and the smoke result. Do NOT notify the orchestrator yourself — the orchestrator will poll.
