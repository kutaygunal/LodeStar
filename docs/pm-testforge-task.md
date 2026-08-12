# Product Manager Challenge — TestForge

You are a **senior product manager** for the Lodestar GNSS/SBAS platform. Your job is to
**challenge the TestForge module** and produce a deep, honest, commercial-grade gap analysis
vs. leading test-management / verification tools. **Do NOT change any source code.** Produce
a detailed HTML report only.

## The module (current state)
TestForge is the IT&V (Integration, Test & Verification) plan/execution/reporting module. It
has:
- **PlanGenerator** — auto-builds a TestProcedure (IT&V plan) from a scenario + measurement checks
- **TestRunner** — executes test runs, measures metrics, compares to expected values/tolerances
- **ReportGenerator** — produces test reports
- **TestForgeDao** — persistence for procedures, steps, runs, step results
- Schema: test_procedures, test_steps, test_runs, step_results (migration 002)
- Integrates with ScenarioForge (scenarios) and TraceLink (coverage)

## What to analyze (deep, product-manager lens)
Compare TestForge against commercial test-management / verification tools (e.g. **VectorCAST,
Rapita RapiTest, LDRA, Jama, Polarion, Codebeamer, TestRail, NI TestStand, Keysight**). For
each area, state what TestForge has, what commercial tools have, and the **gap**:

1. **Test authoring** — Plan generation quality? Test case design (equivalence, boundary,
   MC/DC)? Reusable test libraries? Parameterized tests?
2. **Test execution** — Real-time execution? Hardware-in-the-loop? Parallel execution?
   Deterministic timing? Data acquisition?
3. **Measurement & analysis** — Metric capture, pass/fail thresholds, tolerances, statistical
   analysis, anomaly detection?
4. **Coverage** — Requirement coverage, code coverage, MC/DC, statement/decision? Integration
   with TraceLink coverage?
5. **Reporting** — Certification-ready reports, DO-178C evidence, traceability of results to
   requirements, export formats?
6. **Workflow** — Test case lifecycle, review/approval, baselines, change impact on tests?
7. **Integration** — With ScenarioForge (GNSS scenarios), TraceLink, AssureCheck, external
   RF tools (Spirent/R&S/Skydel), CI/CD?
8. **UX** — Test plan editor, run dashboard, results browser, gap highlighting?
9. **Commercial readiness** — What would a verification lead or auditor expect that is missing?

## Deliverable
Write a **detailed HTML report** to `docs/reports/testforge-pm-report.html`. Structure:
- Executive summary (verdict + top gaps)
- Capability vs. commercial tools comparison table
- Deep-dive per area (1–9) with "Have / Commercial / Gap"
- Prioritized gap list (P0/P1/P2) with effort estimate
- Recommended roadmap

Use clean, self-contained HTML (inline CSS, no external deps). Make it professional and
readable. When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE pm-testforge'`.
