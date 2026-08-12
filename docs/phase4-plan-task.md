# Planner Task — Phase 4 (ScenarioForge: real GNSS math)

You are the **planner** for the Lodestar GNSS/SBAS platform. Your job is to produce a
DETAILED, COMMERCIAL-GRADE plan for **Phase 4: ScenarioForge (real GNSS math)**.

## Context

- Repo: `C:/Users/kutay/Desktop/Projects/Lodestar`
- Read `PLAN.md` (Phase 4 row is currently TODO) and `docs/architecture.md` Section 4
  (Real GNSS math) and Section 5 (module layout). The module lives in `core/scenario`.
- Phases 1-3 are DONE (architecture doc, monorepo scaffold, vertical slice for
  common/persistence/tracelink). `core/scenario` currently contains only `stub.cpp`.
- This is a **COMMERCIAL GRADE** application: production-quality C++, proper error
  handling, real GNSS math — NOT a toy.

## Your deliverable

Think through **6 detailed items** for Phase 4 and create **child items** where needed.
Do NOT stop at a shallow one-line list. For each item, specify:
- **Purpose** and scope.
- **Key data structures / classes** (with names).
- **Public API / interfaces** (function signatures or method names).
- **Real math / algorithms** to implement (cite the standard/equation where relevant).
- **Error handling** requirements.
- **Dependencies** on other items (so the scrum-master can order the work).
- **Acceptance criteria** (how the engineer self-verifies by build + smoke run).

The 6 top-level items are:

1. **Orbit propagation** — Keplerian two-body propagation + SGP4 (SDP4) for LEO.
   Include: ECEF/ECI frames, WGS-84, time systems (GPS time, UTC, Julian date), and
   ground-station/satellite geometry.
2. **RINEX parser** — parse RINEX 3.x navigation (broadcast ephemeris) and observation
   files. Robust tokenization, header/record handling, error reporting.
3. **NMEA generator** — emit standard NMEA-0183 sentences (GGA, RMC, GSA, GSV, ZDA) from
   computed PVT, with correct checksums and field formatting.
4. **Pseudorange & Doppler** — compute pseudorange and Doppler from satellite position,
   receiver position, clock offsets, and range-rate. Include geometric range, signal
   propagation, and relativistic corrections.
5. **Error models** — clock error (receiver + satellite), ionosphere (Klobuchar), and
   troposphere (Saastamoinen / Hopfield) models. Provide configurable model selection.
6. **SBAS augmentation** — SBAS (WAAS/EGNOS) message handling and integrity parameters:
   fast corrections, long-term corrections, ionospheric grid corrections, UDRE/GIVE,
   and protection levels (HPL/VPL).

For each item, break it into child items (e.g. "1.1 frame/time utilities", "1.2 Keplerian
propagator", "1.3 SGP4 propagator", "1.4 satellite geometry"). Identify which child items
are prerequisites for others.

## Output

Update `PLAN.md`:
- Replace the Phase 4 row's Description with a compact summary and set Status to
  `PLANNED`.
- Add a **Phase 4 — Detailed Plan** section below the existing phases with the full
  6-item breakdown (including child items, dependencies, and acceptance criteria).

Also write the full detailed plan to `docs/phase4-plan.md` (the scrum-master and engineer
will read it).

Do NOT implement any code. Do NOT commit. Reply `DONE planner` when the plan is written.
