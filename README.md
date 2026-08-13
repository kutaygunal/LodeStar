<div align="center">

# 🛰️ Lodestar — GNSS/SBAS Integrated Test & Verification Platform

A modern engineering platform that automates the full **IT&V lifecycle** for GNSS/SBAS
systems — from scenario generation to certification-ready reporting — with built-in
DO-178C / ARP4754A / ARP4761 compliance, FMEA/hazard analysis, and cross-disciplinary integration.

[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-Build-064F8C?style=for-the-badge&logo=cmake&logoColor=white)](https://cmake.org/)
[![SQLite](https://img.shields.io/badge/SQLite-Persistence-003B57?style=for-the-badge&logo=sqlite&logoColor=white)](https://www.sqlite.org/)
[![Jenkins](https://img.shields.io/badge/Jenkins-CI%2FCD-D24939?style=for-the-badge&logo=jenkins&logoColor=white)](https://www.jenkins.io/)
[![Qt](https://img.shields.io/badge/Qt-Desktop-41CD52?style=for-the-badge&logo=qt&logoColor=white)](https://www.qt.io/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](LICENSE.md)

**📦 Repository:** [github.com/kutaygunal/Lodestar](https://github.com/kutaygunal/Lodestar)

</div>

---

## 📖 Overview

Lodestar is a **monorepo** with a C++-only service core, a thin C++ REST API for external
tools, SQLite persistence, a Qt Widgets desktop dashboard, Jenkins CI/CD, and local LLMs
(Qwen/Gemma). The platform deliberately avoids .NET — the real-time GNSS math and the
service boundary live in one language and one process model.

The core wraps external RF tools (Spirent, Rohde & Schwarz, Skydel) through a single
adapter interface instead of re-implementing proprietary hardware. Genuinely novel value —
software-defined GNSS scenario generation and LLM-assisted FMEA — stays in-house as native
modules.

**What it does:**
1. Generates realistic GPS/Galileo/SBAS RF and data scenarios for test injection
2. Auto-builds IT&V plans and designs black-box test cases from requirements
3. Links requirements ↔ design ↔ interfaces ↔ tests in a full traceability graph
4. Runs compliance checks against assurance standards and scores code coverage
5. Assists hazard/FMEA and requirement-quality analysis with local LLMs
6. Coordinates cross-disciplinary issues and produces certification-ready reports

---

## 🏗️ Architecture

A **C++ service core** owns the SQLite database, the GNSS math, the trace graph, and the
adapters. The Qt Widgets UI runs **in-process** with the core; Python intelligence and local
LLMs run **out-of-process** through the thin REST API.

```
Qt Widgets UI (desktop dashboard)
   ScenarioForge · TestForge · TraceLink · AssureCheck · RiskAI · IntegrateHub
        │  Qt signals/slots + C++ service API (in-process)
        ▼
C++ Service Core (the service)
   common · persistence(SQLite) · tracelink · scenario · adapters · thin C++ REST API
        │                    │                     │
        ▼ adapter            ▼ adapter             ▼ adapter
 External RF tools        Python               Local LLMs
 Spirent/R&S/Skydel       intelligence         Qwen/Gemma
```

| Layer | Tech | Responsibility |
|-------|------|----------------|
| `core/scenario` | C++17 | **ScenarioForge** — GNSS/SBAS scenario generation, real orbit/PVT/SBAS math |
| `core/testforge` | C++17 | **TestForge** — IT&V plan generation, test-case design, MC/DC coverage, reporting |
| `core/tracelink` | C++17 | **TraceLink** — requirements/design/interface/test trace graph, baselines, reviews |
| `core/assurecheck` | C++17 | **AssureCheck** — compliance engine for ARP4754A, ARP4761, DO-178C, DO-278A, DO-254 |
| `core/riskai` | C++17 | **RiskAI** — LLM-assisted FMEA and requirement-quality scoring |
| `core/integratehub` | C++17 | **IntegrateHub** — cross-disciplinary issue and coordination model |
| `core/adapters` | C++17 | Adapter pattern for Spirent, R&S, Skydel, LLM, Python, Mock |
| `core/api` | C++17 | Thin C++ REST API + web browser review layer |
| `core/persistence` | SQLite | 27 migration files, DAO layer, FTS5 search, RBAC, audit |
| `ui/` | Qt Widgets | Cross-platform desktop dashboard (in-process) |
| `ci/` | Jenkins | Windows matrix builds, CTest + JUnit XML, coverage |
| `python/` | Python | Intelligence/analysis layer (out-of-process) |

---

## ✨ Main Features

- **ScenarioForge** — software-defined GNSS/SBAS scenario generation with real C++
  math: Keplerian/SGP4 orbit propagation, TLE, RINEX nav/obs parsing, pseudorange and
  Doppler computation, PVT solver, SBAS integrity/corrections/messages, ionosphere and
  troposphere models, RF impairments, baseband, and an SCPI-style automation API.
- **TestForge** — auto-generate IT&V test procedures from a scenario and measurement
  checks; black-box test design via equivalence partitioning and boundary-value
  analysis; structural coverage (statement / decision / MC/DC) with Cobertura import and
  real OpenCppCoverage instrumentation; plan and report generation.
- **TraceLink** — full traceability graph over requirements, design items, interfaces,
  and test cases; typed link integrity-on-write, status state-machine, suspect-link
  detection, duplicate clustering, FTS5 full-text search, baseline diff, change requests,
  and review/approval workflow.
- **AssureCheck** — automated compliance evaluation against DO-178C (Table A-1..A-x),
  ARP4754A, ARP4761, DO-278A and DO-254 checklists; DAL-level (A–E) applicability;
  evidence linking, review/approval/sign-off workflow, dashboards, and evidence packages.
- **RiskAI** — LLM-assisted FMEA (failure mode / effect / severity × likelihood risk) with
  a deterministic fallback, plus five-dimension requirement-quality scoring (clarity,
  testability, atomicity, completeness, ambiguity).
- **IntegrateHub** — cross-disciplinary issue model (Systems / Software / Hardware / Test /
  Safety) with coordination notes, persisted to SQLite.
- **REST API + Web review** — thin C++ HTTP/JSON API (TraceLink, AssureCheck routes) and
  a browser read-review layer (requirements, trace matrix, compliance summary) with
  session-token auth and RBAC.
- **Security & governance** — API-key service, RBAC roles, audit/baseline tracking, and
  user sessions.
- **CI/CD** — Jenkins Windows matrix (Release/Debug) build, CTest wired to every test
  target with JUnit XML reporting, and coverage measurement.

---

## 🖥️ Suggested Screens (seven)

System Overview · ScenarioForge (GNSS Scenario) · TestForge (Plan & Coverage) ·
TraceLink (Traceability Matrix) · AssureCheck (Compliance Dashboard) · RiskAI (FMEA Review) · IntegrateHub (Issues)

---

## 🗂️ Repository Layout

```
CMakeLists.txt, CMakePresets.json, vcpkg.json    # cross-platform build (MSVC + vcpkg)
core/common                # logging, config, Result, UUID, SHA-256, version
core/persistence/          # SQLite database, DAOs, migration runner + 27 migrations
core/tracelink/            # trace graph, baselines, rules, reviews, OSLC, RBAC
core/scenario/             # ScenarioForge: orbit, rinex, pvt, sbas, nmea, frames, errors
core/testforge/            # TestForge: coverage, plan/report generation, Cobertura import
core/assurecheck/          # compliance engine, standards, reports, dashboards, evidence
core/riskai/               # FMEA + requirement-quality (LLM-assisted)
core/integratehub/         # cross-disciplinary issues
core/adapters/             # Spirent, R&S, Skydel, LLM, Python, Mock adapters
core/api/                  # embedded HTTP server, REST + web review layer
core/test/                 # 50+ phase/work-package test suites
ui/                        # Qt Widgets desktop app (main.cpp + view stubs)
python/                    # Python intelligence layer (out-of-process)
ci/                        # Jenkinsfile, run_all_tests, run_coverage
packaging/                 # installer.ps1 staging + distributable bundle
docs/                      # architecture, user-guide, support, standards checklist
```

See `PLAN.md` for the phased build plan and `docs/architecture.md` for the design
decision record.

---

## 🎯 Why Lodestar / Who It's For

Lodestar is the **single, automated IT&V workbench for GNSS/SBAS systems** — aimed at
avionics, aerospace, and PNT test teams that must move from scenario generation to
certification-ready evidence. One tool for software-defined scenario generation, test-plan
automation, requirements traceability, structural coverage, assurance-standard compliance,
LLM-assisted risk/FMEA analysis, and cross-disciplinary integration — built on a
deterministic C++ core that runs on Windows and Linux and reaches real RF hardware through
vendor adapters.

---

## 🧪 Demo / Work Packages

The repository ships as a series of work packages (WP-1 … WP-F) covering the full feature
surface, each with its own acceptance database and test suite — TraceLink entity/link
CRUD, suspect-link and review workflows, AssureCheck compliance and dashboards, audit and
RBAC, coverage, API routes, and UI view models. Each WP database and its `*_tests.cpp`
suite demonstrate the module end-to-end against a real SQLite store.

---

## 📄 License

MIT — see [`LICENSE.md`](LICENSE.md).

---

<div align="center">
  Made with ❤️ for precision navigation & safer skies
</div>
