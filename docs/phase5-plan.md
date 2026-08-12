# Phase 5 — Adapters + Thin C++ REST API — Detailed Plan

> **Owner:** orchestrator · **Status:** PLANNED · **Modules:** `core/adapters`, `core/api`
> **Standard:** COMMERCIAL GRADE — production C++, proper error handling.
> **Verification:** build + smoke run (no test agents). `docs/architecture.md` Sections 1 & 2.

This is the integration boundary of the platform. It unblocks TestForge/AssureCheck/RiskAI
and external-tool + Python/LLM integration. The core owns SQLite + GNSS math; everything
else talks to it only through adapters + the thin C++ REST API.

---

## Item 1 — Adapter layer (`core/adapters`)

**Purpose:** Wrap external tools (RF generators, local LLMs, Python intelligence) behind one
stable `IAdapter` interface so the core treats all external dependencies uniformly and can be
tested with a mock when no hardware is present.

### 1.1 `IAdapter` interface + registry + config
- **Classes:** `IAdapter` (abstract), `AdapterConfig`, `AdapterStatus`, `AdapterRegistry`.
- **API** (from architecture.md 2.2):
  ```cpp
  class IAdapter {
  public:
      virtual ~IAdapter() = default;
      virtual std::string name() const = 0;
      virtual bool connect(const AdapterConfig& cfg) = 0;
      virtual void disconnect() = 0;
      virtual AdapterStatus status() const = 0;
      virtual nlohmann::json invoke(const std::string& op, const nlohmann::json& params) = 0;
  };
  ```
- `AdapterRegistry` registers adapters by name and looks them up; `AdapterConfig` holds
  endpoint/host/port/auth; `AdapterStatus` holds connected/error + last error message.
- **Error handling:** registry lookup of an unknown adapter -> typed error; connect failure
  returns false + status.error.

### 1.2 Concrete adapters
- **`SpirentAdapter`** — wraps Spirent GSS9000/SimGEN/PNT-Automation remote-control. Ops:
  `start`, `stop`, `setScenario`, `queryState`.
- **`RsAdapter`** — wraps Rohde & Schwarz SMW200A via SCPI. Ops: `connect`, `sendScpi`,
  `setFreq`, `setLevel`.
- **`SkydelAdapter`** — wraps Skydel automation API. Ops: `start`, `stop`, `setConstellation`.
- **`LlmAdapter`** — HTTP/JSON to a local Qwen/Gemma model server. Ops: `complete`, `health`.
- **`PythonAdapter`** — HTTP/JSON to the Python intelligence layer. Ops: `analyze`,
  `report`.
- **`MockAdapter`** — no hardware; returns canned success/values so the core and the smoke
  path run without any external dependency.

### 1.3 Error handling
- Every `invoke` returns an `nlohmann::json` that distinguishes `ok` from `error` (or the
  adapter stores a status + throws `ScenarioError`-style typed errors on hard failures).
- Unknown op -> typed `Unsupported` error. Network/timeout failures -> typed error with
  message.

### Acceptance
Smoke path constructs the registry, registers a `MockAdapter` and a `LlmAdapter` (pointing at
a dummy endpoint), invokes a known op successfully, and invokes an unknown adapter/op and gets
a typed error.

---

## Item 2 — Thin C++ REST API (`core/api`)

**Purpose:** Expose the core over HTTP/JSON for out-of-process Python/LLM/UI and external
tools. Small embedded server; C++ only.

### 2.1 Embedded HTTP server
- **Choice:** `cpp-httplib` (header-only, no build dep) OR a minimal custom HTTP/1.1 listener
  using winsock/BSD sockets. **Decision:** use cpp-httplib via a vendored header if available;
  otherwise implement a minimal listener. Document the choice in code.
- Provides routing (method + path), request body parsing, response with HTTP status.

### 2.2 JSON
- **Choice:** `nlohmann/json` (header-only). Used by adapters and the API. Verify it can be
  fetched/vendored; if offline, implement a small JSON parse/emit utility. Document the choice.

### 2.3 Endpoints
- `GET /health` -> `{"status":"ok","version":N}`.
- `GET /adapters` -> list registered adapters + status.
- `POST /adapters/<name>/invoke` -> `{"op":...}` -> adapter result JSON.
- `GET /scenario/satellites` -> count/PRNs of a built scenario (ScenarioForge).
- `POST /scenario/step` -> step one epoch, return PVT + protection levels + NMEA stream.

### 2.4 Error handling
- Proper HTTP status codes: 200 ok, 400 bad request, 404 unknown adapter/route, 500 internal.
- JSON error body: `{"error":{"code":..., "message":...}}`.

### Acceptance
Smoke path starts the server on an ephemeral port, issues `GET /health` and
`POST /adapters/mock/invoke`, and verifies 200 + expected JSON. Unknown route returns 404.

---

## Item 3 — CMake + smoke integration

- Register `lodestar_adapters` and `lodestar_api` targets with the new sources (replace the
  `stub.cpp` registrations), keep `module_version()` linkable.
- Extend `core/smoke` with `adapters_api_smoke.cpp` wired into `lodestar_smoke`.
- No new external build-time deps required (header-only libs vendored under `third_party/` if
  needed). Verify `cmake -B build && cmake --build build` succeeds.

---

## Build order
1. `IAdapter` + `AdapterConfig`/`AdapterStatus` + `AdapterRegistry` + `MockAdapter`
2. Concrete adapters (Llm, Python, Spirent, Rs, Skydel)
3. JSON + HTTP server
4. API endpoints
5. CMake + smoke

## Phase-level acceptance
1. Build succeeds with the new `core/adapters` + `core/api` sources.
2. Smoke prints PASS for: registry + mock invoke + unknown-adapter error + `GET /health` 200 +
   unknown-route 404.
3. No new heavy external dependencies (header-only libs vendored).
4. `PLAN.md` Phase 5 row updated to `DONE` by devops when committed.
