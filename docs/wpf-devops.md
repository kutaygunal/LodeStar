# WP-F Devops Task (devops-wpf)

You are `devops-wpf`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/working-rules.md. Commit and push WP-F (robustness hardening).

## Steps
1. `git status` to see changes.
2. Stage and commit the WP-F implementation + tests + PLAN.md status update as a `chore(wp-f): ...` commit.
3. If a git remote exists, push. If not, commit locally only (do NOT attempt a bogus push).
4. Report the commit hash.

Do NOT run `find /`. Do NOT modify code.

When done, run: `herdr agent prompt orchestrator 'DONE devops-wpf <commit-hash>'`
