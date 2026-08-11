# Lodestar — Architecture

> **Single source of truth** for all design decisions in the Lodestar GNSS/SBAS
> Integrated Test & Verification Platform. This document unblocks Phase 2 (monorepo
> scaffold) and Phase 3 (vertical slice). Where a decision is referenced by another
> phase, the section is marked **[Phase 2]** or **[Phase 3]**.

## 0. System overview

Lodestar automates the full IT&V lifecycle for GNSS/SBAS systems — from scenario
generation to certification-ready reporting. It is a **monorepo** with a **C++-only
service core**, a **Python intelligence layer**, a **Qt Widgets desktop UI**, **SQLite**
persistence, **Jenkins CI/CD**, and **local LLMs** (Qwen/Gemma).

The platform is deliberately **NOT .NET**. The service layer is C++-only, exposing a thin
C++ REST API for external tools. This keeps the real-time GNSS math and the service
boundary in one language and one process model.

```
┌────────────────────────────────────────────────────────────────────┐
│                        Qt Widgets UI (desktop)                    │
│              ScenarioForge · TestForge · TraceLink · AssureCheck    │
│              RiskAI · IntegrateHub · CI/CD Metrics dashboard        │
└───────────────▲────────────────────────────────────────────────────┘
                │ Qt signals/slots + C++ service API (in-process)
┌───────────────┴────────────────────────────────────────────────────┐
│                    C++ Service Core (the service)                   │
│   common · persistence(SQLite) · tracelink · scenario · adapters   │
│   thin C++ REST API (external tools)                               │
└───────▲───────────────▲───────────────────────────▲─────────────────┘
        │               │                           │
        │ adapter       │ adapter                   │ adapter
┌───────┴──────┐ ┌──────┴────────┐ ┌───────────────┴──────────────┐
│ External RF  │ │ Python        │ │ Local LLMs (Qwen/Gemma)      │
│ tools        │ │ intelligence  │ │ RiskAI · AssureCheck         │
│ Spirent/R&S/ │ │ analysis+     │ │                              │
│ Skydel       │ │ reporting     │ │                              │
└──────────────┘ └───────────────┘ └──────────────────────────────┘
```

---

## 1. C++-only service core

### 1.1 Why a C++ core

- **Real-time GNSS math.** GPS/Galileo/SBAS signal generation, orbit propagation,
  pseudorange/Doppler computation, and scenario stepping must run at deterministic,
  low-latency rates. C++ provides the performance and control needed for this.
- **Single language for the service boundary.** The service layer, the math, and the
  persistence layer all live in C++. There is no .NET service layer; the thin REST API
  is C++ (e.g. a small embedded HTTP server such as cpp-httplib or a Qt-based listener).
- **Determinism and resource control.** C++ gives explicit memory and thread management,
  which matters for repeatable test scenarios and for running on both Windows and Linux.

### 1.2 Process / service boundary

- The **C++ core** is the authoritative service. It owns the SQLite database, the GNSS
  math, the trace graph, and the adapters.
- The **Qt Widgets UI** runs **in-process** with the core (same process, same address
  space) and talks to it through a **C++ service API** (a plain C++ interface, not IPC).
  This keeps the desktop dashboard fast and simple.
- The **Python intelligence layer** and **local LLMs** run **out-of-process** and talk to
  the core through the **thin C++ REST API** (HTTP/JSON). This keeps Python and LLM
  processes isolated from the real-time core.
- External tools (Spirent, R&S, Skydel) are reached **out-of-process** through the
  **adapter layer** (Section 2).

### 1.3 How Python / Qt / LLM layers talk to the core

| Layer | Process | Transport | Interface |
|-------|---------|-----------|-----------|
| Qt Widgets UI | in-process | direct C++ calls | C++ service API |
| Python intelligence | out-of-process | HTTP/JSON | thin C++ REST API |
| Local LLMs (Qwen/Gemma) | out-of-process | HTTP/JSON | thin C++ REST API |
| External RF tools | out-of-process | vendor remote-control | adapter layer |

The **adapter pattern** (Section 2) is the single integration mechanism used for both
external tools and the Python/LLM layers, so every external dependency is behind a
stable, testable boundary.

---

## 2. Adapter pattern

### 2.1 Purpose

Lodestar does **not** re-implement RF signal generation, MC/DC analysis, or vendor
hardware. Instead it **wraps** external tools through their remote-control / automation
APIs. This is the strategic integration layer that owns the IT&V workflow.

### 2.2 Adapter interface

Every adapter implements a common C++ interface so the core can treat all external tools
uniformly:

```cpp
// core/adapters/Adapter.h (conceptual)
class IAdapter {
public:
    virtual ~IAdapter() = default;
    virtual std::string name() const = 0;
    virtual bool connect(const AdapterConfig& cfg) = 0;
    virtual void disconnect() = 0;
    virtual AdapterStatus status() const = 0;
    virtual nlohmann::json invoke(const std::string& op,
                                  const nlohmann::json& params) = 0;
};
```

### 2.3 Concrete adapters

| Adapter | Target | Mechanism |
|---------|--------|-----------|
| `SpirentAdapter` | Spirent GSS9000 / SimGEN / PNT-Automation | vendor remote-control / automation API |
| `RsAdapter` | Rohde & Schwarz SMW200A + GNSS suites | vendor remote-control (SCPI / automation) |
| `SkydelAdapter` | Skydel software-defined GNSS | Skydel automation API |
| `LlmAdapter` | Qwen / Gemma (local) | HTTP/JSON to local model server |
| `PythonAdapter` | Python intelligence layer | HTTP/JSON to thin C++ REST API |

### 2.4 Why adapters, not re-implementation

- **Cost and correctness.** Vendor RF tools are proprietary, closed, and expensive.
  Wrapping them via their automation APIs is far cheaper and more correct than
  re-implementing RF.
- **Novel value stays in-house.** Genuinely novel value lives in **ScenarioForge**
  (software-defined GNSS scenario generation) and **RiskAI** (LLM-assisted FMEA), both
  of which are native modules, not adapters.
- **Testability.** Adapters are behind a common interface, so the core can be tested with
  a mock adapter when no hardware is present.

---

## 3. SQLite persistence

### 3.1 Where the DB lives

- The SQLite database file lives in a **user data directory** resolved at runtime
  (e.g. `~/.lodestar/lodestar.db` on Linux, `%APPDATA%/Lodestar/lodestar.db` on Windows).
  The path is configurable via the config module (`core/common`).
- The database is owned exclusively by the **C++ core**. No other process writes to it
  directly; Python/LLM/UI access it only through the core service API.

### 3.2 Schema strategy

- Schema is defined in **SQL migration files** under `core/persistence/migrations/`
  (e.g. `001_initial.sql`, `002_...sql`).
- Tables cover the Phase 3 vertical slice and the module domains:
  - `requirements` — requirements (TraceLink)
  - `design_items` — design elements (TraceLink)
  - `interfaces` — interface definitions (TraceLink)
  - `test_cases` — test cases (TestForge / TraceLink)
  - `trace_links` — edges in the trace graph (TraceLink)
  - `scenarios` — GNSS/SBAS scenarios (ScenarioForge)
  - `hazards` / `fmea` — risk data (RiskAI)
  - `assurance_checks` — compliance results (AssureCheck)
  - `integration_issues` — cross-team issues (IntegrateHub)
- A `schema_version` table records the current migration version.

### 3.3 Migration approach

- On startup, the core runs a **migration runner** (`core/persistence`) that:
  1. Reads the current `schema_version`.
  2. Applies each pending migration file in order, inside a transaction.
  3. Updates `schema_version` on success.
- Migrations are **append-only**; existing files are never edited. This is the single
  source of truth for schema evolution.

### 3.4 DAO layer

- A thin **DAO layer** (`core/persistence`) wraps SQLite access for each domain
  (e.g. `RequirementDao`, `TraceLinkDao`, `ScenarioDao`).
- The DAOs are used by the core modules (e.g. `core/tracelink`) and are the only code
  that issues SQL. **[Phase 3]**

---

## 4. Real GNSS math

- All GPS/Galileo/SBAS **signal and scenario math** lives in the **C++ core**, in the
  `core/scenario` module (ScenarioForge).
- This includes: constellation/orbit propagation, pseudorange and Doppler computation,
  signal-in-space modeling, SBAS augmentation and integrity parameters, and scenario
  time-stepping.
- The math is deterministic and runs in the core process so it can drive real-time test
  injection through the adapters.
- **ScenarioForge** is the native, software-defined GNSS scenario generator — the
  genuinely novel in-house capability. It does not re-implement vendor RF hardware; it
  produces scenario data that the adapters inject into the target tools.

---

## 5. Monorepo layout

The folder structure below is the **target for Phase 2**. Module boundaries map one-to-one
to the platform modules.

```
Lodestar/
├── CMakeLists.txt          # top-level, wires subprojects
├── cmake/                  # CMake helpers / toolchain files
├── core/                   # C++ core (the service)
│   ├── CMakeLists.txt
│   ├── common/             # logging, config, utils            [Phase 3]
│   ├── persistence/        # SQLite schema + DAOs              [Phase 3]
│   ├── tracelink/          # graph model (TraceLink)           [Phase 3]
│   ├── scenario/           # ScenarioForge (stub in this loop)
│   ├── testforge/          # TestForge (stub in this loop)
│   ├── assurecheck/        # AssureCheck (stub in this loop)
│   ├── riskai/             # RiskAI (stub in this loop)
│   ├── integratehub/       # IntegrateHub (stub in this loop)
│   ├── adapters/           # adapter pattern (Section 2)
│   └── api/                # thin C++ REST API (core/api)
├── python/                 # Python intelligence (stub in this loop)
├── ui/                     # Qt Widgets (stub in this loop)
├── docs/
└── ci/                     # Jenkins pipeline definitions
```

### 5.1 Module boundaries

| Module | Location | Responsibility |
|--------|----------|----------------|
| ScenarioForge | `core/scenario` | GNSS/SBAS scenario generation (real math) |
| TestForge | `core/testforge` | IT&V plan generation, execution, reporting |
| TraceLink | `core/tracelink` | Requirements/design/interface/test trace graph |
| AssureCheck | `core/assurecheck` | Compliance checks (ARP4754A, ARP4761, DO-178C, DO-278A, DO-254) |
| RiskAI | `core/riskai` | LLM-assisted hazard/FMEA analysis |
| IntegrateHub | `core/integratehub` | Cross-disciplinary data and issue resolution |
| CI/CD + Metrics | `ci/` + `ui/` | Jenkins pipeline and Qt metrics dashboard |

Each module is a **CMake library target** so Phase 2 can compile empty/stub targets and
Phase 3 can build real ones incrementally.

---

## 6. Qt Widgets UI

- The UI is a **Qt Widgets desktop dashboard** — one UI for all modules.
- It is **cross-platform** (Windows and Linux).
- The UI runs **in-process** with the C++ core and calls the **C++ service API**
  directly (no IPC), keeping the dashboard responsive.
- The dashboard hosts views for ScenarioForge, TestForge, TraceLink, AssureCheck,
  RiskAI, IntegrateHub, and the CI/CD metrics dashboard.
- The UI is a stub in this loop; it is wired in later phases.

---

## 7. Cross-platform build

- Build system is **CMake** (top-level `CMakeLists.txt` + per-module `CMakeLists.txt`).
- Toolchain notes:
  - **Windows:** MSVC (Visual Studio generator) or MinGW; SQLite via a bundled/installed
    library; Qt via `CMAKE_PREFIX_PATH` pointing at the Qt installation.
  - **Linux:** GCC/Clang (Unix Makefiles or Ninja); SQLite via system `libsqlite3-dev`;
    Qt via system or Qt-provided packages.
- `cmake/` holds helper modules and optional toolchain files.
- The build must succeed with `cmake -B build && cmake --build build` on the host
  platform (Phase 2 acceptance criterion).

---

## 8. CI/CD (Jenkins)

- **Jenkins** drives the pipeline. Pipeline definitions live in `ci/` (Jenkinsfile
  skeleton in Phase 2).
- Pipeline shape for this loop (steps 1–3):
  1. **Checkout** the monorepo.
  2. **Build** the C++ core (and stubs) with CMake on the host platform.
  3. **Self-verify** — run the smoke path (e.g. open DB, create schema, insert/query a
     trace link) to confirm the build works.
- **No test agents in this loop.** Verification is by build + smoke run, not a dedicated
  test phase. The pipeline reflects this: build + self-verify only.
- Later phases may add matrix builds for Windows/Linux and coverage/metrics reporting.

---

## 9. Local LLMs (Qwen / Gemma)

- Local LLMs (Qwen/Gemma) integrate through the **adapter layer** (`LlmAdapter`),
  out-of-process, via HTTP/JSON to a local model server.
- They are used by:
  - **RiskAI** — LLM-assisted hazard assessments and FMEA with risk mitigation tracking.
  - **AssureCheck** — LLM-assisted compliance analysis against the assurance standards.
- Because LLMs are behind the same `IAdapter` interface as external tools, the core can
  swap models, mock them in tests, and keep the LLM boundary stable.

---

## 10. Consistency notes / decisions summary

- **No .NET anywhere.** The service layer is C++-only; the thin REST API is C++.
- **One DB, one owner.** SQLite is owned by the C++ core; all other layers go through the
  service API.
- **One integration mechanism.** The adapter pattern is used for external RF tools, Python,
  and LLMs.
- **Real math in C++.** GNSS signal/scenario math is native C++ in `core/scenario`.
- **Module = CMake target.** Every module is a library target, enabling incremental
  Phase 2/3 builds.

## 11. Phase handoff

- **Phase 2 (monorepo scaffold):** create the folder tree in Section 5, top-level and
  per-module `CMakeLists.txt`, `cmake/` helpers, `ci/` Jenkinsfile skeleton. All module
  targets compile as empty/stub libraries.
- **Phase 3 (vertical slice):** implement `core/common`, `core/persistence` (SQLite
  schema + DAOs + migration runner), and `core/tracelink` (graph model backed by
  persistence). Self-verify with a smoke path that opens the DB, creates the schema, and
  inserts/queries a trace link.
