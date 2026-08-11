# Orchestration Manual

> Working rules are single-sourced in `_shared/working-rules.md`. Follow them. See the
> Context guard section below and the `reborn` skill for handoff.

This project is driven by a multi-agent loop. **This loop has NO testing agents** — the
user explicitly requested no testing phase. Instead, the engineer/architect self-verifies
(compiles/builds) and reports, then devops commits.

Roles:

| Agent | Duty |
|-------|------|
| `planner` | Produces/updates the phased plan in a `PLAN*.md` tracker. |
| `scrum-master` | Reviews the plan, assigns the current phase to an engineer/architect. Does NOT implement. |
| `senior-architect-<phase>` | Implements architecture/design phases (e.g. step 1 architecture doc). Re-spawned per phase. |
| `senior-engineer-<phase>` | Implements build/code phases (e.g. step 2 scaffold, step 3 vertical slice). Re-spawned per phase. |
| `devops-<phase>` | Commits and pushes. |
| `orchestrator` | Coordinates the loop, closes non-essential agents between phases. |

Workflow loop per phase (NO testing step):
1. Planner produces/updates the plan.
2. Scrum-master reviews, assigns the phase to an architect or engineer.
3. Architect/engineer implements and self-verifies (builds/compiles), reports "done".
4. Devops commits and pushes, reports "phase finished".
5. Orchestrator closes subagents except scrum-master and planner.
6. Orchestrator informs scrum-master and planner the phase is done.
7. Scrum-master starts the next loop -> assigns the next phase to a fresh architect/engineer.
8. Loop until all phases in the tracker are complete.

## The 3-phase loop (steps 1-3)

| Phase | Step | Agent role |
|-------|------|-----------|
| 1 | Write `docs/architecture.md` (full architecture) | `senior-architect-1` |
| 2 | Scaffold monorepo skeleton with compiling CMake | `senior-engineer-2` |
| 3 | Build Phase 1 vertical slice (common, persistence, tracelink) | `senior-engineer-3` |

## Context guard

The orchestrator tracks its context usage in `docs/context-log.json`. At **~30M tokens**
warn the user; at **~45M tokens** STOP starting new phases and suggest the user run the
**`reborn` skill** so a fresh-context successor can take over with full project state.
