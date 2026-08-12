# WP-2 Devops Task (devops-wp2)

You are `devops-wp2`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/working-rules.md. Commit and push WP-2 (review/comment/approval).

## IMPORTANT — shared files
Do NOT stage `core/CMakeLists.txt` or `core/smoke/tracelink_smoke.cpp` — devops-wp1
commits those shared files. Stage ONLY your own WP-2 files.

## Steps
1. `git status` to see changes.
2. Stage and commit ONLY these files (do NOT `git add -A`):
   - `core/persistence/migrations/014_tracelink_reviews.sql`
   - `core/tracelink/ReviewService.h` `core/tracelink/ReviewService.cpp`
   - `core/test/wp2_review_tests.cpp`
   - `docs/wp2-task.md` `docs/wp2-test.md` `docs/wp2-devops.md`
   Do NOT modify PLAN.md (the orchestrator updates it).
   Commit as `chore(wp-2): review/comment/approval — migration 014 + ReviewService + tests`.
3. If a git remote exists, push. If not, commit locally only.
4. Report the commit hash.

Do NOT run `find /`. Do NOT modify code.

When done, run: `herdr agent prompt orchestrator 'DONE devops-wp2 <commit-hash>'`
