# Product Manager Challenge — ScenarioForge

You are a **senior product manager** for the Lodestar GNSS/SBAS platform. Your job is to
**challenge the ScenarioForge module** and produce a deep, honest, commercial-grade gap
analysis vs. leading GNSS/SBAS simulation tools. **Do NOT change any source code.** Produce a
detailed HTML report only.

## The module (current state)
ScenarioForge is the software-defined GNSS/SBAS scenario generator. It has real C++ math:
- **Frames**: ECEF/geodetic, time systems, geometry
- **Orbit**: Keplerian, TLE, SGP4, constellation
- **RINEX**: NAV + OBS parsing
- **NMEA**: GGA/RMC/ZDA/GSA/GSV sentence generation
- **PVT**: pseudorange, Doppler, solver
- **Error models**: clock, Klobuchar ionosphere, Saastamoinen troposphere
- **SBAS**: message parsing, corrections, integrity (HPL/VPL protection levels)
- **Scenario facade**: `addGps`, `addTle`, `setReceiver`, `setErrorModels`, `addSbasIgp`, `step()`

## What to analyze (deep, product-manager lens)
Compare ScenarioForge against commercial GNSS/SBAS simulation tools (e.g. **Spirent GSS9000 +
SimGEN, Rohde & Schwarz SMW200A + GNSS suites, Skydel, Keysight, u-blox**). For each area,
state what ScenarioForge has, what commercial tools have, and the **gap**:

1. **Signal generation** — RF signal generation (IF/RF), multi-constellation (GPS/Galileo/
   GLONASS/BeiDou), multi-frequency (L1/L2/L5), signal power/CN0 control?
2. **Scenario modeling** — Trajectory/vehicle motion, multipath, interference/jamming,
   antenna patterns, atmospheric effects, dynamic scenarios?
3. **Constellation fidelity** — Ephemeris accuracy, almanac, clock, orbit propagation
   (SGP4 vs. high-precision), satellite health/availability?
4. **SBAS/GBAS** — SBAS message generation, integrity, ionospheric grid, augmentation
   accuracy, GBAS support?
5. **Receiver modeling** — Receiver dynamics, tracking loops, multipath, interference
   robustness, PVT solver accuracy?
6. **Interoperability** — Export to vendor formats (Spirent/R&S/Skydel), RINEX, NMEA,
   scenario interchange, hardware-in-the-loop?
7. **Performance** — Real-time stepping, channel count, update rate, determinism?
8. **UX** — Scenario editor, visualization, trajectory builder, results analysis?
9. **Commercial readiness** — What would a GNSS test engineer expect that is missing?

## Deliverable
Write a **detailed HTML report** to `docs/reports/scenarioforge-pm-report.html`. Structure:
- Executive summary (verdict + top gaps)
- Capability vs. commercial tools comparison table
- Deep-dive per area (1–9) with "Have / Commercial / Gap"
- Prioritized gap list (P0/P1/P2) with effort estimate
- Recommended roadmap

Use clean, self-contained HTML (inline CSS, no external deps). Make it professional and
readable. When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE pm-scenarioforge'`.
