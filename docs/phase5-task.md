# Phase 5 — Adapters + Thin C++ REST API — Build Summary & Devops Task

> **Status: DONE (implemented + smoke-verified by orchestrator)**
> **Standard: COMMERCIAL GRADE.**
> **Owner of the commit: devops-5**

## What was built

The integration boundary of the platform is now in place: `core/adapters` (the
`IAdapter` pattern over external RF tools, local LLMs, and the Python intelligence
layer) and `core/api` (a thin embedded C++ REST/HTTP service over the core). Everything
outside the real-time core talks to it only through adapters + the REST API.

### Adapter layer — `core/adapters/`
- `Adapter.h` — `IAdapter`, `AdapterConfig`, `AdapterStatus`, and a typed
  `AdapterError` (codes: Unsupported, NotConnected, ConnectFailed, Network, Timeout,
  Protocol, InvalidParams, Internal).
- `AdapterRegistry.h/.cpp` — register/get adapters by name; unknown name -> typed
  `Unsupported` error. Keeps `module_version()` linkable.
- `MockAdapter` — no hardware; `ping` -> `{"ok":true,"result":"pong"}`; status connected.
- `LlmAdapter` — HTTP/JSON to a local Qwen/Gemma server (`complete`, `health`);
  unreachable host -> typed `Network` error.
- `PythonAdapter` — HTTP/JSON to the Python intelligence layer (`analyze`, `report`).
- `SpirentAdapter` — vendor remote control (`start`, `stop`, `setScenario`, `queryState`);
  not-connected -> typed error.
- `RsAdapter` — R&S SMW200A SCPI (`connect`, `sendScpi`, `setFreq`, `setLevel`).
- `SkydelAdapter` — Skydel automation API (`start`, `stop`, `setConstellation`).
- `HttpClient.h/.cpp` — minimal blocking HTTP/1.1 client over Winsock/BSD sockets
  (used by the network adapters; no external HTTP dependency).
- `Json.h/.cpp` — self-contained JSON value type (already present; kept as the JSON
  choice for the whole adapter/API layer).

### Thin C++ REST API — `core/api/`
- `HttpServer.h/.cpp` — minimal embedded HTTP/1.1 server over Winsock/BSD sockets
  (chosen instead of vendoring cpp-httplib, per the plan's "otherwise implement a
  minimal listener" option). Method+path routing with `<name>` path params, request
  body parsing, HTTP status + JSON responses. Exception-safe accept loop.
- `ApiServer.h/.cpp` — wires the registry + a built 6-satellite GPS `Scenario` to the
  endpoints. Keeps `module_version()` linkable.
- Endpoints: `GET /health`, `GET /adapters`, `POST /adapters/<name>/invoke`,
  `GET /scenario/satellites`, `POST /scenario/step`. Error model: 200 ok, 400 bad
  request, 404 unknown adapter/route, 500 internal, body
  `{"error":{"code":...,"message":...}}`.

### CMake + smoke
- `core/CMakeLists.txt` — replaced the `adapters/stub.cpp` and `api/stub.cpp` stub
  registrations with real `lodestar_adapters` and `lodestar_api` targets (link `ws2_32`
  on Windows; `lodestar_api` links `lodestar_adapters` + `lodestar_scenario`).
- `core/smoke/adapters_api_smoke.cpp` — wired into `lodestar_smoke` (called from
  `smoke/main.cpp`). Exercises registry + mock invoke + unknown-adapter error +
  LlmAdapter network error + Spirent not-connected error + all API endpoints.

## Verification (done by orchestrator)
`cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/Users/kutay/vcpkg/scripts/buildsystems/vcpkg.cmake`
then `cmake --build build --config Debug` succeeds; `./build/core/Debug/lodestar_smoke.exe`
exits 0 and prints:
- `SMOKE OK: schema v1, trace link insert+query round-trip passed.`
- `SCENARIO SMOKE OK` (Phase 4 still passes)
- `ADAPTERS+API SMOKE OK` (Phase 5: 17/17 checks PASS)
  - registry + mock invoke + unknown-adapter error ✓
  - `GET /health` -> 200 ✓, unknown-route -> 404 ✓, unknown-adapter -> 404 ✓,
    missing-op -> 400 ✓, `POST /adapters/mock/invoke` -> 200 ✓,
    `GET /scenario/satellites` -> 200 ✓, `POST /scenario/step` -> 200 ✓

---

## Devops-5 task

Commit and push ALL Phase 5 changes to the `main` remote. Include:

1. New files: `core/adapters/Adapter.h`, `AdapterRegistry.h/.cpp`, `MockAdapter.h/.cpp`,
   `LlmAdapter.h/.cpp`, `PythonAdapter.h/.cpp`, `SpirentAdapter.h/.cpp`,
   `RsAdapter.h/.cpp`, `SkydelAdapter.h/.cpp`, `HttpClient.h/.cpp`,
   `core/adapters/Json.h` + `Json.cpp`, `core/api/HttpServer.h/.cpp`,
   `core/api/ApiServer.h/.cpp`, `core/smoke/adapters_api_smoke.cpp`,
   `docs/phase5-plan.md`, `docs/phase5-scrum.md`, `docs/phase5-task.md`.
2. Modified files: `core/CMakeLists.txt`, `core/smoke/main.cpp`.
3. Update the `PLAN.md` Phase 5 row: set `Status` to `DONE` and `Committed` to `yes`.

Commit with a conventional message, e.g.
`feat(core): add adapters + thin C++ REST API (Phase 5)`.

Then push to `origin main`. Reply `DONE devops-5` to the orchestrator when the push
lands.
