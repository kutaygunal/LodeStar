# WP-B Devops Task (devops-wpb)

You are `devops-wpb`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/working-rules.md. Commit and push WP-B (change management).

## Steps
1. `git status` to see changes.
2. Stage and commit the WP-B implementation + tests + PLAN.md status update as a `chore(wp-b): ...` commit.
3. If a git remote exists, push. If not, commit locally only (do NOT attempt a bogus push).
4. Report the commit hash.

Do NOT run `find /`. Do NOT modify code.

When done, run: `herdr agent prompt orchestrator 'DONE devops-wpb <commit-hash>'`
