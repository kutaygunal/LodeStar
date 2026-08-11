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

## Definition of Done (per phase)

1. Deliverable produced per the phase's acceptance criteria.
2. Engineer/architect self-verifies (builds/compiles, runs smoke path) and reports "done".
3. Devops commits and pushes; updates the Status/Committed columns.
4. Orchestrator closes non-essential agents; scrum-master assigns the next phase.
