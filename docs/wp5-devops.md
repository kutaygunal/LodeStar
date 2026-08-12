# WP-5 Devops Task (devops-wp5)

You are `devops-wp5`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/working-rules.md. Commit and push WP-5 (TestForge coverage wiring).

## IMPORTANT — shared files
Do NOT stage `core/CMakeLists.txt` or `core/smoke/tracelink_smoke.cpp` — devops-wp1
commits those shared files. Stage ONLY your own WP-5 files.

## Steps
1. `git status` to see changes.
2. Stage and commit ONLY these files (do NOT `git add -A`):
   - `core/persistence/migrations/017_test_run_coverage.sql`
   - `core/tracelink/CoverageService.h` `core/tracelink/CoverageService.cpp`
   - `core/test/wp5_coverage_tests.cpp`
   - `docs/wp5-task.md` `docs/wp5-test.md` `docs/wp5-devops.md`
   Do NOT modify PLAN.md (the orchestrator updates it).
   Commit as `chore(wp-5): TestForge coverage wiring — migration 017 + CoverageService + tests`.
3. If a git remote exists, push. If not, commit locally only.
4. Report the commit hash.

Do NOT run `find /`. Do NOT modify code.

When done, run: `herdr agent prompt orchestrator 'DONE devops-wp5 <commit-hash>'`
