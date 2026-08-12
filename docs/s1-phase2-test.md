# S1 Phase 2 Test Contract — Functional Adapters (real Skydel + LLM invoke)

> Written by the scrum-master BEFORE the Phase 2 engineer implements the feature.
> The engineer must implement the contract below so the adapters perform REAL
> out-of-process calls. Do NOT weaken the assertions to make them pass; implement
> the feature to satisfy them. This is a TEST CONTRACT, not a testing task.
>
> **Scope:** Sprint 1 Phase 2 (PLAN.md). Deliverable = one end-to-end RF injection
> (or simulated) + a real LLM call. The adapters currently only `connect()` and return
> canned JSON from `invoke()`; this phase makes `invoke()` perform real HTTP calls.
> Phase 3 (RiskAI) and Phase 5 (RT/determinism) depend on this phase's LLM and Skydel
> halves respectively.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)

```bash
# 1. Configure with tests enabled.
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON

# 2. Build the adapter tests (HARD TIMEOUT).
timeout 600 cmake --build build --config Release --target lodestar_s1_phase2_tests

# 3. Run the adapter tests (HARD TIMEOUT).
timeout 120 ./build/core/Release/lodestar_s1_phase2_tests.exe
```

## Test file
- **File:** `core/test/s1_phase2_tests.cpp`
- **CMake target:** `lodestar_s1_phase2_tests`
- **Links:** `lodestar_common`, `lodestar_adapters` (+ `ws2_32` on Windows)
- **Binary:** `./build/core/Release/lodestar_s1_phase2_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (prints `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures). No DB required.

## Contract the Phase 2 engineer must provide

### (A) `LlmAdapter::invoke("complete", ...)` performs a REAL HTTP call
- `doComplete(params)` must POST to the configured model server (default Ollama at
  `127.0.0.1:11434/api/generate`) via `HttpClient`, send `{"model","prompt","stream":false}`,
  and return a `Json` with `ok`, `status`, and a parsed `reply`.
- `doHealth()` must GET the model server and return `ok`/`status`/`body`.
- On transport failure (unreachable host / timeout) it must throw
  `AdapterError(Network|Timeout)` and set `status_.state = Error`.

### (B) `SkydelAdapter::invoke("start"|"stop"|"setConstellation", ...)` performs a REAL call
- `invoke()` must POST to the configured Skydel automation endpoint (default
  `127.0.0.1:8081`) via `HttpClient` and return the vendor response as `Json`.
- When no real Skydel is reachable, the engineer must provide a **simulated** mode
  (e.g. a `simulate` config param) that returns a realistic vendor-shaped response so
  the end-to-end RF-injection path is exercised without hardware.
- On transport failure it must throw `AdapterError(Network|Timeout)` and set
  `status_.state = Error`.

### (C) End-to-end RF injection (or simulated) + real LLM call
- The test must drive one full RF-injection sequence: `connect` → `invoke("start")` →
  `invoke("setConstellation", ...)` → `invoke("stop")`, and one real LLM call:
  `connect` → `invoke("complete", {prompt})` → assert a non-empty `reply`.
- The LLM call must hit a real local model server (Ollama). If no server is running, the
  test must report a clear `[SKIP]` for the live-LLM assertion but still PASS the
  transport-error and simulated-path assertions, so the suite is deterministic in CI.

## Test cases & expected behavior

### T1. LlmAdapter health against a live server returns ok
- `connect({host:"127.0.0.1", port:11434})`; `invoke("health")`.
- **Expect:** `ok == true`, `status == 200`. (If no server: `[SKIP]`, not FAIL.)

### T2. LlmAdapter complete returns a non-empty reply
- `connect({host:"127.0.0.1", port:11434})`; `invoke("complete", {prompt:"Say hi"})`.
- **Expect:** `ok == true`; `reply` is present and non-empty. (If no server: `[SKIP]`.)

### T3. LlmAdapter unreachable host → typed network error
- `connect({host:"127.0.0.1", port:1, timeoutMs:1000})`; `invoke("complete", ...)`.
- **Expect:** throws `AdapterError` with `code == Network` or `Timeout`; `status().failed()`.

### T4. SkydelAdapter simulated end-to-end RF injection
- `connect({host:"127.0.0.1", port:8081, params:{"simulate":"1"}})`.
- `invoke("start")` → `ok == true`.
- `invoke("setConstellation", {constellation:"GPS"})` → `ok == true`, echoes the params.
- `invoke("stop")` → `ok == true`.
- **Expect:** all three ops return `ok == true` and the sequence completes without error.

### T5. SkydelAdapter unreachable host → typed network error
- `connect({host:"127.0.0.1", port:1, timeoutMs:1000})` (no simulate); `invoke("start")`.
- **Expect:** throws `AdapterError` with `code == Network` or `Timeout`; `status().failed()`.

### T6. SkydelAdapter not connected → typed NotConnected error
- Fresh `SkydelAdapter`; `invoke("start", ...)` without `connect()`.
- **Expect:** throws `AdapterError` with `code == NotConnected`.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
# --- S1 Phase 2: functional adapters (real Skydel + LLM invoke) -----------
add_executable(lodestar_s1_phase2_tests
    test/s1_phase2_tests.cpp)
target_link_libraries(lodestar_s1_phase2_tests PRIVATE
    lodestar_common
    lodestar_adapters)
if(WIN32)
    target_link_libraries(lodestar_s1_phase2_tests PRIVATE ws2_32)
endif()
```

> Note: T1/T2 require a live Ollama server and are marked `[SKIP]` when absent so the
> suite stays deterministic. T3–T6 are fully deterministic and must always run.
