# Orchestration Manual

> Working rules are single-sourced in `_shared/working-rules.md`. Follow them. See the
> Context guard section below and the `reborn` skill for handoff.

This project is driven by a multi-agent loop. Roles:

| Agent | Duty |
|-------|------|
| `planner` | Produces/updates the phased plan in a `PLAN*.md` tracker. |
| `scrum-master` | Reviews the plan, assigns the current phase to an engineer, writes the phase tests. Does NOT implement. |
| `senior-engineer-<phase>` | Implements the current phase. Re-spawned per phase. |
| `testing-<phase>` | Runs the phase tests. FAIL -> engineer; PASS -> devops. |
| `devops-<phase>` | Commits and pushes. |
| `orchestrator` | Coordinates the loop, closes non-essential agents between phases. |

Workflow loop per phase:
1. Planner produces/updates the plan.
2. Scrum-master reviews, assigns the phase, writes the phase tests.
3. Engineer implements and reports "done".
4. Testing runs the phase tests. FAIL -> engineer; PASS -> devops.
5. Devops commits and pushes, reports "phase finished".
6. Orchestrator closes subagents except scrum-master and planner.
7. Orchestrator informs scrum-master and planner the phase is done.
8. Scrum-master starts the next loop -> assigns the next phase to a fresh engineer.
9. Loop until all phases in the tracker are complete.

## Subagent workspace & delegation

- Run engineer/tester/devops subagents in a **dedicated `agents` workspace** (create it if it
does not exist) and move each subagent's pane into it after spawn.
- **Delegate with a bounded wait** (`--wait --timeout 3000`, 3s): do NOT wait for subagent
completion; tell each subagent who the orchestrator is and to notify the orchestrator when
done (`herdr agent prompt orchestrator 'DONE <name>'`).
- **Close the subagent's pane** when the orchestrator receives the `DONE` callback.

## Completion guard

Always **finalize state BEFORE closing any panes**: call `mark_complete "<PROJECT_PATH>"
"<summary>"` (writes docs/loop-state.md + logs), THEN close subagent panes. Call
`update_loop_state` after each phase. This prevents a mid-finalize crash from losing the
result and orphaning subagents. Run `watchdog_loop "<PROJECT_PATH>" orchestrator` before
each phase and after completion to detect a dead orchestrator or orphaned subagents.

## Context guard

The orchestrator tracks its context usage in `docs/context-log.json`. At **~30M tokens**
warn the user; at **~45M tokens** STOP starting new phases and suggest the user run the
**`reborn` skill** so a fresh-context successor can take over with full project state.
