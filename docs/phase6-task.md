# Phase 6 — TestForge (IT&V plan generation, execution, reporting) — Implementation Task

> **Status:** IN PROGRESS · **Standard:** COMMERCIAL GRADE.
> **Owner of the code:** orchestrator (driving directly — reliable path).
> **Owner of the commit:** devops-6.

## Scope

Build `core/testforge` — the TestForge module — to production quality. Full detail in
`docs/phase6-plan.md`. Follow it. Deliver these files under `core/testforge/`:

- `Models.h` — `TestStep`, `TestProcedure`, `StepResult`, `TestRun`, `TestReport`,
  `StepStatus`/`RunStatus` enums + `toString` helpers.
- `PlanGenerator.h/.cpp` — `generate(...)` from scenario + checks; `checksFromObjective`.
- `TestRunner.h/.cpp` — `IMeasurementProvider` + `TestRunner::run(procedure)` +
  `MockMeasurementProvider`.
- `ReportGenerator.h/.cpp` — `toMarkdown`, `toJson`, `summarize`.
- `TestForgeDao.h/.cpp` — persistence (save/load procedure + run).
- `core/persistence/migrations/002_testforge.sql` — new tables (append-only; never edit
  `001_initial.sql`).
- `core/smoke/testforge_smoke.cpp` — self-verifying smoke; wired into
  `core/smoke/main.cpp`.
- Update `core/CMakeLists.txt`: replace the `testforge/stub.cpp` line with the real
  `lodestar_testforge` target (link `lodestar_common`, `lodestar_persistence`); keep
  `module_version()` linkable.

## Conventions (match the existing code)

- Namespace `lodestar::testforge`. `Result<T>` from `core/common/Result.h`.
- `newUuid()` from `core/common/Uuid.h` for ids; `Logger` from `core/common/Logger.h`.
- DAOs issue SQL through `persistence::Database` (see `core/persistence/daos.cpp` for the
  `bindText`/`columnText` helper pattern). Use transactions for multi-row saves.
- JSON: reuse `lodestar::Json` (`core/adapters/Json.h`) in `ReportGenerator::toJson`.
- Use `/W4`-clean MSVC code (no warnings).

## Self-verification (do BEFORE reporting done)

1. `cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/Users/kutay/vcpkg/scripts/buildsystems/vcpkg.cmake`
   then `cmake --build build --config Debug` — must succeed.
2. `./build/core/Debug/lodestar_smoke.exe` — must exit 0 and print:
   - `SMOKE OK: schema v2, ...`
   - `SCENARIO SMOKE OK`
   - `ADAPTERS+API SMOKE OK`
   - `TESTFORGE SMOKE OK`
   - Prior phase lines must still be present (regression check).

The smoke must assert: PlanGenerator produces the right step count + expected values;
TestRunner returns Passed/Blocked/Failed correctly (use MockMeasurementProvider with a
metric that passes and one that is missing -> Blocked); ReportGenerator markdown/JSON
fields match; persistence save/load round-trips both a procedure (with steps) and a run
(with results).

## Devops-6 task (after code verified)

Commit and push to `main` remote:

1. New files: `core/testforge/Models.h`, `PlanGenerator.h/.cpp`, `TestRunner.h/.cpp`,
   `ReportGenerator.h/.cpp`, `TestForgeDao.h/.cpp`,
   `core/persistence/migrations/002_testforge.sql`, `core/smoke/testforge_smoke.cpp`,
   `docs/phase6-plan.md`.
2. Modified files: `core/CMakeLists.txt`, `core/smoke/main.cpp`, `PLAN.md` (Phase 6 row
   -> `DONE` / `Committed` = `yes`).
3. Conventional message, e.g. `feat(core): add TestForge IT&V plan/run/report (Phase 6)`.
4. Push to `origin main`; reply `DONE devops-6` to the orchestrator.
