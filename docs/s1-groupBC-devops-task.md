# Devops Task — S1 Group B/C commit (Phases 3, 5)

You are **devops-groupbc**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Commit and push the completed S1 Group B/C work (Phases 3 and 5). Both phases' tests PASS
(verified by the orchestrator). Do NOT modify code — commit only.

## What to commit
- **Phase 3 (RiskAI):** `core/riskai/` (real `RiskAiService` replacing stub),
  `core/test/s1_phase3_tests.cpp`, `core/CMakeLists.txt` (new `lodestar_s1_phase3_tests`
  target).
- **Phase 5 (RT/determinism):** `core/test/s1_phase5_tests.cpp`, `core/CMakeLists.txt`
  (new `lodestar_s1_phase5_tests` target), `docs/reports/s1-phase5-benchmarks.md`.
- Also commit the test-spec/task docs: `docs/s1-phase3-test.md`, `docs/s1-phase5-test.md`,
  `docs/s1-phase3-task.md`, `docs/s1-phase5-task.md`, `docs/sprint1-scrum-ph35-task.md`.

## Steps
1. `git status` and `git diff --stat` to see what changed.
2. Stage the relevant files and commit as **separate `chore(...)` commits** per phase
   (e.g. `chore(s1): phase 3 riskai`, `chore(s1): phase 5 rt/determinism benchmarks`).
3. Push if a remote exists (`git push`). If no remote, commit locally only.
4. Report the commit hashes.

## Rules
- Only you (devops) commit/push. Never modify source code.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE devops-groupbc'`.
