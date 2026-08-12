# Second PM Gap Analysis — Post-Sprint-1 Re-assessment

You are the **pm-gap-recheck** subagent. Your job is to re-run the Product Manager gap
analysis AFTER Sprint 1 ("Make it run") has completed, to determine **what was fixed** and
produce a **new gap analysis** reflecting the current state.

## Context

Sprint 1 was the first of a 4-sprint plan (see `docs/reports/sprint-plan.html`). Its scope
was the P0 "make it run" layer:

- S1.1 Build the desktop Qt app (enable `LODESTAR_BUILD_UI`, wire MainWindow to service API)
- S1.2 Make adapters functional (real `invoke()` for Skydel + LLM)
- S1.3 Implement RiskAI first slice (LLM-assisted FMEA/hazard)
- S1.4 Implement IntegrateHub first slice (cross-disciplinary hub)
- S1.5 Validate real-time / determinism (benchmarks + HIL smoke)

## Your task

1. **Read the four original PM reports** in `docs/reports/`:
   - `platform-pm-report.html`
   - `scenarioforge-pm-report.html`
   - `testforge-pm-report.html`
   - `assurecheck-pm-report.html`
   These describe the gaps BEFORE Sprint 1.

2. **Inspect the current source tree** to verify what Sprint 1 actually changed. Check:
   - `ui/` — is the Qt app built? Is `LODESTAR_BUILD_UI` now ON? Does `MainWindow` wire to the service API?
   - `core/adapters/` — do `SkydelAdapter` and `LlmAdapter` now have real `invoke()` logic (not connect-only stubs)?
   - `core/riskai/` — is it still a stub, or is there a real LLM-assisted FMEA implementation?
   - `core/integratehub/` — is it still a stub, or is there a real issue/coordination model?
   - Real-time / determinism — are there new benchmark tests or HIL smoke tests?
   - Check `git log --oneline -20` to see the Sprint 1 commits.
   - Check `PLAN.md` and `docs/loop-state.md` for the recorded Sprint 1 status.

3. **Produce a NEW gap analysis** as an HTML report at
   `docs/reports/sprint1-gap-recheck.html`. It must:
   - State clearly which Sprint 1 items were **FIXED** (with evidence from the source tree / git log).
   - State which Sprint 1 items were **NOT fixed / partial** (if any).
   - Re-assess the remaining gaps across all four modules (Platform, ScenarioForge, TestForge, AssureCheck) in light of what changed.
   - Give an updated prioritized gap list (P0/P1/P2) for the NEXT sprint (Sprint 2).
   - Match the visual style of the existing PM reports (clean HTML, cards, tables, priority pills).

4. **Do NOT modify any source code.** This is analysis only.

## Deliverable

Write `docs/reports/sprint1-gap-recheck.html`. When done, notify the orchestrator:
`herdr agent prompt orchestrator 'DONE pm-gap-recheck'`.
