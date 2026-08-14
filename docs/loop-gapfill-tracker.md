# Gap-Fill Loop Tracker

Source plan: `docs/gap-fill-plan.md`. Working rules: prove it, don't just build it;
every work item gets a passing CTest target; honest status (no fabricated values);
keep the mature core stable.

Legend: `[ ]` todo · `[x]` done · `[~]` in progress · `[-]` skipped/out-of-scope

## Wave 1 — Foundations + highest exposure
- [x] W1.0 Baseline: MSVC build green, 52/54 tests pass (2 pre-existing out-of-scope failures)
- [~] CC#4 Shared LlmClient abstraction (local LLM + deterministic fallback) — LlmAdapter + deterministic paths already shared; authoring/scoring reuse common::QualityScoring (no-LLM path)
- [~] CC#3 Shared report service (PDF/Word/CSV/XLSX/ReQIF) — CertReportService + RiskReportService reuse; consolidated under one reporting pattern
- [x] RiskAI 1.1 FMEA workflow engine (AIAG/VDA shape) + `r1_fmea_workflow_tests`
- [x] RiskAI 1.2 RPN + Action Priority + `r2` boundary tests

## Wave 2 — RiskAI depth
- [x] RiskAI 1.3 FMEA assessment of existing documents + `r2_fmea_assess_tests`
- [x] RiskAI 1.4 AIAG/VDA-compatible export
- [x] RiskAI 1.5 Multi-document knowledge input
- [x] RiskAI 1.6 Agentic / self-validating pipeline + `r3_agentic_pipeline_tests`
- [x] RiskAI 1.7 Inline requirement-quality scoring in TraceLink

**Module 1 (RiskAI) DONE** — 6 CTest targets green (r1,r2,r3,r4,r5,r7).

## Wave 3 — Cert evidence
- [x] AssureCheck 2.2 Vetted standards-content library
- [x] AssureCheck 2.1 SAS / PSAC-style generation + `a1_sas_tests`
- [x] AssureCheck 2.3 Unified live coverage view + `a2_live_coverage_view_tests`
- [x] IntegrateHub 6.1 PR → CR → impact analysis + `i1_impact_tests`

**Wave 3 DONE** — 3 AssureCheck + 1 IntegrateHub CTest targets green.

## Wave 4 — Collaboration
- [x] TraceLink 3.1 Client–server persistence path
- [x] TraceLink 3.2 Real-time multi-user collaboration + `t1_collab_tests`
- [x] TraceLink 3.3 Variant / module reuse management
- [x] TraceLink 3.4 Electronic signatures
- [x] TraceLink 3.5 OSLC integration ecosystem
- [ ] CC#1 Multi-user web/review layer
- [x] CC#2 Security (KDF, sessions, RBAC)

## Wave 5 — Coverage + ScenarioForge
- [x] TestForge 4.1 Decision / MC-DC coverage + `f1_mcdc_tests`
- [x] TestForge 4.2 Tool-qualification evidence pack
- [ ] TestForge 4.2 Tool-qualification evidence pack
- [x] ScenarioForge 5.1 HIL stream + `s1_hil_stream_tests`
- [x] ScenarioForge 5.2 RTK virtual reference station
- [x] ScenarioForge 5.3 A-GNSS assistance-data generation
- [x] ScenarioForge 5.4 Advanced interference & multipath depth + `s2_interference_tests`
- [x] ScenarioForge 5.5 First-party baseband / multi-GNSS synthesis
- [ ] IntegrateHub 6.2 Integration with certification control
