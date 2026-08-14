# Verification Task — Second Eye on the Gap-Fill Plan

You are `gapfill-verifier`, an independent second eye. Your job is to **independently
verify** that the Lodestar gap-fill plan was implemented correctly and honestly. Do NOT
trust the orchestrator's claims — check the actual code, build, and tests yourself.

## Project
`C:/Users/kutay/Desktop/Projects/Lodestar`

## Source of truth
- Plan: `docs/gap-fill-plan.md`
- Tracker (orchestrator's claims): `docs/loop-gapfill-tracker.md`

## Build environment (Windows, MSVC)
The project builds with MSVC via a helper batch file. Use this exact pattern:

```bash
cd C:/Users/kutay/Desktop/Projects/Lodestar
cat > _msvc_build.bat <<'EOF'
@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set VCPKG_ROOT=C:\Users\kutay\vcpkg
%*
EOF
cmd //c "_msvc_build.bat cmake --build build-msvc"
```

The build dir `build-msvc` already exists and is configured. To run the full test suite:

```bash
cmd //c "_msvc_build.bat ctest --test-dir build-msvc -C Release"
```

## What to verify (be thorough and honest)

### 1. Build integrity
- Run the full MSVC build. It must complete with no errors.
- Run the full CTest suite. Record the pass/fail count.

### 2. Every plan work item has a real, passing CTest target
For each module in the plan, confirm the listed test target exists in
`core/CMakeLists.txt`, is registered with CTest, and PASSES. The claimed new targets are:

- **RiskAI (Module 1):** `lodestar_r1_fmea_workflow_tests`, `lodestar_r2_fmea_assess_tests`,
  `lodestar_r3_agentic_pipeline_tests`, `lodestar_r4_fmea_export_tests`,
  `lodestar_r5_knowledge_tests`, `lodestar_r7_quality_scoring_tests`
- **AssureCheck (Module 2):** `lodestar_a1_sas_tests`, `lodestar_a2_live_coverage_view_tests`,
  `lodestar_a4_cert_change_control_tests`
- **IntegrateHub (Module 6):** `lodestar_i1_impact_tests`, `lodestar_i2_cert_control_tests`
- **TraceLink (Module 3):** `lodestar_t1_server_persistence_tests`, `lodestar_t1_collab_tests`,
  `lodestar_t3_variant_override_tests`, `lodestar_t4_signature_tests`,
  `lodestar_t5_oslc_server_tests`
- **TestForge (Module 4):** `lodestar_f1_mcdc_tests`, `lodestar_f2_tool_qual_dossier_tests`
- **ScenarioForge (Module 5):** `lodestar_s1_hil_stream_tests`, `lodestar_s2_interference_tests`,
  `lodestar_s3_rtk_agnss_tests`, `lodestar_s4_multignss_tests`
- **Cross-cutting:** `lodestar_cc1_web_review_tests`, `lodestar_cc2_security_tests`,
  `lodestar_cc3_shared_report_tests`, `lodestar_cc4_llm_client_tests`

For each: run it directly (e.g. `./build-msvc/core/lodestar_r1_fmea_workflow_tests.exe`)
and confirm it reports `0 failure(s)`.

### 3. Honesty check (the plan's "prove it, don't just build it" rule)
- **No fabricated "measured" values.** Check the coverage view (`CoverageComplianceView`) and
  the MC-DC import (`LlvmCovImport`) — they must report `no-evidence` / honest counts when
  tooling is absent, not invent numbers.
- **Migrations are real.** Confirm the new migration files exist in
  `core/persistence/migrations/` (028–034) and are referenced by the tests.
- **No test is weakened to pass.** Spot-check a couple of test files to confirm the
  assertions are meaningful (not `check(true, ...)`).

### 4. Report
Write your findings to `docs/verification-report.md` in the project. Structure it as:
- Build result (pass/fail, error count)
- CTest summary (X passed / Y failed, list any failures)
- Per-module table: work item → target → status (PASS/FAIL/MISSING)
- Honesty findings (any fabricated values, weakened tests, or missing migrations)
- Overall verdict: COMPLETE / PARTIAL / INCOMPLETE, with reasons

Be honest and specific. If something is wrong, say exactly what and where.

## When done
Notify the orchestrator by running:
```bash
herdr agent prompt orchestrator "DONE gapfill-verifier"
```
Do not wait for me. Work independently.
