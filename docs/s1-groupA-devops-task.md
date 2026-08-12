# Devops Task — S1 Group A commit (Phases 1, 2, 4)

You are **devops-groupA**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Commit and push the completed S1 Group A work (Phases 1, 2, 4). All three phases' tests
PASS (verified by the orchestrator). Do NOT modify code — commit only.

## What to commit
- **Phase 1 (Desktop app):** `ui/CMakeLists.txt` (runnable `lodestar_app`), `ui/app/main.cpp`,
  `core/test/s1_phase1_tests.cpp`, `core/CMakeLists.txt` (new `lodestar_s1_phase1_tests`
  target).
- **Phase 2 (Functional adapters):** `core/adapters/` changes (real Skydel/LLM invoke),
  `core/test/s1_phase2_tests.cpp`, `core/CMakeLists.txt` (new `lodestar_s1_phase2_tests`
  target).
- **Phase 4 (IntegrateHub):** `core/integratehub/` (real `IntegrateHubService` replacing
  stub), `core/persistence/migrations/022_integratehub.sql`,
  `core/test/s1_phase4_tests.cpp`, `core/CMakeLists.txt` (new `lodestar_s1_phase4_tests`
  target).
- Also commit the planning/test-spec docs: `PLAN.md`, `docs/s1-phase*-test.md`,
  `docs/s1-phase*-task.md`, `docs/sprint1-*.md`.

## Steps
1. `git status` and `git diff --stat` to see what changed.
2. Stage the relevant files and commit as **separate `chore(...)` commits** per phase
   (e.g. `chore(s1): phase 1 desktop app`, `chore(s1): phase 2 functional adapters`,
   `chore(s1): phase 4 integratehub`).
3. Push if a remote exists (`git push`). If no remote, commit locally only.
4. Report the commit hashes.

## Rules
- Only you (devops) commit/push. Never modify source code.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE devops-groupA'`.
