# Phase 5 — Adapters + Thin C++ REST API — Task List

> **Owner:** orchestrator · **Status:** PLANNED → IN PROGRESS · **Assigned to:** senior-engineer-5
> **Source:** `docs/phase5-plan.md` · **Standard:** COMMERCIAL GRADE.

## Assignment
One engineering task for a single engineer (items share `IAdapter`/JSON types). Implement in
build order below.

## Tasks

**P5-1.1 — Adapter core (`IAdapter`, `AdapterConfig`, `AdapterStatus`, `AdapterRegistry`)**
Scope: `core/adapters/Adapter.h`, `AdapterRegistry.h/.cpp`.
Acceptance: registry add/get by name; unknown name -> typed error.

**P5-1.2 — `MockAdapter`**
Scope: `core/adapters/MockAdapter.h/.cpp`.
Acceptance: connects, invokes `ping` -> ok JSON; status connected.

**P5-1.3 — `LlmAdapter` (HTTP/JSON to local model server)**
Scope: `core/adapters/LlmAdapter.h/.cpp`.
Acceptance: `complete` posts a request and returns the model reply; unreachable host -> typed error.

**P5-1.4 — `PythonAdapter` (HTTP/JSON)**
Scope: `core/adapters/PythonAdapter.h/.cpp`.
Acceptance: `analyze` posts and returns a JSON report; unreachable host -> typed error.

**P5-1.5 — `SpirentAdapter`**
Scope: `core/adapters/SpirentAdapter.h/.cpp`.
Acceptance: `start`/`stop`/`queryState` return status JSON; not connected -> typed error.

**P5-1.6 — `RsAdapter` (SCPI)**
Scope: `core/adapters/RsAdapter.h/.cpp`.
Acceptance: `sendScpi`/`setFreq`/`setLevel` return ok; not connected -> typed error.

**P5-1.7 — `SkydelAdapter`**
Scope: `core/adapters/SkydelAdapter.h/.cpp`.
Acceptance: `start`/`setConstellation` return ok; not connected -> typed error.

**P5-2.1 — HTTP server + JSON**
Scope: `core/api/HttpServer.h/.cpp`, `third_party/` (cpp-httplib + nlohmann/json vendored).
Acceptance: router dispatches GET/POST to handlers; returns HTTP status + JSON body.

**P5-2.2 — API endpoints**
Scope: `core/api/ApiServer.h/.cpp`.
Acceptance: `GET /health`, `GET /adapters`, `POST /adapters/<name>/invoke`,
`GET /scenario/satellites`, `POST /scenario/step`. Unknown route -> 404; bad op -> 400.

**P5-3 — CMake + smoke**
Scope: `core/CMakeLists.txt`, `core/smoke/adapters_api_smoke.cpp`, `core/smoke/main.cpp`.
Acceptance: build succeeds; smoke prints PASS for registry + mock invoke + unknown-adapter
error + `/health` 200 + unknown-route 404; existing Phase 3/4 smoke still passes.

## Build order
P5-1.1 → 1.2 → 1.3/1.4/1.5/1.6/1.7 → 2.1 → 2.2 → 3.
