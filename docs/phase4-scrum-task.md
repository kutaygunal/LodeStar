# Scrum-Master Task — Phase 4 (ScenarioForge: real GNSS math)

You are the **scrum-master** for the Lodestar GNSS/SBAS platform. You review the plan and
itemize it into concrete, assignable tasks. You do NOT implement code.

## Context

- Repo: `C:/Users/kutay/Desktop/Projects/Lodestar`
- Read `docs/phase4-plan.md` (the planner's detailed plan) and `PLAN.md`.
- The plan has 7 items (6 required + a Scenario facade integration item) with 20 child
  items, plus a dependency graph and recommended build order.

## Your deliverable

1. **Review** the plan for completeness, feasibility, and ordering. Flag any gaps or
   ambiguities (e.g. missing error handling, unclear acceptance criteria, missing CMake
   wiring). If you find gaps, note them but do NOT block — the engineer will resolve them.

2. **Itemize** the plan into a concrete, assignable task list. For each task specify:
   - **Task ID** (e.g. `P4-1.1`, `P4-1.2`, ...).
   - **Title** and **scope** (which files/classes).
   - **Dependencies** (which tasks must be done first).
   - **Acceptance criteria** (how the engineer self-verifies by build + smoke run).
   - **Estimated effort** (S/M/L).

3. **Assign** the phase to a **senior-engineer-4** agent. Since the items are
   interdependent (see the dependency graph), assign the whole phase as ONE engineering
   task to a single `senior-engineer-4` agent that implements items in dependency order.
   Do NOT split into multiple parallel engineers (the items share too much state).

4. Write the itemized task list to `docs/phase4-scrum.md` and update `PLAN.md` Phase 4
   row: set `Assigned to` to `senior-engineer-4` and Status to `IN PROGRESS`.

Do NOT implement any code. Do NOT commit. Reply `DONE scrum-master` when the task list is
written and PLAN.md is updated.
