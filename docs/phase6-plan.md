# Phase 6 — TestForge (IT&V plan generation, execution, reporting) — Detailed Plan

> **Owner:** orchestrator · **Status:** PLANNED · **Module:** `core/testforge`
> **Standard:** COMMERCIAL GRADE — production C++, proper error handling, full schema.
> **Verification:** build + smoke run (no test agents). `docs/architecture.md` §5.1.
> **Focus:** ONE module done exceptionally well, not breadth.

TestForge is the core IT&V deliverable: it auto-generates test procedures from
requirements/scenarios, executes them step-by-step evaluating pass/fail, and produces
reports. It is the automation layer that cuts test-procedure authoring effort ~90%.

The module links `lodestar_common` (Result/Logger/Uuid) and `lodestar_persistence`
(Database). It introduces a new migration `002_testforge.sql` with its own DAOs (the
DAOs are the only code that issues SQL, matching the persistence convention).

---

## Item 1 — Domain models (`core/testforge/Models.h`)

Plain data structures for the TestForge domain:

- `TestStep` — id, seq, name, description, `metric` (what is measured), expectedValue,
  tolerance, status.
- `TestProcedure` — id, name, version, objective, scenarioId, status, vector of steps.
- `StepResult` — stepId, seq, name, status (Passed/Failed/Blocked), actualValue,
  expectedValue, tolerance, measured flag, message.
- `TestRun` — id, procedureId, scenarioId, status (Pending/Running/Passed/Failed/Blocked),
  startedAt, finishedAt, vector of StepResults.
- `TestReport` — runId, procedureName, total/passed/failed/blocked counts, conclusion,
  step results.
- `StepStatus` / `RunStatus` enum classes with `toString` helpers.

### Acceptance
Models compile; enums round-trip via `toString`; used by Items 2–5.

---

## Item 2 — Plan generation (`PlanGenerator.h/.cpp`)

Auto-generate a `TestProcedure` from a scenario + a set of measurement checks.

- `generate(name, version, objective, scenarioId, std::vector<Check>)` where a `Check`
  is `{name, description, metric, expectedValue, tolerance}`.
- Produces a `TestProcedure` with one `TestStep` per check, a UUID id, and `status =
  Draft`.
- `checksFromObjective` convenience: turn a plain-text objective into a single
  `Check` (metric `"behavioral"`, tolerance 0) for quick smoke use.

### Acceptance
Generation produces the correct number of steps, sequential seq, expected values copied
into steps.

---

## Item 3 — Test execution (`TestRunner.h/.cpp`)

Execute a `TestProcedure` against a measurement source and evaluate each step.

- `IMeasurementProvider` interface: `common::Result<std::optional<double>>
  measure(const std::string& metric)`.
- `TestRunner` takes a `IMeasurementProvider&`. `run(procedure)` iterates steps:
  - measure the metric; if no value -> `Blocked` with message.
  - else compare `|actual - expected| <= tolerance` -> `Passed`; otherwise `Failed`.
- Builds a `TestRun` (Running at start, terminal status at end), fills `StepResult`s.
- `MockMeasurementProvider` — returns a value from a map (keyed by metric) so the smoke
  path runs without hardware.

### Acceptance
`run()` yields the right per-step statuses for pass/blocked/fail cases; run terminal
status is Passed iff every step passed.

---

## Item 4 — Reporting (`ReportGenerator.h/.cpp`)

Turn a `TestRun` into human-readable and structured reports.

- `toMarkdown(run)` — a README-style report: header, summary counts, per-step table,
  conclusion.
- `toJson(run)` — structured JSON (`lodestar::Json` from adapters is the JSON choice;
  testforge may reuse it) with run + step results.
- `summarize(run)` -> `TestReport` with counts + conclusion ("ALL PASSED" / "N FAILED").

### Acceptance
Markdown contains run id, counts, each step result; JSON parses back and fields match.

---

## Item 5 — Persistence (`TestForgeDao.h/.cpp` + migration `002_testforge.sql`)

Persist procedures and runs; append-only migration.

- `002_testforge.sql`:
  - `test_procedures` (id, name, version, objective, scenario_id, status)
  - `test_steps` (id, procedure_id, seq, name, description, metric,
    expected_value, tolerance)
  - `test_runs` (id, procedure_id, scenario_id, status, started_at, finished_at)
  - `step_results` (id, run_id, step_id, seq, name, status, actual_value,
    expected_value, tolerance, measured, message)
- `TestForgeDao`:
  - `saveProcedure(procedure)` (inserts procedure + steps in a transaction)
  - `loadProcedure(id)` -> full procedure with steps
  - `saveRun(run)` (inserts run + step results)
  - `loadRun(id)` -> full run with step results
- Wire the migration path (CMake `LODESTAR_TESTFORGE` migration dir not needed; the
  runner already scans the shared `persistence/migrations` dir).

### Acceptance
Procedure save/load round-trips with steps; run save/load round-trips with results.
Schema migrates from v1 -> v2.

---

## Item 6 — CMake + smoke integration

- Replace `testforge/stub.cpp` registration in `core/CMakeLists.txt` with the real
  `lodestar_testforge` target (link `lodestar_common`, `lodestar_persistence`).
- Keep `module_version()` linkable.
- Add `core/smoke/testforge_smoke.cpp` wired into `lodestar_smoke`:
  generate -> run (MockMeasurementProvider) -> report -> persist -> load-back, all
  assertions PASS.
- `core/smoke/main.cpp` calls the new smoke function (Phase 6).

### Acceptance
Build succeeds; `lodestar_smoke.exe` prints `TESTFORGE SMOKE OK`; phases 1–5 still pass.

---

## Build order
1. Models.h
2. Migration 002 + DAO
3. PlanGenerator
4. TestRunner + MockMeasurementProvider
5. ReportGenerator
6. CMake + smoke

## Phase-level acceptance
1. `cmake --build build` succeeds.
2. `lodestar_smoke.exe` exits 0; `TESTFORGE SMOKE OK` printed; prior phases still PASS.
3. Schema migrates v1 -> v2; procedure + run round-trip through SQLite.
4. No new heavy external dependencies.
5. `PLAN.md` Phase 6 row updated to `DONE` + `Committed` by devops when pushed.
