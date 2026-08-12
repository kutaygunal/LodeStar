# Devops Task — Phase 4 (ScenarioForge: real GNSS math)

You are **devops-4** for the Lodestar GNSS/SBAS platform. You commit and push the completed
Phase 4 work.

## Context

- Repo: `C:/Users/kutay/Desktop/Projects/Lodestar`
- Phase 4 (ScenarioForge real GNSS math) has been implemented and self-verified (build
  succeeded; `lodestar_smoke.exe` ran and printed `SCENARIO SMOKE OK` and `SMOKE OK`, exit 0).
- You are the ONLY agent that commits and pushes (per `docs/working-rules.md`).

## Your job

1. Review what changed: `git status --short` and `git diff --stat`.
2. Stage and commit the Phase 4 work with a conventional commit message, e.g.
   `feat(core): add ScenarioForge real GNSS math (Phase 4)`.
   - Include: the new `core/scenario` sources, the `core/CMakeLists.txt` changes, the new
     smoke path (`core/smoke/scenario_smoke.cpp`, `core/smoke/main.cpp`), the tracker updates
     (`PLAN.md`, `docs/loop-state.md`), and the Phase 4 planning/scrum/engineer task docs.
   - Do NOT commit the `build/` directory (it should be gitignored; verify).
3. Push to `origin` (branch `main`).
4. Verify the push succeeded (`git log --oneline -1` and `git status`).

## Constraints

- Do NOT modify source files. Commit only.
- Do NOT run `find /`.

## Report

When done, run: `herdr agent prompt orchestrator 'DONE devops-4'`.
