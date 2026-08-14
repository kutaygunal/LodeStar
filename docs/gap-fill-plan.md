# Lodestar — Gap-Filling Plan (per module)

**Source of gaps:** `docs/gap-analysis-competitors.md` (competitive analysis vs Spirent,
Skydel/Safran, Rohde & Schwarz, IBM DOORS, Siemens Polarion, Jama, VectorCAST, LDRA, RISQ,
Datapetal, QDES, FMEA Excellence, fmea-tool.ai).

**Scope:** Product functionality only. DevOps/CI/testing-infrastructure items are excluded (tracked
in `PLAN.md`).

**Cross-cutting principles applied to every module:**
- Each work item is independently shippable and testable (CTest target where feasible).
- Prefer small, honest slices over broad claims — follow the project's "prove it, don't just
  build it" rule.
- Keep the mature core (TraceLink, ScenarioForge math, AssureCheck engine) stable; add capability
  behind new services/adapters rather than churning proven code.
- Privacy/local-LLM and deterministic-fallback remain explicit differentiators — never regress them.

---

## Module 1 — RiskAI (highest exposure)

**Gap:** Basic severity×likelihood FMEA; competitors (RISQ, QDES, FMEA Excellence, fmea-tool.ai)
offer AIAG/VDA-guided workflows, RPN/Action-Priority, assessment of existing FMEAs, multi-document
input, structured export, and agentic self-validating pipelines. Also, requirement-quality scoring
is disconnected from TraceLink authoring.

### 1.1 FMEA workflow engine (AIAG/VDA shape)
1. Define a structured FMEA data model in `core/riskai`:
   - Structure (next-higher / focus / next-lower), Function, Requirement.
   - Failure chain: Effect (FE) → Failure Mode (FM) → Cause (FC).
   - Ratings: Severity (S), Occurrence (O), Detection (D); Action Priority (AP).
2. Implement a **workflow state machine** mirroring AIAG-VDA steps 1–7
   (Planning → Structure → Function → Failure → Risk → Optimization → Documentation), each a
   distinct, persistable stage with required fields and validation.
3. Persist models via a new SQLite table set + DAOs (follow `core/persistence` conventions).
4. **Test:** `core/test/r1_fmea_workflow_tests.cpp` — CRUD, stage gating, required-field rules.

### 1.2 RPN and Action Priority computation
1. Add deterministic rating tables (S/O/D 1–10, AIAG-VDA Action Priority matrix) as data files,
   not hardcoded strings — mirrors how `assurecheck-standards-checklist.md` stores standards.
2. Compute RPN (S×O×D) and AP (H/M/L) with a pure function; unit-test every matrix boundary.
3. Expose a scoring API reused by both the generation and assessment paths.

### 1.3 FMEA assessment of existing documents
1. Add an import path for uploaded FMEA content (CSV/XLSX/PDF text), parse into the model.
2. Run a checklist-based **quality assessor** (completeness, clarity, severity justification,
   detection adequacy, risk-scoring consistency) using the local LLM + deterministic rule fallback.
3. Emit a per-item improvement recommendation list.
4. **Test:** `core/test/r2_fmea_assess_tests.cpp` with fixture FMEA inputs and known-good scores.

### 1.4 AIAG/VDA-compatible export
1. Extend `CertReportService`-style reporting (or a new `RiskReportService`) to emit:
   - AIAG-VDA structured FMEA spreadsheet (Excel/CSV) with all 7-step columns.
   - HTML/PDF review report.
2. Support round-trip: export → edit in Excel → re-import (FMEA Excellence pattern).
3. **Test:** export produces parseable, schema-valid files; re-import preserves row identity.

### 1.5 Multi-document knowledge input
1. Add an ingestion service accepting requirements, flow charts, standards, historical data,
   drawings text, lessons-learned.
2. Build a lightweight retrieval step that feeds context to the LLM prompt for generation.
3. Keep the **deterministic fallback** so generation works with no LLM.

### 1.6 Agentic / self-validating pipeline
1. Refactor single-pass generation into ANALYZE → RATE → VALIDATE → CORRECT → FINALIZE stages,
   each with a quality gate (consistency of failure chain, rating-range validity, AP-consistency).
2. On gate failure, loop a bounded CORRECT pass; log corrections as audit events.
3. **Test:** `core/test/r3_agentic_pipeline_tests.cpp` — forced-invalid inputs converge to valid.

### 1.7 Inline requirement-quality scoring in TraceLink
1. Move the five-dimension scoring (clarity, testability, atomicity, completeness, ambiguity) to a
   shared `QualityScoring` service used by **both** RiskAI and TraceLink.
2. Add a TraceLink authoring surface: score a requirement on save, show per-dimension flags and
   suggested rewording (local LLM, deterministic fallback to heuristic flags).
3. **Test:** scoring determinism, TraceLink write hook.

**Priority:** High. **Dependencies:** 1.1 → 1.2 → 1.3/1.4/1.5; 1.6 depends on 1.1–1.5; 1.7
touches TraceLink (small, isolated service).

---

## Module 2 — AssureCheck (certification evidence)

**Gap:** No certification-authority-facing artifact generation (SAS/PSAC-style evidence packages)
and no vetted pre-built DO-178C content library; live coverage not unified as a compliance view.

### 2.1 Software Accomplishment Summary (SAS) / PSAC-style generation
1. Add `SasService` producing the standard aviation artifacts from live compliance data:
   - Plan for Software Aspects of Certification (PSAC).
   - Software Accomplishment Summary (SAS) with objectives→evidence mapping.
2. Reuse the existing templated PDF/Word/ReQIF pipeline (`CertReportService`) as the renderer.
3. **Test:** `core/test/a1_sas_tests.cpp` — artifact renders with correct objectives table and
   evidence links.

### 2.2 Vetted standards-content library
1. Curate the existing DO-178C/DO-254/ARP4754A/ARP4761/DO-278A checklists into a structured,
   versioned content bundle (objectives, sub-objectives, DAL applicability, evidence requirements).
2. Load from data files (versioned) rather than code, so content can be updated without a rebuild.
3. **Test:** content loads, validates, and maps to DAL A–E.

### 2.3 Unified live coverage view
1. Pull measured coverage (statement today; decision/MC-DC when tooling lands) from TestForge into
   a single compliance dashboard that maps coverage → DO-178C objectives (A-3..A-7).
2. Show per-objective status: compliant / partial / no-evidence, with links to the measured lines.
3. **Test:** `core/test/a2_live_coverage_view_tests.cpp` — objective aggregation correctness.

### 2.4 Certification change/impact control
1. Wire IntegrateHub/TraceLink change requests into a DO-178C configuration-control view:
   - Every PR/CR shows affected HLRs, LLRs, and tests, and its approval state per baseline.
2. Emit the change-impact report as a certification artifact (reuse 2.1 renderer).
3. **Test:** PR→impact→artifact end-to-end.

**Priority:** High. **Dependencies:** 2.2 content library underpins 2.1 and 2.3; 2.4 depends on
IntegrateHub (Module 6).

---

## Module 3 — TraceLink (collaboration & scale)

**Gap:** Desktop single-user; no multi-user real-time collaboration, variant management,
e-signatures, or OSLC integration ecosystem; AI requirement-quality not inline (moved to 1.7).

### 3.1 Client–server persistence path
1. Move TraceLink data operations behind a persistence interface already present in the codebase
   (`core/persistence`); add a server-backed adapter for the web deployment mode.
2. Keep SQLite as the single-user default; the server mode is an alternative storage backend.
3. **Test:** run the existing TraceLink test suites against both backends (parameterized).

### 3.2 Real-time multi-user collaboration
1. Add a change-notification layer (operation log with version vectors per baseline/item).
2. Implement optimistic concurrency with conflict detection on conflicting edits.
3. Web UI (JS) subscribes to changes and renders live updates (aligns with PLAN S3.6).
4. **Test:** `core/test/t1_collab_tests.cpp` — concurrent edits, conflict resolution, vector merge.

### 3.3 Variant / module reuse management
1. Model **variants** (product-line) and **reusable modules** with inheritance/override of
   requirement attributes.
2. Implement baseline → variant mapping and branching.
3. **Test:** variant inheritance and branch divergence.

### 3.4 Electronic signatures
1. Add an approval-signature model (signer, role, timestamp, hash of approved content) on top of
   the existing review/approval workflow.
2. Persist signatures immutably and surface them in certification exports (Module 2).
3. **Test:** signature validity on content change.

### 3.5 OSLC integration ecosystem
1. Complete the OSLC server slice (discovery, resource-shape catalog, query) — align with PLAN S3.10.
2. Add OSLC consumer capability: import/export requirements and links from/to external ALM tools.
3. **Test:** OSLC round-trip against a reference client.

**Priority:** Medium–High. **Dependencies:** 3.1 first (backend), then 3.2; 3.4/3.5 parallel.

---

## Module 4 — TestForge (qualified coverage)

**Gap:** Coverage tooling (OpenCppCoverage/Cobertura) not DO-330 qualified; no decision/MC-DC.

### 4.1 Decision and MC/DC coverage
1. Add a coverage engine adapter that accepts **clang-format/clang-cl + llvm-cov** output for
   branch/decision/MC-DC measurement (install a real toolchain, matching PLAN's S3.3 boundary note).
2. Extend `coverage_results` to populate `decisions_total/conditions_total` honestly.
3. **Test:** `core/test/f1_mcdc_tests.cpp` — known branch structures produce expected counts.

### 4.2 Tool-qualification evidence pack
1. Produce a **DO-330 tool-qualification dossier** for the coverage path: purpose, operational
   environment, verification results, deviation/limitation log.
2. Document the exact toolchain versions and the verification run used (evidence, not claims).
3. This is a documentation + reproducibility artifact; validate by re-running the qualification
   test set and capturing the transcript.

**Priority:** Medium (compliance-value). **Dependencies:** 4.1 → 4.2. 4.1 depends on toolchain
availability (matches the project's known scope boundary).

---

## Module 5 — ScenarioForge (RF/HIL/RTK depth)

**Gap:** No first-party RF output, real-time HIL streaming, RTK virtual reference station,
A-GNSS assistance data, or advanced interference/multipath depth comparable to Spirent/Skydel/R&S.

### 5.1 Real-time hardware-in-the-loop (HIL) stream
1. Add a first-party HIL position/attitude feed over UDP and SCPI (100 Hz), with a latency-control
   loop: latency measurement, calibration, command jitter buffering, trajectory prediction.
2. Model after the R&S SMW-K109 behavior (documented in vendor specs).
3. Keep vendor adapters as an alternative RF path; this adds a software-first feed.
4. **Test:** `core/test/s1_hil_stream_tests.cpp` — feed rate, latency statistics, interpolation.

### 5.2 RTK virtual reference station
1. Add an RTK base-station model that emits **RTCM 3.3 / NTRIP** corrections.
2. Wire to ScenarioForge position config; stream corrections over LAN.
3. **Test:** RTCM message generation against reference message sets.

### 5.3 A-GNSS assistance-data generation
1. Add generation of almanac / navigation / acquisition assistance files for TTFF testing.
2. **Test:** assistance files parse and match simulated constellation.

### 5.4 Advanced interference & multipath depth
1. Extend RF-impairment modeling: matched-spectrum interferer, CW/AWGN/jamming/spoofing scenarios.
2. Add environment models (urban canyon, roadside planes, obscuration, ground/sea reflection,
   antenna pattern & body-mask files) as data-driven models.
3. **Test:** `core/test/s2_interference_tests.cpp` and environment-model tests.

### 5.5 First-party baseband / multi-GNSS synthesis
1. Aligns with PLAN S3.9: full multi-GNSS baseband (GLONASS/BeiDou, multi-satellite RF synthesis).
2. Document that **real RF emission** remains adapter-driven (vendor hardware); the in-house slice
   is a software baseband/SDR-ready signal model, clearly scoped — do not overclaim RF output.
3. **Test:** baseband synthesis produces valid I/Q frames per constellation.

**Priority:** Medium (ScenarioForge is already a mature core; these extend, not fix). **Order:**
5.1 → 5.2 → 5.3 (self-contained); 5.4 and 5.5 parallel.

---

## Module 6 — IntegrateHub (issue/impact)

**Gap:** Lightweight issue log; competitors embed problem-report + change-request + impact-analysis
linked to requirements/baselines.

### 6.1 Problem Report → Change Request → impact analysis
1. Formalize a PR workflow (fields, states, approval authority) and CR model linked to it.
2. Add **impact analysis**: on a CR, compute affected requirements, design items, tests, and
   baselines; show risk of unverified impact.
3. Link PR/CR rows into the TraceLink traceability graph (reuse its link types).
4. **Test:** `core/test/i1_impact_tests.cpp` — impact set correctness, approval gating.

### 6.2 Integration with certification control
1. Feed PR/CR and impact into the AssureCheck configuration-control view (Module 2.4).
2. Emit the PR/CR log as a certification artifact.

**Priority:** Medium. **Depends on** 2.4 and TraceLink link model.

---

## Module 7 — Cross-cutting (applies to all)

1. **Multi-user web/review layer** — interactive, editable JS frontend + server backend (PLAN
   S3.6/S3.7). Every module above exposes a REST route so the web layer can drive it.
2. **Security** — iterated password KDF, session/token expiry, RBAC enforcement at the service
   boundary (PLAN S3.8). Validate each new service checks authorization.
3. **Standard-format exports** — one shared reporting service (PDF/Word/CSV/XLSX/ReQIF) reused by
   RiskAI (1.4), AssureCheck (2.1/2.4), and IntegrateHub (6.2), rather than per-module exporters.
4. **Data privacy / local LLM / deterministic fallback** — a shared `LlmClient` abstraction that
   every AI feature (RiskAI, TraceLink scoring) uses, guaranteeing: local models, no data egress,
   and a deterministic rule path when the LLM is unavailable.

---

## Delivery order (recommended sequence)

| Wave | Focus | Items |
|------|-------|-------|
| **1** | Foundations + highest exposure | Cross-cutting #4 (LlmClient), #3 (shared report service); RiskAI 1.1, 1.2 |
| **2** | RiskAI depth | RiskAI 1.3, 1.4, 1.5, 1.6, 1.7 |
| **3** | Cert evidence | AssureCheck 2.2, 2.1, 2.3; IntegrateHub 6.1 |
| **4** | Collaboration | TraceLink 3.1, 3.2, 3.3, 3.4, 3.5; Cross-cutting #1, #2 |
| **5** | Coverage + ScenarioForge | TestForge 4.1, 4.2; ScenarioForge 5.1–5.5; IntegrateHub 6.2 |

**Definition of done (each module):** every work item has a passing CTest target, honest status
(no fabricated "measured" values where tooling is absent), and — for certification-facing items —
a human review gate per the project's Working rules.

---

*This plan is derived strictly from the app-level competitive gaps. DevOps/CI/test-infrastructure
items remain in `PLAN.md`; the two documents are complementary.*
