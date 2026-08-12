# WP-8 Devops Task (devops-wp8)

You are `devops-wp8`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/working-rules.md. Commit and push WP-8 (interactive traceability matrix).

## IMPORTANT — shared files
Do NOT stage `core/CMakeLists.txt`, `core/smoke/tracelink_smoke.cpp`,
`core/tracelink/UiWiringService.h/.cpp`, `ui/CMakeLists.txt`, or `ui/MainWindow.h/.cpp` —
devops-wp7 commits those shared files. Stage ONLY your own WP-8 files.

## Steps
1. `git status` to see changes.
2. Stage and commit ONLY these files (do NOT `git add -A`):
   - `core/persistence/migrations/018_matrix_views.sql`
   - `core/test/wp8_matrix_tests.cpp`
   - `ui/MatrixView.h` `ui/MatrixView.cpp`
   - `docs/wp8-task.md` `docs/wp8-test.md` `docs/wp8-devops.md`
   Do NOT modify PLAN.md (the orchestrator updates it).
   Commit as `chore(wp-8): interactive traceability matrix — matrixFiltered + saved views + tests`.
3. If a git remote exists, push. If not, commit locally only.
4. Report the commit hash.

Do NOT run `find /`. Do NOT modify code.

When done, run: `herdr agent prompt orchestrator 'DONE devops-wp8 <commit-hash>'`
