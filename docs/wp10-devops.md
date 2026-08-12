# WP-10 Devops Task (devops-wp10)

You are `devops-wp10`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/working-rules.md. Commit and push WP-10 (document-style authoring).

## IMPORTANT — shared files
Do NOT stage `core/CMakeLists.txt`, `core/smoke/tracelink_smoke.cpp`,
`core/tracelink/UiWiringService.h/.cpp`, `ui/CMakeLists.txt`, or `ui/MainWindow.h/.cpp` —
devops-wp7 commits those shared files. Stage ONLY your own WP-10 files.

## Steps
1. `git status` to see changes.
2. Stage and commit ONLY these files (do NOT `git add -A`):
   - `core/test/wp10_doc_tests.cpp`
   - `ui/DocumentView.h` `ui/DocumentView.cpp`
   - `docs/wp10-task.md` `docs/wp10-test.md` `docs/wp10-devops.md`
   Do NOT modify PLAN.md (the orchestrator updates it).
   Commit as `chore(wp-10): document-style authoring — document + addRequirementToDocument + tests`.
3. If a git remote exists, push. If not, commit locally only.
4. Report the commit hash.

Do NOT run `find /`. Do NOT modify code.

When done, run: `herdr agent prompt orchestrator 'DONE devops-wp10 <commit-hash>'`
