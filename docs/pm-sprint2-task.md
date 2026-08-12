# pm-sprint2 — Fresh Product Manager Re-assessment (two-stage)

You are **pm-sprint2**, a fresh Product Manager subagent. You will assess the Lodestar product
in TWO stages. Follow the stages in order. Do NOT skip ahead.

## Stage 1 — Independent gap analysis (do this FIRST)

You have NO prior knowledge of this product's history. Do your OWN independent assessment of
the current state of the Lodestar source tree. Do not rely on any prior report.

1. Inspect the source tree at `/c/Users/kutay/Desktop/Projects/Lodestar`:
   - `core/` (tracelink, assurecheck, scenario, testforge, riskai, integratehub, adapters, api, persistence)
   - `ui/` (Qt desktop app)
   - `CMakeLists.txt`, `PLAN.md`, `docs/loop-state.md`
   - `git log --oneline -30`
2. Determine, from the code alone, what is **implemented and working**, what is **partial**, and
   what is **missing** — across the whole platform (requirements/traceability, compliance,
   GNSS scenario generation, test plan/execution, adapters, API, UI, CI, packaging).
3. Produce a **draft** gap-analysis HTML report at `docs/reports/sprint2-pm-draft.html` with your
   independent findings and a prioritized gap list (P0/P1/P2).

## Stage 2 — Compare against the previous report (do this AFTER Stage 1)

Now that you have your own independent assessment, read the previous PM recheck report at
`docs/reports/sprint1-gap-recheck.html`. Compare your independent findings against it.

1. Identify which gaps from the previous report are now **FIXED** (with evidence from the source).
2. Identify which gaps are **still missing or partial**.
3. Produce the **FINAL latest report** at `docs/reports/sprint2-gap-recheck.html` that:
   - States what was fixed since the previous report (with evidence).
   - Lists what is **STILL missing** (if any), with an updated P0/P1/P2 priority list for the next sprint.
   - Matches the visual style of the existing PM reports (clean HTML, cards, tables, priority pills).

## Deliverable

Write `docs/reports/sprint2-pm-draft.html` (Stage 1) and `docs/reports/sprint2-gap-recheck.html`
(Stage 2). Do NOT modify any source code — analysis only.

When both stages are complete, notify the orchestrator:
`herdr agent prompt orchestrator 'DONE pm-sprint2'`.
