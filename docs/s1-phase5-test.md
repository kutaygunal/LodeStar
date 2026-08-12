# S1 Phase 5 Test Contract — Real-time / determinism validation (recorded benchmarks)

> Written by the scrum-master BEFORE the Phase 5 engineer implements the feature.
> The engineer must implement the contract below so the suite measures a core operation,
> verifies determinism, and records benchmark numbers. Do NOT weaken the assertions to
> make them pass; implement the feature to satisfy them. This is a TEST CONTRACT, not a
> testing task.
>
> **Scope:** Sprint 1 Phase 5 (PLAN.md). Deliverable = recorded benchmark numbers.
> Phase 5 **depends on Phase 2** (the Skydel adapter's real `invoke()`), which is DONE and
> committed. This phase validates real-time / determinism of the core.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)

```bash
# 1. Configure with tests enabled.
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON

# 2. Build the Phase 5 tests (HARD TIMEOUT).
timeout 600 cmake --build build --config Release --target lodestar_s1_phase5_tests

# 3. Run the Phase 5 tests (HARD TIMEOUT).
timeout 120 ./build/core/Release/lodestar_s1_phase5_tests.exe
```

## Test file
- **File:** `core/test/s1_phase5_tests.cpp`
- **CMake target:** `lodestar_s1_phase5_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s1_phase5_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Contract the Phase 5 engineer must provide

### (A) Benchmark harness
A small harness in the test file that:
- Times a core operation (e.g. a `TraceGraph` query: insert N requirements + links, then
  `linksTo`/`requirements()` repeatedly) using a monotonic clock.
- Runs the operation M times, records min/avg/max microseconds, and prints them.
- **Records the numbers to `docs/reports/s1-phase5-benchmarks.md`** (append a dated
  section with the operation name, N, M, and min/avg/max). The file must be created if it
  does not exist.

### (B) Determinism check
- The same input (same DB seed, same query) must produce the same output every run.
- The test asserts byte-identical results across repeated runs of a core operation.

## Test cases & expected behavior

### T1. Determinism: same input → same output
- Fresh DB, run migrations, insert a fixed set of requirements + test cases + `verifies`
  links.
- Run `TraceGraph::requirements()` and `linksTo("requirement", id)` twice.
- **Expect:** both runs return identical results (same ids, same order, same fields).

### T2. Determinism: repeated query is stable
- Run the same query 5 times on the same DB.
- **Expect:** all 5 results are identical (no ordering or content drift).

### T3. Benchmark: graph query timing is recorded
- Insert N=100 requirements and N=100 `verifies` links.
- Time `linksTo` / `requirements()` over M=100 iterations.
- **Expect:** the harness prints min/avg/max microseconds and appends a section to
  `docs/reports/s1-phase5-benchmarks.md` containing the operation name and the three
  numbers. The file exists after the run.

### T4. Benchmark: insert throughput is recorded
- Time inserting N=100 requirements + links.
- **Expect:** the harness prints and records insert timing to the same report file.

### T5. Report file is well-formed
- After T3/T4, read `docs/reports/s1-phase5-benchmarks.md`.
- **Expect:** the file contains a `##` section for the current run with the operation
  names and numeric timing values present.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
# --- S1 Phase 5: real-time / determinism validation (benchmarks) ----------
add_executable(lodestar_s1_phase5_tests
    test/s1_phase5_tests.cpp)
target_link_libraries(lodestar_s1_phase5_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink)
target_compile_definitions(lodestar_s1_phase5_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the benchmark report path is relative to the working directory where the test is
> run (the repo root). The engineer must ensure the `docs/reports/` directory exists
> (create it if needed) and that the test writes the report file there. T1/T2 are pure
> determinism checks; T3/T4/T5 exercise the recording harness.
