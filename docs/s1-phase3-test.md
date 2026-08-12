# S1 Phase 3 Test Contract — RiskAI first slice (hazard → LLM → FMEA table)

> Written by the scrum-master BEFORE the Phase 3 engineer implements the feature.
> The engineer must implement the contract below so RiskAI turns a hazard description
> into a structured FMEA table via a real LLM call. Do NOT weaken the assertions to make
> them pass; implement the feature to satisfy them. This is a TEST CONTRACT, not a
> testing task.
>
> **Scope:** Sprint 1 Phase 3 (PLAN.md). Deliverable = working LLM-assisted FMEA.
> `core/riskai/stub.cpp` is currently a 5-line placeholder; this phase replaces it with a
> real `RiskAiService`. Phase 3 **depends on Phase 2** (the LLM adapter's real
> `invoke("complete", ...)`), which is DONE and committed.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)

```bash
# 1. Configure with tests enabled.
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON

# 2. Build the RiskAI tests (HARD TIMEOUT).
timeout 600 cmake --build build --config Release --target lodestar_s1_phase3_tests

# 3. Run the RiskAI tests (HARD TIMEOUT).
timeout 120 ./build/core/Release/lodestar_s1_phase3_tests.exe
```

## Test file
- **File:** `core/test/s1_phase3_tests.cpp`
- **CMake target:** `lodestar_s1_phase3_tests`
- **Links:** `lodestar_common`, `lodestar_adapters`
- **Binary:** `./build/core/Release/lodestar_s1_phase3_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (prints `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures). No DB required.

## Contract the Phase 3 engineer must provide

### (A) `RiskAiService` (new, `core/riskai/RiskAiService.h`)
A service that takes a hazard description, calls the LLM adapter, and returns a
structured FMEA table. It must provide:

```cpp
namespace lodestar::riskai {

// One FMEA row.
struct FmeaRow {
    std::string failureMode;   // how the hazard manifests
    std::string effect;        // consequence on the system
    int severity = 0;          // 1..10
    int likelihood = 0;       // 1..10
    int risk = 0;             // severity * likelihood
};

class RiskAiService {
public:
    // llm is the Phase-2 LlmAdapter (already connected). cfg carries the model
    // name and any prompt-tuning params.
    explicit RiskAiService(adapters::IAdapter& llm, const adapters::AdapterConfig& cfg);

    // Run FMEA on a hazard. Returns the FMEA table. On a live-LLM failure it
    // falls back to a deterministic canned table so callers always get rows.
    common::Result<std::vector<FmeaRow>> analyze(const std::string& hazard);
};

}
```

- `analyze(hazard)` must build a prompt, call `llm.invoke("complete", {model, prompt})`,
  parse the returned `reply` into FMEA rows, and compute `risk = severity * likelihood`.
- **Deterministic fallback:** when the LLM call throws `AdapterError` (no live server,
  network, timeout) or the reply cannot be parsed, `analyze` must return a **canned FMEA
  table** (at least 2 rows with valid severity/likelihood/risk) instead of failing. This
  guarantees the suite always has rows to assert on.

### (B) Live-LLM path
- When a live Ollama server is reachable, `analyze` must return rows parsed from the real
  model reply. The test marks this assertion `[SKIP]` when no server is running, so the
  suite stays deterministic in CI.

## Test cases & expected behavior

### T1. analyze returns a non-empty FMEA table (deterministic fallback)
- Build a `RiskAiService` over a `MockAdapter` (or an LLM adapter pointed at an
  unreachable host so it throws).
- `analyze("GPS signal loss during approach")`.
- **Expect:** returns at least 2 rows; every row has `severity` in 1..10, `likelihood`
  in 1..10, and `risk == severity * likelihood`.

### T2. risk is computed as severity * likelihood
- From T1's result, for each row assert `row.risk == row.severity * row.likelihood`.
- **Expect:** all rows satisfy the identity.

### T3. analyze against a live LLM returns parsed rows
- `connect` a real `LlmAdapter` to `127.0.0.1:11434`; `analyze("GPS signal loss")`.
- **Expect:** returns rows parsed from the model reply (non-empty). If no server is
  running, print `[SKIP]` and do not count a failure.

### T4. analyze never throws on LLM failure
- Point the LLM adapter at an unreachable host (`127.0.0.1:1`, `timeoutMs:1000`).
- `analyze("hazard")`.
- **Expect:** returns a `Result` that is `isOk()` (fallback table), never an error.

### T5. canned fallback is deterministic
- Call `analyze("hazard A")` twice against the unreachable-host adapter.
- **Expect:** both calls return identical rows (same failure modes, same risk values).

### T6. empty hazard is rejected
- `analyze("")` or `analyze("   ")`.
- **Expect:** returns a `Result` that is `failed()` with a clear error message.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
# --- S1 Phase 3: RiskAI first slice (hazard -> LLM -> FMEA) ---------------
add_executable(lodestar_s1_phase3_tests
    test/s1_phase3_tests.cpp)
target_link_libraries(lodestar_s1_phase3_tests PRIVATE
    lodestar_common
    lodestar_adapters)
if(WIN32)
    target_link_libraries(lodestar_s1_phase3_tests PRIVATE ws2_32)
endif()
```

> Note: the engineer must register the `RiskAiService` sources in the `lodestar_riskai`
> module in `core/CMakeLists.txt` (replacing the `stub.cpp` placeholder) and link
> `lodestar_adapters` into it. T3 requires a live Ollama server and is marked `[SKIP]`
> when absent; T1/T2/T4/T5/T6 are fully deterministic and must always run.
