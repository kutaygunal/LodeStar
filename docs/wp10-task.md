# WP-10 Task — Document-style authoring (senior-engineer-wp10)

You are `senior-engineer-wp10`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/reborn-brief.md first. Implement WP-10 to commercial grade.

## Scope (PLAN.md WP-10)
Document-style authoring: author requirements in a document context with atomic traceability.

## Contract
The test contract is in `docs/wp10-test.md` (written by scrum-master). Follow it exactly:
- Extend `core/tracelink/UiWiringService.h` (+ .cpp) with `DocumentSection`,
  `DocumentModel`, `document()`, `addRequirementToDocument()`, and
  `reorderRequirements()` (Qt-independent wiring).
- Create `core/test/wp10_doc_tests.cpp` implementing the contract's test cases T1..T5.
- The CMake target `lodestar_wp10_doc_tests` is already registered in
  `core/CMakeLists.txt`. Do NOT weaken assertions.
- ALSO create `ui/DocumentView` to render the document (sections + requirements) from
  `document()` and call `addRequirementToDocument()` / `reorderRequirements()`.
  Enable LODESTAR_BUILD_UI=ON.

## Working rules (docs/working-rules.md)
- Build with HARD TIMEOUT: `timeout 600 cmake --build build --config Release`.
- For the UI build, configure with:
  `cmake -S . -B build -DCMAKE_PREFIX_PATH=/c/Qt/6.8.2/msvc2022_64 -DLODESTAR_BUILD_UI=ON -DLODESTAR_BUILD_TESTS=ON`
  then `timeout 600 cmake --build build --config Release`.
- Run tests ONE AT A TIME with timeouts.
- Do NOT commit/push — that is devops's job.
- Do NOT run `find /`.

## Definition of done
1. `./build/core/Release/lodestar_wp10_doc_tests.exe` passes (all assertions green).
2. The UI shell builds with Qt 6.8.2 (LODESTAR_BUILD_UI=ON).
3. No regressions in existing wp1..wp8, wpA..wpG, and Phase-10 suites.
4. Smoke passes: `./build/core/Release/lodestar_smoke.exe`.
5. Report a concise summary of what you implemented and the test results.

When done, run: `herdr agent prompt orchestrator 'DONE senior-engineer-wp10'`
