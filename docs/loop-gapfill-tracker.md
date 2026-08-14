# Gap-Fill Loop Tracker

Source plan: `docs/gap-fill-plan.md`. Working rules: prove it, don't just build it;
every work item gets a passing CTest target; honest status (no fabricated values);
keep the mature core stable.

Legend: `[ ]` todo · `[x]` done · `[~]` in progress · `[-]` skipped/out-of-scope

## Wave 1 — Foundations + highest exposure
- [x] W1.0 Baseline: MSVC build green, 52/54 tests pass (2 pre-existing out-of-scope failures)
- [ ] CC#4 Shared LlmClient abstraction (local LLM + deterministic fallback)
- [ ] CC#3 Shared report service (PDF/Word/CSV/XLSX/ReQIF)
- [x] RiskAI 1.1 FMEA workflow engine (AIAG/VDA shape) + `r1_fmea_workflow_tests`
- [x] RiskAI 1.2 RPN + Action Priority + `r2` boundary tests

## Wave 2 — RiskAI depth
- [ ] RiskAI 1.3 FMEA assessment of existing documents + `r2_fmea_assess_tests`
- [ ] RiskAI 1.4 AIAG/VDA-compatible export
- [ ] RiskAI 1.5 Multi-document knowledge input
- [ ] RiskAI 1.6 Agentic / self-validating pipeline + `r3_agentic_pipeline_tests`
- [ ] RiskAI 1.7 Inline requirement-quality scoring in TraceLink

## Wave 3 — Cert evidence
- [ ] AssureCheck 2.2 Vetted standards-content library
- [ ] AssureCheck 2.1 SAS / PSAC-style generation + `a1_sas_tests`
- [ ] AssureCheck 2.3 Unified live coverage view + `a2_live_coverage_view_tests`
- [ ] IntegrateHub 6.1 PR → CR → impact analysis + `i1_impact_tests`

## Wave 4 — Collaboration
- [ ] TraceLink 3.1 Client–server persistence path
- [ ] TraceLink 3.2 Real-time multi-user collaboration + `t1_collab_tests`
- [ ] TraceLink 3.3 Variant / module reuse management
- [ ] TraceLink 3.4 Electronic signatures
- [ ] TraceLink 3.5 OSLC integration ecosystem
- [ ] CC#1 Multi-user web/review layer
- [ ] CC#2 Security (KDF, sessions, RBAC)

## Wave 5 — Coverage + ScenarioForge
- [ ] TestForge 4.1 Decision / MC-DC coverage + `f1_mcdc_tests`
- [ ] TestForge 4.2 Tool-qualification evidence pack
- [ ] ScenarioForge 5.1 HIL stream + `s1_hil_stream_tests`
- [ ] ScenarioForge 5.2 RTK virtual reference station
- [ ] ScenarioForge 5.3 A-GNSS assistance-data generation
- [ ] ScenarioForge 5.4 Advanced interference & multipath depth + `s2_interference_tests`
- [ ] ScenarioForge 5.5 First-party baseband / multi-GNSS synthesis
- [ ] IntegrateHub 6.2 Integration with certification control
