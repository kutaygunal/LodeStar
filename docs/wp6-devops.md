# WP-6 Devops Task (devops-wp6)

You are `devops-wp6`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/working-rules.md. Commit and push WP-6 (Qt UI shell).

## IMPORTANT — shared files
Do NOT stage `core/smoke/tracelink_smoke.cpp` (not modified here). Stage ONLY the WP-6
files listed below. Do NOT use `git add -A` (it would sweep unrelated untracked docs).

## Steps
1. `git status` to see changes.
2. Stage and commit ONLY these files:
   - `core/tracelink/UiWiringService.h` `core/tracelink/UiWiringService.cpp`
   - `core/test/wp6_ui_tests.cpp`
   - `ui/ProjectTreeView.h` `ui/ProjectTreeView.cpp`
   - `ui/DetailPanelView.h` `ui/DetailPanelView.cpp`
   - `ui/MainWindow.h` `ui/MainWindow.cpp`
   - `ui/CMakeLists.txt`
   - `core/CMakeLists.txt` (adds lodestar_wp6_ui_tests target)
   - `docs/wp6-task.md` `docs/wp6-test.md` `docs/wp6-devops.md`
   Do NOT modify PLAN.md (the orchestrator updates it).
   Commit as `chore(wp-6): Qt UI shell — project tree + detail panel + UiWiringService + tests`.
3. If a git remote exists, push. If not, commit locally only.
4. Report the commit hash.

Do NOT run `find /`. Do NOT modify code.

When done, run: `herdr agent prompt orchestrator 'DONE devops-wp6 <commit-hash>'`
