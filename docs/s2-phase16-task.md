# Senior Engineer Task — S2 Phase 16 (Variants / branching)

You are **senior-engineer-phase16**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 16 per the test contract in **`docs/s2-phase16-test.md`** (read it fully).
Deliverable: variants / branching — product-line engineering (Codebeamer/Polarion-style).

## Background (read these first)
- `PLAN.md` Phase 16 section.
- `core/tracelink/` — requirements/traceability model to which variants apply.
- `core/persistence/migrations/` — add a migration for variant tables.

## What to do
1. Read `docs/s2-phase16-test.md` and `PLAN.md` (Phase 16).
2. Add a **variant model**: a product variant (e.g. "Base", "Pro", "Avionics") with a
   set of included/excluded requirements (or a feature model).
3. Add **branching**: create a branch of a requirement set, apply variant-specific
   changes, and merge back (with conflict detection).
4. Add a new migration (e.g. `025_variants.sql`) for variant + branch tables.
5. Add the test file `core/test/s2_phase16_tests.cpp` and register the
   `lodestar_s2_phase16_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
6. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase16_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase16_tests.exe`
7. Make all T1–T4 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase16'`.
