# Senior Engineer Task — S1 Phase 2 (Functional adapters)

You are **senior-engineer-phase2**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S1 Phase 2 per the test contract in **`docs/s1-phase2-test.md`** (read it fully).
Deliverable: real `invoke()` for the Skydel adapter and the LLM adapter (one end-to-end RF
injection or simulated + a real LLM call).

## What to do
1. Read `docs/s1-phase2-test.md` and `PLAN.md` (Phase 2 section).
2. Make `LlmAdapter::invoke("complete", ...)` perform a REAL HTTP POST to the configured
   model server (default Ollama `127.0.0.1:11434/api/generate`) via `HttpClient`; add
   `doHealth()`. On transport failure throw `AdapterError(Network|Timeout)` and set
   `status_.state = Error`.
3. Make `SkydelAdapter::invoke("start"|"stop"|"setConstellation", ...)` perform a REAL POST
   to the Skydel automation endpoint (default `127.0.0.1:8081`); add a `simulate` mode that
   returns a realistic vendor-shaped response when no real Skydel is reachable. On transport
   failure throw `AdapterError(Network|Timeout)`.
4. Add the adapter test `core/test/s1_phase2_tests.cpp` and register the
   `lodestar_s1_phase2_tests` target in `core/CMakeLists.txt` (inside
   `if(LODESTAR_BUILD_TESTS)`) exactly as the contract specifies (link `ws2_32` on Windows).
5. Build and run ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s1_phase2_tests`
   - `timeout 120 ./build/core/Release/lodestar_s1_phase2_tests.exe`
6. Make T1–T6 pass. T1/T2 are `[SKIP]` when no live Ollama server; T3–T6 must always pass.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase2'`.
