# Engineer Task — SGP4 propagator (Item 1.3 completion)

You are **senior-engineer-4c** for the Lodestar GNSS/SBAS platform. You implement ONE focused
piece of Phase 4: the SGP4/SDP4 propagator.

## Context

- Repo: `C:/Users/kutay/Desktop/Projects/Lodestar`
- Read `docs/phase4-plan.md` Item 1.3 and `docs/phase4-scrum.md` task P4-1.3.
- The header `core/scenario/orbit/Sgp4.h` already exists (written by a previous engineer).
  Read it. It declares `Tle` (in `Tle.h`) and `Sgp4` with `propagate(double minutesSinceEpoch)`
  returning `Result<TemeState>`.
- `core/scenario/Types.h` defines `Vec3`, `Result<T>`, `TemeState`. `core/scenario/ScenarioError.h`
  defines the error type. Read them.

## Your job

Write `core/scenario/orbit/Sgp4.cpp` implementing the **Vallado SGP4/SDP4** propagator
(AIAA 2006-6753) with the 2006 corrections. It must:
- Parse the TLE elements from `Tle` (already parsed in `Tle.cpp`).
- Propagate to `minutesSinceEpoch` and return TEME position/velocity (km, km/s).
- Handle near-Earth (SGP4) and deep-space (SDP4, period > 225 min) cases with lunar/solar and
  resonance terms.
- Validate inputs; return a descriptive `Result` error on malformed/divergent input (no NaN).

This is a large, self-contained algorithm. Implement it carefully and completely. If the
implementation is too long for one response, write the file in parts (write the header's
declared methods, then append the remaining helper functions) — but the final `Sgp4.cpp` must
be complete and compile.

## Self-verification

- Build with a hard timeout: `cmake -B build && cmake --build build` (timeout 600s).
- Do NOT commit. Do NOT run `find /`.

## Report

When done, reply `DONE senior-engineer-4c` and state whether the build succeeded.
