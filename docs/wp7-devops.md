# WP-7 Devops Task (devops-wp7)

You are `devops-wp7`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/working-rules.md. Commit and push WP-7 (coverage dashboard + charts).

## IMPORTANT — shared files
The engineers modified shared files that belong to the whole Batch 3 (WP-7..10):
`core/CMakeLists.txt`, `core/smoke/tracelink_smoke.cpp`, `core/tracelink/UiWiringService.h/.cpp`,
`ui/CMakeLists.txt`, `ui/MainWindow.h/.cpp`. You are responsible for committing these shared
files along with your own WP-7 files. The other devops agents (wp8..wp10) will commit ONLY
their own WP files and will NOT touch these shared files.

## Steps
1. `git status` to see changes.
2. Stage and commit ONLY these files (do NOT `git add -A`):
   - `core/tracelink/UiWiringService.h` `core/tracelink/UiWiringService.cpp` (shared)
   - `core/test/wp7_dashboard_tests.cpp`
   - `ui/CoverageDashboardView.h` `ui/CoverageDashboardView.cpp`
   - `ui/MainWindow.h` `ui/MainWindow.cpp` (shared)
   - `ui/CMakeLists.txt` (shared)
   - `core/CMakeLists.txt` (shared — all WP-7..10 target registrations)
   - `core/smoke/tracelink_smoke.cpp` (shared — schema version bump to 18)
   - `docs/wp7-task.md` `docs/wp7-test.md` `docs/wp7-devops.md`
   Do NOT modify PLAN.md (the orchestrator updates it).
   Commit as `chore(wp-7): coverage dashboard + charts — liveCoverage + coverageCharts + tests`.
3. If a git remote exists, push. If not, commit locally only.
4. Report the commit hash.

Do NOT run `find /`. Do NOT modify code.

When done, run: `herdr agent prompt orchestrator 'DONE devops-wp7 <commit-hash>'`
