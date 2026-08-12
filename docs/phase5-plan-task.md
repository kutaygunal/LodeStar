# Planner Task — Phase 5 (Adapters + thin C++ REST API)

You are the **planner** for the Lodestar GNSS/SBAS platform. Produce a DETAILED,
COMMERCIAL-GRADE plan for **Phase 5: Adapters + thin C++ REST API**.

## Context

- Repo: `C:/Users/kutay/Desktop/Projects/Lodestar`
- Read `PLAN.md` (Phase 5 row is TODO) and `docs/architecture.md` Section 2 (Adapter
  pattern) and Section 1 (C++-only service core + thin C++ REST API).
- The modules live in `core/adapters` and `core/api` (each currently `stub.cpp`).
- Phases 1-4 are DONE. `core/scenario` (ScenarioForge) has real GNSS math.
- This is COMMERCIAL GRADE: production-quality C++, proper error handling, no toy code.

## Your deliverable

Think through the items for Phase 5 with child items where needed. Do NOT stop at a shallow
list. For each item specify: purpose/scope, key classes, public API, error handling,
dependencies on other items, and acceptance criteria (build + smoke run).

The 2 top-level areas are:

1. **Adapter layer (`core/adapters`)** — implement the `IAdapter` interface (from
   architecture.md Section 2.2) and concrete adapters:
   - `SpirentAdapter` (Spirent GSS9000 / SimGEN / PNT-Automation)
   - `RsAdapter` (Rohde & Schwarz SMW200A, SCPI)
   - `SkydelAdapter` (Skydel automation API)
   - `LlmAdapter` (Qwen/Gemma, HTTP/JSON to local model server)
   - `PythonAdapter` (Python intelligence layer, HTTP/JSON)
   - A `MockAdapter` for tests/smoke (so the core can be tested with no hardware).
   - Include: adapter registry, adapter config, status reporting.

2. **Thin C++ REST API (`core/api`)** — a small embedded HTTP/JSON service exposing the
   core over HTTP. Requirements:
   - Embedded HTTP server (e.g. cpp-httplib header-only, or a minimal custom listener —
     justify the choice; must be self-contained / no heavy deps beyond what's reasonable).
   - JSON serialization/parsing (e.g. nlohmann/json header-only).
   - Endpoints over the core: health, list adapters, invoke an adapter op, and basic
     ScenarioForge access (e.g. list scenario satellites, step an epoch).
   - Authentication/error handling (proper HTTP status codes, JSON error bodies).

For each top-level area, break into child items (e.g. "1.1 IAdapter interface",
"1.2 SpirentAdapter", ..., "2.1 HTTP server", "2.2 JSON", "2.3 scenario endpoints").
Identify dependencies and a recommended build order.

## Output

- Update `PLAN.md`: set Phase 5 Status to `PLANNED`, Assigned to `planner` (keep), and add a
  "Phase 5 — Detailed Plan" section.
- Write the full detailed plan to `docs/phase5-plan.md`.

Do NOT implement code. Do NOT commit. Reply `DONE planner`.
