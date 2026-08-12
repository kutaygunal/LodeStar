# WP-3 Devops Task (devops-wp3)

You are `devops-wp3`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/working-rules.md. Commit and push WP-3 (compliance templates/checklists).

## IMPORTANT — shared files
Do NOT stage `core/CMakeLists.txt` or `core/smoke/tracelink_smoke.cpp` — devops-wp1
commits those shared files. Stage ONLY your own WP-3 files.

## Steps
1. `git status` to see changes.
2. Stage and commit ONLY these files (do NOT `git add -A`):
   - `core/persistence/migrations/015_compliance_templates.sql`
   - `core/tracelink/ComplianceService.h` `core/tracelink/ComplianceService.cpp`
   - `core/test/wp3_compliance_tests.cpp`
   - `docs/wp3-task.md` `docs/wp3-test.md` `docs/wp3-devops.md`
   Do NOT modify PLAN.md (the orchestrator updates it).
   Commit as `chore(wp-3): compliance templates/checklists — migration 015 + ComplianceService + tests`.
3. If a git remote exists, push. If not, commit locally only.
4. Report the commit hash.

Do NOT run `find /`. Do NOT modify code.

When done, run: `herdr agent prompt orchestrator 'DONE devops-wp3 <commit-hash>'`
