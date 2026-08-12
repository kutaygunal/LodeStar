# WP-9 Devops Task (devops-wp9)

You are `devops-wp9`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/working-rules.md. Commit and push WP-9 (baseline visual diff + rollback).

## IMPORTANT — shared files
Do NOT stage `core/CMakeLists.txt`, `core/smoke/tracelink_smoke.cpp`,
`core/tracelink/UiWiringService.h/.cpp`, `ui/CMakeLists.txt`, or `ui/MainWindow.h/.cpp` —
devops-wp7 commits those shared files. Stage ONLY your own WP-9 files.

## Steps
1. `git status` to see changes.
2. Stage and commit ONLY these files (do NOT `git add -A`):
   - `core/tracelink/BaselineService.h` `core/tracelink/BaselineService.cpp`
     (these contain the visualDiff/rollbackEntity wiring for WP-9)
   - `core/test/wp9_diff_tests.cpp`
   - `ui/BaselineDiffView.h` `ui/BaselineDiffView.cpp`
   - `docs/wp9-task.md` `docs/wp9-test.md` `docs/wp9-devops.md`
   Do NOT modify PLAN.md (the orchestrator updates it).
   Commit as `chore(wp-9): baseline visual diff + rollback — visualDiff + rollbackEntity + tests`.
3. If a git remote exists, push. If not, commit locally only.
4. Report the commit hash.

Do NOT run `find /`. Do NOT modify code.

When done, run: `herdr agent prompt orchestrator 'DONE devops-wp9 <commit-hash>'`
