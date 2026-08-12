# Product Manager Challenge — AssureCheck

You are a **senior product manager** for the Lodestar GNSS/SBAS platform. Your job is to
**challenge the AssureCheck module** and produce a deep, honest, commercial-grade gap analysis
vs. leading compliance/ALM tools. **Do NOT change any source code.** Produce a detailed HTML
report only.

## The module (current state)
AssureCheck is a compliance-checking engine covering **ARP4754A, ARP4761, DO-178C, DO-254,
DO-278A**. It has:
- Standards registry + 136 checklist objectives (seeded from `docs/assurecheck-standards-checklist.md`)
- DAL (Development Assurance Level) A–E support
- Compliance engine: PASS/FAIL/NA/WARNING per objective, evidence links
- Evidence collection from TraceLink (requirements/design/test/trace) + TestForge (test runs)
- Certification-ready reports (HTML/CSV/JSON), objective coverage %
- Performance: indexed/batched/incremental, 10k-scale
- REST API (`/assurecheck/*`) + compliance dashboard (Qt)

## What to analyze (deep, product-manager lens)
Compare AssureCheck against commercial compliance/ALM tools (e.g. **LDRA, Rapita RapiTest,
VectorCAST, QA Systems, Visure, Codebeamer, Polarion, Jama, DOORS**). For each area, state
what AssureCheck has, what commercial tools have, and the **gap**:

1. **Compliance coverage** — Are the 136 objectives complete vs. the real standards? Are
   DO-178C Tables A-1..A-7, DO-254, ARP4754A, ARP4761 fully represented? Missing objectives?
2. **Certification evidence** — Does it produce auditor-ready evidence packages? Traceability
   of evidence to objectives? Sign-off/approval workflow?
3. **Verification methods** — Does it support analysis/test/inspection/demonstration? MC/DC,
   statement, decision coverage? Coupling analysis?
4. **DAL handling** — Correct DAL applicability per objective? DAL assignment workflow?
5. **Workflow & review** — Objective status lifecycle, review/approval, waivers/deviations?
6. **Reporting** — Certification reports, per-standard/per-DAL, export formats, audit trail?
7. **Integration** — With TraceLink/TestForge? Import of external evidence? ReqIF?
8. **UX** — Compliance dashboard, objective browser, gap highlighting?
9. **Commercial readiness** — What would a certification authority or a paying customer
   expect that is missing?

## Deliverable
Write a **detailed HTML report** to `docs/reports/assurecheck-pm-report.html`. Structure:
- Executive summary (verdict + top gaps)
- Capability vs. commercial tools comparison table
- Deep-dive per area (1–9) with "Have / Commercial / Gap"
- Prioritized gap list (P0/P1/P2) with effort estimate
- Recommended roadmap

Use clean, self-contained HTML (inline CSS, no external deps). Make it professional and
readable. When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE pm-assurecheck'`.
