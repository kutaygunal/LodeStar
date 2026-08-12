# Product Manager Challenge — Lodestar Platform (overall)

You are a **senior product manager** for the Lodestar GNSS/SBAS Integrated Test & Verification
platform. Your job is to **challenge the whole platform** and produce a deep, honest,
commercial-grade gap analysis vs. the leading commercial IT&V / ALM / GNSS platforms. **Do NOT
change any source code.** Produce a detailed HTML report only.

## The platform (current state)
Lodestar is a C++17 CMake monorepo (MSVC/Windows) with a Qt Widgets desktop UI, SQLite
persistence, Jenkins CI, and local LLMs. Modules (all implemented):
- **ScenarioForge** — software-defined GNSS/SBAS scenario generation (real C++ math)
- **TestForge** — IT&V plan generation, execution, reporting
- **TraceLink** — requirements/design/interface/test traceability, impact, coverage, compliance
  rules, baselines, change workflow, ReqIF/CSV/HTML import-export, REST API, Qt UI
- **AssureCheck** — compliance engine for ARP4754A/ARP4761/DO-178C/DO-254/DO-278A
- **Adapters** — Spirent/R&S/Skydel/Python/LLM integration
- **API** — thin C++ REST API + API-key auth
- **UI** — Qt desktop dashboard (matrix, graph, impact, coverage, compliance views)
- **RiskAI** (stub), **IntegrateHub** (stub), **Python layer** (stub), **CI** (skeleton)

## What to analyze (deep, product-manager lens)
Compare the whole Lodestar platform against commercial IT&V / ALM / GNSS platforms (e.g.
**Spirent PNT-Automation, R&S, Skydel, Jama, Polarion, Codebeamer, DOORS, VectorCAST, LDRA,
Rapita**). For each area, state what Lodestar has, what commercial tools have, and the **gap**:

1. **End-to-end IT&V workflow** — Does it cover the full lifecycle (scenario → test → trace →
   compliance → report)? Where are the seams?
2. **Integration & interoperability** — Vendor RF tools, ReqIF, OSLC, CI/CD, data exchange?
3. **Collaboration & multi-user** — Roles, permissions, concurrent editing, review workflows?
4. **Automation & AI** — LLM-assisted analysis, auto-reporting, intelligent gap detection?
5. **Deployment** — Desktop vs. web, on-prem/cloud, licensing, scalability?
6. **Certification readiness** — DO-178C/ARP4754A evidence, audit trail, compliance reports?
7. **UX & product polish** — Dashboard, navigation, visualization, onboarding?
8. **Performance & reliability** — Real-time, determinism, scale, robustness?
9. **Commercial viability** — What would a customer/IT&V lead expect that is missing? Pricing,
   support, documentation, packaging?

## Deliverable
Write a **detailed HTML report** to `docs/reports/platform-pm-report.html`. Structure:
- Executive summary (verdict + top gaps)
- Capability vs. commercial tools comparison table
- Deep-dive per area (1–9) with "Have / Commercial / Gap"
- Prioritized gap list (P0/P1/P2) with effort estimate
- Recommended roadmap

Use clean, self-contained HTML (inline CSS, no external deps). Make it professional and
readable. When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE pm-platform'`.
