# Senior Engineer Task — S2 Phase 12 (OSLC integration)

You are **senior-engineer-phase12**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 12 per the test contract in **`docs/s2-phase12-test.md`** (read it fully).
Deliverable: OSLC integration — expose/consume OSLC (Open Services for Lifecycle
Collaboration) for interop with DOORS/Polarion/Codebeamer ecosystems.

## Background (read these first)
- `PLAN.md` Phase 12 section.
- `core/api/` — existing REST API servers (TraceLink, AssureCheck).
- `core/tracelink/` — requirements/traceability model to expose via OSLC.

## What to do
1. Read `docs/s2-phase12-test.md` and `PLAN.md` (Phase 12).
2. Add an **OSLC provider**: expose requirements (and optionally test cases) as OSLC
   resources (RDF/XML or JSON-LD) with the standard OSLC requirement resource shapes
   (dcterms:title, dcterms:identifier, oslc_rm:Requirement).
3. Add an **OSLC consumer**: fetch/import an OSLC requirement resource from a remote
   provider into the local TraceLink model.
4. Add the test file `core/test/s2_phase12_tests.cpp` and register the
   `lodestar_s2_phase12_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
5. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase12_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase12_tests.exe`
6. Make all T1–T4 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase12'`.
