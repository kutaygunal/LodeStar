# WP-1 Devops Task (devops-wp1)

You are `devops-wp1`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/working-rules.md. Commit and push WP-1 (suspect-link workflow).

## IMPORTANT — shared files
The engineers modified shared files that belong to the whole Batch 1 (WP-1..5):
`core/CMakeLists.txt`, `core/smoke/tracelink_smoke.cpp`. You are responsible for
committing these shared files along with your own WP-1 files. The other devops agents
(wp2..wp5) will commit ONLY their own WP files and will NOT touch these shared files.

## Steps
1. `git status` to see changes.
2. Stage and commit ONLY these files (do NOT `git add -A`):
   - `core/persistence/migrations/013_suspect_flags.sql`
   - `core/tracelink/SuspectService.h` `core/tracelink/SuspectService.cpp`
   - `core/test/wp1_suspect_tests.cpp`
   - `core/CMakeLists.txt` (shared — includes all WP-1..5 target registrations)
   - `core/smoke/tracelink_smoke.cpp` (shared — schema version bump to 17)
   - `docs/wp1-task.md` `docs/wp1-test.md` `docs/wp1-devops.md`
   Do NOT modify PLAN.md (the orchestrator updates it).
   Commit as `chore(wp-1): suspect-link workflow — migration 013 + SuspectService + tests`.
3. If a git remote exists, push. If not, commit locally only.
4. Report the commit hash.

Do NOT run `find /`. Do NOT modify code.

When done, run: `herdr agent prompt orchestrator 'DONE devops-wp1 <commit-hash>'`
