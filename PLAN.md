# Plan

Purpose: GNSS/SBAS Integrated Test & Verification Platform (Lodestar) - C++ core, Python intelligence, Qt Widgets UI, SQLite, Jenkins CI/CD, local LLMs

**Loop scope: steps 1-3 only. NO testing agents.** Engineers/architects self-verify by
compiling/building and report; devops commits. Follow `docs/working-rules.md` (hard
timeouts, no full-filesystem scans, no commits from engineer agents). Each phase is
assigned via a task file at `docs/<phase>-task.md` per the delegation rule.

| # | Phase | Description | Priority | Status | Assigned to | Committed |
|---|-------|-------------|----------|--------|-------------|-----------|
| 1 | Architecture doc | Write `docs/architecture.md` capturing all decisions (C++-only service, adapter pattern, SQLite, real GNSS math, monorepo, Qt Widgets, cross-platform) | High | DONE | senior-architect-1 | yes |
| 2 | Monorepo scaffold | Create folder structure with a compiling CMake build (Windows/Linux) | High | DONE | senior-engineer-2 | yes |
| 3 | Phase 1 vertical slice | Build core/common, core/persistence (SQLite schema), core/tracelink (graph model) | High | DONE | senior-engineer-3 | yes |
| 4 | ScenarioForge (real GNSS math) | Build core/scenario: orbit propagation (Keplerian + SGP4), RINEX parser, NMEA generator, pseudorange/Doppler, error models (clock/ionosphere/troposphere), SBAS augmentation. COMMERCIAL GRADE. Planner details into 6 items (with child items if needed); scrum-master itemizes. | High | DONE | senior-engineer-4 | yes |
| 5 | Adapters + thin C++ REST API | Build core/adapters (IAdapter pattern: Spirent/R&S/Skydel/Llm/Python) and core/api (thin embedded C++ REST/HTTP service over core). Unblocks TestForge/AssureCheck/RiskAI + external tool + Python/LLM integration. COMMERCIAL GRADE. Planner details into items (child items if needed); scrum-master itemizes. | High | DONE | senior-engineer-5 | yes |
| 6 | TestForge (single module, perfection) | Build core/testforge: IT&V plan generation (auto-generate procedures from requirements/scenarios), execution (TestRunner with step pass/fail evaluation), reporting (markdown + JSON). Persisted via SQLite (new migration 002) + DAOs. Self-verifying smoke path. COMMERCIAL GRADE. Focus: one module done exceptionally well. | High | DONE | senior-engineer-6 | yes |

Update the Status and Committed columns as phases complete. Commit tracker updates as
chore(...) commits.

---

## Phase 1 — Architecture doc (`senior-architect-1`)

**Deliverable:** `docs/architecture.md` (single source of truth for all design decisions).

**Must cover:**
- **C++-only service core** — why a C++ core (real-time GNSS math, performance), the
  process/service boundary, and how Python/Qt/LLM layers talk to it (adapter pattern).
- **Adapter pattern** — the integration layer that wraps external tools (Spirent, R&S,
  Skydel) via their remote-control/automation APIs rather than re-implementing RF.
- **SQLite persistence** — schema strategy, migration approach, and where the DB lives.
- **Real GNSS math** — GPS/Galileo/SBAS signal and scenario math lives in C++ core.
- **Monorepo layout** — the folder structure (see Phase 2) and module boundaries
  (ScenarioForge, TestForge, TraceLink, AssureCheck, RiskAI, IntegrateHub, CI/CD).
- **Qt Widgets UI** — desktop dashboard, cross-platform (Windows/Linux).
- **Cross-platform build** — CMake + toolchain notes for Windows and Linux.
- **CI/CD** — Jenkins pipeline shape (build, self-verify; no test agents in this loop).
- **Local LLMs** — how Qwen/Gemma integrate (RiskAI, AssureCheck) via the adapter layer.

**Acceptance criteria:** document exists, is internally consistent, and unblocks Phase 2
(folder structure) and Phase 3 (vertical slice) without ambiguity.

## Phase 2 — Monorepo scaffold (`senior-engineer-2`)

**Deliverable:** a compiling CMake build (Windows and Linux) with the full folder skeleton.

**Target layout:**
```
Lodestar/
├── CMakeLists.txt          # top-level, wires subprojects
├── cmake/                  # CMake helpers / toolchain
├── core/                   # C++ core (the service)
│   ├── CMakeLists.txt
│   ├── common/             # logging, config, utils
│   ├── persistence/        # SQLite schema + DAOs
│   ├── tracelink/          # graph model
│   ├── scenario/           # ScenarioForge (stub in this loop)
│   └── ...
├── python/                 # Python intelligence (stub in this loop)
├── ui/                     # Qt Widgets (stub in this loop)
├── docs/
└── ci/                     # Jenkins pipeline definitions
```

**Acceptance criteria:** `cmake -B build && cmake --build build` succeeds on the host
platform; the tree matches the layout above; empty/stub targets compile; CI folder has a
Jenkinsfile skeleton.

## Phase 3 — Phase 1 vertical slice (`senior-engineer-3`)

**Deliverable:** working C++ code for the first three core modules, self-verified by build.

- **core/common** — logging, configuration, common utilities used by the other modules.
- **core/persistence** — SQLite schema (tables for requirements, tests, trace links,
  scenarios) + DAO layer; schema creation/migration on startup.
- **core/tracelink** — graph model linking requirements, design, interfaces, and test
  cases; basic add/query operations backed by persistence.

**Acceptance criteria:** the three modules compile and link into the core target; a small
self-verifying smoke path (e.g. a main/CLI that opens the DB, creates the schema, and
inserts/queries a trace link) runs successfully. No test agents — verification is by
build + smoke run.

---

## Phase 4 — Detailed Plan (`planner`)

Full breakdown in `docs/phase4-plan.md`. Six top-level items, each with child items,
dependencies, and acceptance criteria:

1. **Orbit propagation** — 1.1 frame/time utilities (GPS/UTC/Julian, ECEF/ECI, WGS-84,
   geometry) · 1.2 Keplerian two-body (IS-GPS-200 broadcast ephemeris) · 1.3 SGP4/SDP4
   (Vallado, TLE-based LEO) · 1.4 satellite geometry (constellation views).
2. **RINEX parser** — 2.1 navigation (broadcast ephemeris) · 2.2 observation. Robust
   tokenization, header/record handling, line-numbered errors.
3. **NMEA generator** — 3.1 sentence builder + checksum · 3.2 emitters (GGA/RMC/GSA/GSV/ZDA).
4. **Pseudorange & Doppler** — 4.1 pseudorange (geometric range, Sagnac, relativistic) ·
   4.2 Doppler (range-rate).
5. **Error models** — 5.1 clock (receiver+satellite) · 5.2 ionosphere (Klobuchar) ·
   5.3 troposphere (Saastamoinen/Hopfield) · 5.4 configurable model selection.
6. **SBAS augmentation** — 6.1 message parsing (RTCA DO-229) · 6.2 fast/long-term
   corrections · 6.3 ionospheric grid corrections · 6.4 integrity (UDRE/GIVE, HPL/VPL).
7. **Scenario facade** — integration tying 1–6 into a single `Scenario` smoke-path entry.

**Build order:** 1.1 → (1.2, 1.3) → 1.4 → 2 → 5 → 4 → 3 → 6 → 7. All math self-contained in
`core/scenario` (no new external deps). Verification by build + smoke run.

---

## Definition of Done (per phase)

1. Deliverable produced per the phase's acceptance criteria.
2. Engineer/architect self-verifies (builds/compiles, runs smoke path) and reports "done".
3. Devops commits and pushes; updates the Status/Committed columns.
4. Orchestrator closes non-essential agents; scrum-master assigns the next phase.
