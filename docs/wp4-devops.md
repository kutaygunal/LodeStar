# WP-4 Devops Task (devops-wp4)

You are `devops-wp4`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/working-rules.md. Commit and push WP-4 (roles/permissions + concurrency).

## IMPORTANT — shared files
Do NOT stage `core/CMakeLists.txt` or `core/smoke/tracelink_smoke.cpp` — devops-wp1
commits those shared files. Stage ONLY your own WP-4 files.

## Steps
1. `git status` to see changes.
2. Stage and commit ONLY these files (do NOT `git add -A`):
   - `core/persistence/migrations/016_rbac.sql`
   - `core/tracelink/RbacService.h` `core/tracelink/RbacService.cpp`
   - `core/tracelink/TraceLinkService.h` `core/tracelink/TraceLinkService.cpp`
     (these contain the optimistic-locking `updateEntityIfVersion` for WP-4)
   - `core/test/wp4_rbac_tests.cpp`
   - `docs/wp4-task.md` `docs/wp4-test.md` `docs/wp4-devops.md`
   Do NOT modify PLAN.md (the orchestrator updates it).
   Commit as `chore(wp-4): RBAC + optimistic locking — migration 016 + RbacService + tests`.
3. If a git remote exists, push. If not, commit locally only.
4. Report the commit hash.

Do NOT run `find /`. Do NOT modify code.

When done, run: `herdr agent prompt orchestrator 'DONE devops-wp4 <commit-hash>'`
