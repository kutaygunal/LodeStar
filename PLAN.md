# Plan — Sprint 3: "Prove it, don't just build it"

Purpose: close every gap raised by the **2026-08-12 independent PM gap analysis**
(verified against the live build/test run and source tree, not against the
project's own prior status reports — see git history for that analysis if
needed). Sprint 1 made the product runnable; Sprint 2 made it broad
(RBAC, a web layer, workflow/evidence, structural-coverage/cert-export/OSLC/
baseband first slices — all real code, all tested). **Sprint 3 makes the
broad parts credible**: real CI, real coverage instrumentation, a real
multi-user web/data path, and a review process with a human in the loop.

Status: **IN PROGRESS** — Phase 1 (CI/build reconciliation), Phase 2 (CTest wiring), Phase 3 (real statement coverage), and Phase 4 (certification-grade exports) DONE. Phase 5 (human review gate) is next — Phase 4's diff needs a named human reviewer before merge per the Working rules below; it has not had one yet.
Context: Lodestar C++17 CMake monorepo (MSVC/Windows, vcpkg `x64-windows`).
Build: `cmake --build build --config Release`. Self-verify:
`./build/core/Release/lodestar_smoke.exe`. TraceLink, ScenarioForge, and
AssureCheck's compliance engine are the mature, well-tested core; treat them
as stable and avoid churn there unless a phase below specifically touches
them.

## Sprint 3 scope

| # | Work item | Area | Priority | Effort |
|---|-----------|------|----------|--------|
| S3.1 | Reconcile CI with the actual build platform; get one pipeline run genuinely green | Platform/CI | P0 | **DONE** (code) |
| S3.2 | Wire the 21 phase-test suites into CTest with pass/fail + JUnit/XML output | Platform/CI | P0 | **DONE** |
| S3.3 | Real structural code coverage (compiler instrumentation, not a stored percentage) | TestForge | P0 | **DONE** (statement only — see below) |
| S3.4 | Production-grade certification exports (templated, multi-page, evidence-embedded PDF/Word/ReQIF) | AssureCheck | P0 | **DONE** |
| S3.5 | Human review gate on every merged phase (process change, see Working rules) | Process | P0 | immediate |
| S3.6 | Interactive, editable web collaboration layer (JS frontend, real-time multi-user) | Platform | P1 | 8–12 wks |
| S3.7 | Client-server persistence path for the collaborative/web deployment mode | Platform | P1 | 4–8 wks |
| S3.8 | Security hardening: iterated password KDF, session/token expiry, dependency vuln scanning | Platform | P1 | 2–3 wks |
| S3.9 | Full multi-GNSS baseband (GLONASS/BeiDou, multi-satellite RF synthesis) + real automation binding | ScenarioForge | P1 | 6–10 wks |
| S3.10 | Full OSLC server (discovery, resource-shape catalog, query) | Platform | P1 | 2–3 wks |
| S3.11 | Build the Python layer the README already promises, or drop the claim | Platform | P2 | 3–4 wks |
| S3.12 | Enforce the Evaluation/Professional/Team license tiers named in `LICENSE.md` | Platform | P2 | 2–3 wks |
| S3.13 | Static analysis / sanitizers (clang-tidy, ASan in a debug CI leg) | Platform/CI | P2 | 1–2 wks |
| S3.14 | Telemetry, crash reporting, and an update mechanism for the installer | Platform | P2 | 2–3 wks |
| S3.15 | Pilot the actual workflow with one real IT&V lead or avionics engineer | GTM (non-eng) | P2 | ongoing |

## Phase breakdown

- **Phase 1 — CI/build reconciliation (S3.1). DONE (code).** `ci/Jenkinsfile` now runs `bat` steps on a Windows agent and configures via the new `windows` CMake preset (`CMakePresets.json`), which reads the vcpkg toolchain from `$env:VCPKG_ROOT` instead of a hardcoded personal path. Verified locally: `cmake --preset windows` configures cleanly into a fresh build dir. **Not yet verified**: an actual green run on a real Jenkins Windows agent — no Jenkins instance was available to this session, so this is code-complete but pipeline-unverified. Next person with Jenkins access should confirm.
- **Phase 2 — CTest wiring (S3.2). DONE.** All 52 `lodestar_*_tests` targets are registered with `add_test()` (name-pattern discovery, not hand-listed) and `enable_testing()` is called at the top level so `ctest --test-dir build -C Release` finds them from the build root. `--output-junit` confirmed working. The Jenkinsfile's test-gate stage now runs `ctest` instead of the old exe-grepping script. **Found in the process**: `lodestar_s1_phase4_tests` failed once in a full 52-test run but passed standalone and on repeat full runs — looks like Windows file-handle contention on its fixed-name SQLite fixture files (`lodestar_s1p4_*.db`), not a real regression. Not root-caused yet; tracked below, not silently retried away.
- **Phase 3 — Real structural coverage (S3.3). DONE for statement coverage; decision/MC-DC still open.** `core/testforge/CoberturaImport.{h,cpp}` imports a real Cobertura XML report — produced by OpenCppCoverage (already installed on the build machine at `C:\Program Files\OpenCppCoverage`) run against a Debug build of the full CTest suite via `ci/run_coverage.ps1` — into `coverage_results`, replacing the caller-supplied-percentage model with actually-measured data. `ci/run_coverage.ps1` uses its own build tree (`build-coverage/`, gitignored) and wraps `ctest` itself with `--cover_children` so every test binary is instrumented and aggregated into one report in a single pass.
  Final verified state: **74.77% real statement coverage** across 94 `core/` source files (8,714/11,654 statements), full run clean at 100% tests passed (51/51) in ~80s of actual test time. Idempotent re-import confirmed (re-running under the same run id updates rows in place).
  Two real bugs were found and fixed while building this — both are the actual point of Sprint 3 ("prove it, don't just build it"), not incidental:
  1. **Cross-binary aggregation.** With `--cover_children`, the same source file appears once per test binary (a shared header can show up 40+ times in one report). The importer originally let later binaries silently overwrite earlier ones; fixed to union hit-status per line number across all occurrences instead. A fixture-based regression test (`core/test/s3_phase3_tests.cpp`) with two packages touching the same file locks this in.
  2. **CTest's `--timeout` CLI flag doesn't override an explicit per-test `TIMEOUT` property** — verified empirically (Phase 2 set `TIMEOUT 120` on every test; passing `ctest --timeout 600` still killed tests at 120.0Xs). Fixed by making the timeout a CMake cache variable (`LODESTAR_TEST_TIMEOUT_SECS`, default 120) that `ci/run_coverage.ps1` overrides to 900 at configure time in its own build tree, leaving the normal CI gate's tight 120s hang-detection untouched. Even at 900s, two binaries still couldn't complete under instrumentation and are excluded from the coverage run specifically (full evidence and rationale in the script): `lodestar_wp8_tests` (a 10k-entity/50k-link bulk-load test still hadn't finished at 900s — real overhead of per-line breakpoint-trap instrumentation on a 60k-iteration loop, a documented OpenCppCoverage characteristic) and `lodestar_wp5_assurecheck_tests` (has a hardcoded internal 60,000ms *wall-clock* budget assertion — `core/test/wp5_assurecheck_tests.cpp:360` — that measuring real-world speed makes fail under any instrumentation by design, not by any timeout setting). Both still run at full scale in the normal, non-coverage CI test gate; excluding them from the coverage pass only affects which lines get measured, and the report correctly shows their bulk-scale-only lines as unmeasured rather than fabricating them as covered.
  **Not done**: decision and MC/DC coverage. OpenCppCoverage 0.9.9 (the only coverage tool installed) is statement/line-only — verified via `--help`, no branch/decision flag exists. VS Community's bundled LLVM only ships `clang-tidy`/`clang-format`, not `clang-cl`/`llvm-cov`. Getting real decision/MC-DC would need installing additional tooling (clang-cl + llvm-cov, or a commercial engine) — a deliberate scope boundary for this pass, not silently dropped: `decisions_total`/`conditions_total` are left at 0 (honest "not measured") rather than fabricated.
- **Phase 4 — Certification-grade exports (S3.4). DONE.** `CertReportService`
  (`core/assurecheck/CertReportService.{h,cpp}`) moved from single-page/
  basic-XML output to templated, multi-page, evidence-embedded PDF/Word/
  ReQIF. Verified by `core/test/s3_phase4_tests.cpp` (46 assertions across 5
  sections, 0 failures) plus the original `core/test/s2_phase8_tests.cpp`
  contract, unchanged and still green (both registered with CTest; full
  54-test suite 100% passed, ~33s).
  **What changed, against the seven gaps in the scope brief below:**
  1. **PDF (`buildPdf`).** Rewritten around a format-agnostic `ReportDoc` +
     a paginated `pdf::Unit` list: real multi-page output (page objects +
     content-stream objects generated per page, not one), a bold
     Helvetica-Bold title, one `Tj` per line (the old single-Tj-blob bug is
     gone), a 5-column table (Item/Status/DAL/Objective/Evidence) with rule
     lines and the checklist header re-emitted on continuation pages, and a
     "Page X of N" footer. No word-wrap (a stated scope boundary, not a
     silent gap): long cell text is truncated to its column's character
     budget — see the hand-roll-decision comment at the top of
     CertReportService.cpp.
  2. **Word (`exportWord`).** The docx zip now has every part a real docx
     needs: `docProps/core.xml`, `docProps/app.xml`, `word/styles.xml`,
     `word/settings.xml`, `word/fontTable.xml`, `word/_rels/document.xml.rels`,
     and `_rels/.rels` now references `docProps/`. Content is a real
     `<w:tbl>` for the checklist rows (not one paragraph per line) plus
     Title/Heading1 styled paragraphs, verified structurally (parsed back
     out of the zip) by `s3_phase4_tests.cpp` T2, not just "opens in Word" —
     no Word/LibreOffice instance was available in this session to do that
     smoke test; do it before calling this fully closed out.
  3. **ReQIF (`buildReqif`).** Added the missing `DATATYPES` and
     `SPEC-TYPES` sections (`REQ-TYPE` with `SPEC-ATTRIBUTES`, `TRACE-TYPE`),
     so `SPEC-OBJECT-TYPE-REF`/`SPEC-RELATION-TYPE-REF`/attribute/datatype
     refs all resolve — verified by a hand-rolled identifier/reference
     collector in the test (T3), not just substring presence. Also added a
     minimal `TOOL-EXTENSIONS` element.
  4. **Evidence.** `ReportRow` gained a `resultId` field (populated from
     `CheckResult.id` in `ReportService::buildReport`); `row.evidence` is
     now rendered as its own table/cell column in both PDF and Word instead
     of being read and discarded.
  5. **Workflow audit trail.** `CertReportService` now owns a
     `WorkflowService` member and pulls `auditLog(row.resultId)` per row
     into a "Review & Approval Audit Trail" section of both exports —
     verified end-to-end in `s3_phase4_tests.cpp` T4 (seed → submit → approve
     → export → assert both actors appear in the extracted PDF text and the
     Word XML).
  6. **`traceResultToRequirements` naming.** Renamed to
     `verifiedRequirementsForTestCase` to match what it actually does
     (resolve a TraceLink `test_case` entity id to the requirements it
     verifies); `traceResultToRequirements` kept as a non-breaking
     deprecated alias so `s2_phase8_tests.cpp` didn't need to change.
     **Decision, not a default:** did *not* add a TestForge-run-based
     resolver — `test_runs`/`test_procedures` (migration 002) have no
     foreign key to TraceLink's `test_case` entities (migration 020's
     `trace_links` table), so a "real TestForge run id" resolver would have
     nothing real to resolve through; inventing one would mean either
     fabricating a link or a schema change, both out of scope here.
  7. **Test coverage.** New `core/test/s3_phase4_tests.cpp` asserts real
     structural properties: PDF page count via the `/Type /Pages /Count`
     object plus a hand-rolled `Tj`-literal text extractor (proves both
     pagination *and* that no rows were dropped across the page break);
     docx parsed back out of its own zip format and checked for every
     required OOXML part plus exact `<w:tbl>`/`<w:tr>` counts; ReQIF
     type-ref resolution via the identifier/reference collector in point 3.
     The old `s2_phase8_tests.cpp` (byte-count/substring assertions) is left
     in place unchanged as a regression/back-compat check, not replaced.
  **Decision on hand-rolled vs. library (the brief asked for one,
  explicitly):** stayed hand-rolled. `vcpkg.json` still has exactly one
  dependency (`sqlite3`); every other binary/text format in this codebase
  (SkydelAdapter's HTTP client, the OSLC RDF/XML writer, the Cobertura
  importer) is hand-rolled the same way, and this project's differentiator
  is being buildable offline/on-prem for an avionics audience, so a new
  vcpkg dependency was judged a worse trade than the extra format-writing
  code. Full rationale is in the file header comment of
  `CertReportService.cpp`. If a future phase needs real text layout
  (word-wrap, images, per-section headers), that's the point to revisit
  this call.
- **Phase 5 — Human review gate (S3.5).** No code phase — a process change: every phase's diff gets a named human reviewer before it merges to `main`. Record the reviewer in the commit trailer.
- **Phase 6 — Web collaboration layer (S3.6).** Replace the read-only server-rendered `WebServer` pages with a real JS frontend: editing, not just viewing; live updates for concurrent viewers.
- **Phase 7 — Client-server persistence (S3.7).** Give the web/collaborative deployment mode a real multi-client data path (connection-pooled server DB) instead of a single local SQLite file; keep SQLite for the single-user desktop mode.
- **Phase 8 — Security hardening (S3.8).** Swap salted SHA-256 for an iterated KDF (argon2/bcrypt/PBKDF2), add session/token expiry + revocation, add dependency vulnerability scanning for the vcpkg manifest.
- **Phase 9 — Multi-GNSS baseband (S3.9).** Add GLONASS (non-Keplerian broadcast model) and BeiDou constellations; move the I/Q generator from single-PRN to synthesized multi-satellite RF; give the automation API a real REST/Python binding instead of an in-memory SCPI-style stub.
- **Phase 10 — Full OSLC server (S3.10).** Service discovery + resource-shape catalog + query, beyond the current single-resource RDF/XML serializer.
- **Phase 11 — Python layer (S3.11).** Decide and execute: either ship the automation/bindings layer the README's stack line promises, or edit the README to stop promising it. Either is an acceptable outcome; leaving the mismatch is not.
- **Phase 12 — License enforcement (S3.12).** Enforce the Evaluation/Professional/Team tiers named in `LICENSE.md` in code (entitlement check, feature gating), not legal text alone.
- **Phase 13 — Static analysis (S3.13).** Add clang-tidy and an ASan/UBSan debug CI leg, appropriate given the product's own DO-178C-adjacent audience.
- **Phase 14 — Installer lifecycle (S3.14).** Crash reporting, basic telemetry (opt-in), and an update mechanism for the packaged installer.
- **Phase 15 — Pilot (S3.15).** Get one real IT&V lead or avionics engineer to run the actual workflow end-to-end and report back. Non-engineering, but nothing above matters commercially until this happens once.

## Phase 4 scope brief — certification-grade exports (S3.4)

Self-contained brief for whoever/whatever picks up Phase 4. Written against
the actual current code (not aspirational) — every gap below was verified by
reading `core/assurecheck/CertReportService.cpp` (337 lines) and
`core/test/s2_phase8_tests.cpp` (275 lines) directly, this session.

**Objective.** `CertReportService` (`core/assurecheck/CertReportService.{h,cpp}`)
exports a `ComplianceReport` (see `core/assurecheck/ReportService.h`) as PDF,
Word (docx), and ReQIF, plus resolves result→requirement traceability. All
three exports currently produce *valid-format but minimal* output — they
pass their own tests (which only assert "non-empty bytes" / substring
presence) but would not hold up as real certification evidence. Phase 4 is
the depth pass: make the same three exports genuinely audit-ready.

**Exactly what's thin today, file:line-grounded:**

1. **PDF (`buildPdf`, CertReportService.cpp:52-89).** Single page, Helvetica
   only. The entire multi-line report body is passed to *one* `Tj` (show
   text) operator — PDF doesn't interpret embedded `\n` as a line break, so
   this likely renders as a single garbled line, not multiple lines. No
   page breaks, no title/header styling, no table layout for the
   pass/fail rows, no page numbers.
2. **Word (`exportWord`, CertReportService.cpp:253-297).** The docx zip is
   missing parts a real `.docx` normally has: `docProps/core.xml`,
   `docProps/app.xml`, `word/styles.xml`, `word/settings.xml`,
   `word/fontTable.xml`, and `_rels/.rels` doesn't reference `docProps/`.
   Worth checking early whether the current output even opens cleanly in
   real Word/LibreOffice — that's the first useful smoke test for this
   phase. Content is one plain unstyled paragraph per line; no table, no
   heading styles, no title page.
3. **ReQIF (`buildReqif`, CertReportService.cpp:188-223).** `SPEC-OBJECTS`
   reference `SPEC-OBJECT-TYPE-REF="REQ-TYPE"` and `SPEC-RELATIONS`
   reference `SPEC-RELATION-TYPE-REF="TRACE-TYPE"`, but neither `REQ-TYPE`
   nor `TRACE-TYPE` is ever *defined* — there's no `SPEC-TYPES` section at
   all. A real ReQIF consumer (DOORS, Polarion, Codebeamer) validates
   against the schema and will very likely reject a file with dangling
   type refs. Also missing: `DATATYPES`, `SPEC-OBJECT-TYPE` attribute
   definitions, `TOOL-EXTENSIONS`.
4. **Evidence is dropped on the floor.** `ReportRow.evidence` (a real,
   already-populated field — "evidence links summary", see
   `core/assurecheck/ReportService.h:29`) is **never read** by
   `reportBody()` (CertReportService.cpp:226-241) or any exporter. The data
   the export is supposed to be "evidence-embedded" for already exists and
   is simply not being used.
5. **No workflow/audit trail in the export.** `WorkflowService`
   (`core/assurecheck/WorkflowService.h`, built in S2 Phase 3) has a real
   `AuditEntry` (actor, action, timestamp, from-state, to-state) and
   `EvidencePackage` (objective → evidence links) — neither is pulled into
   `CertReportService`. A real certification package needs to show who
   reviewed/approved each objective and when; right now the export can't
   answer that at all.
6. **`traceResultToRequirements(resultId)` doesn't actually take a
   TestForge result.** (CertReportService.cpp:307-335). Despite the name,
   it takes a TraceLink `test_case` entity id directly (confirmed by the
   test itself passing `"tc1"`, a test_case id, as `resultId` —
   `core/test/s2_phase8_tests.cpp:186`) and just re-runs a query TraceLink
   can already answer. It never touches an actual TestForge `TestRun`/step
   result. Decide whether to rename it to match what it does, or make it
   take a real TestForge run id and resolve through to *passed* results
   specifically (arguably the more useful traceability question for a cert
   package: "which requirements does this passing run verify").
7. **Test coverage matches the shallowness.** The existing S2 Phase 8 tests
   only assert non-empty bytes / `size() > 100` for PDF and Word, and a
   substring search for ReQIF. Phase 4 needs real assertions: PDF actually
   has N pages and the expected text is extractable; Word actually parses
   as valid OOXML with the expected paragraphs/tables; ReQIF validates
   against having complete `SPEC-TYPES` and round-trips through a parser
   (even a hand-rolled one, matching this project's existing
   `core/testforge/CoberturaImport.cpp` precedent of writing a small
   targeted parser rather than pulling in a full XML library — same
   judgment call applies here for ReQIF/OOXML if no library is added).

**A decision this phase should make explicitly, not by default:** whether to
pull in a real PDF/DOCX-generation library via vcpkg (`vcpkg.json` currently
has exactly one dependency, `sqlite3` — see project root) or to keep
hand-rolling the formats as this codebase has consistently done everywhere
else (SkydelAdapter's HTTP client, the OSLC RDF/XML writer, the Cobertura
importer). Hand-rolling a *correct* multi-page PDF and a *complete* OOXML
docx is very doable but is real, fiddly binary-format work; a library
trades a new dependency for a lot of correctness for free. Either is
reasonable — pick one and say why, don't default silently into partial work
on both.

**Suggested order:** (1) fix ReQIF `SPEC-TYPES` first — it's the smallest,
most clearly "wrong not just thin" gap and unblocks real DOORS/Polarion
interop testing; (2) wire in `row.evidence` and `WorkflowService`'s audit
trail — pure data-plumbing, no format work, high value; (3) then the
PDF/Word depth work, after the library-vs-hand-rolled decision above; (4)
rewrite the tests to match, verifying real structural properties, not byte
counts.

**Build/verify** (see also `docs/architecture.md`, `docs/user-guide.md`):
`cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`, then
`cmake --build build --config Release`, then
`ctest --test-dir build -C Release -R s2_phase8` (existing suite) once
extended, or add a new `s3_phase4_tests` target following the pattern in
`core/CMakeLists.txt` (search for `s3_phase3` for the most recent example —
add the executable, link `lodestar_assurecheck`, it's auto-registered with
CTest by the name-pattern loop). Windows/MSVC only; follow the working
rules below (hard build timeouts, one test at a time, no new per-phase doc
files — status goes in this file).

## Definition of done (Sprint 3)

- Phases 1–10 (all P0 + P1) implemented, tested, human-reviewed, and merged.
- A real CI run is green — not a claim, a link.
- Coverage numbers come from instrumentation, not a stored field.
- The web layer supports at least one real concurrent multi-user edit flow.
- P2 items (11–15) tracked but may roll into Sprint 4 if capacity is tight; Phase 15 (pilot) should start in parallel regardless of engineering capacity — it does not block on the others.

## Known issues found during Sprint 3

- **`lodestar_s1_phase4_tests` intermittent failure — FIXED.** Root cause was *not* file locking (that was a wrong first guess — logged here for the record since it was stated as such earlier). Re-running with `--output-on-failure` caught the real symptom: `coordinationFor()` returned the right *count* of notes but in the wrong *order*. `IntegrateHubService::addCoordination()` was populating `created_at` with `newUuid()` instead of a real timestamp (copy-paste from the adjacent `id = newUuid()` line), and `coordinationFor()`'s `ORDER BY created_at, id` used the row's random UUID `id` as a tiebreaker — which migration 022's own comment says should have been `rowid` (SQLite's monotonic, insertion-order column) for exactly this reason. Two notes added within the same test therefore sorted randomly. Fixed both: `created_at` now comes from `common::nowIso()` (added in Sprint 2 for this exact "'now' placeholder" problem elsewhere, just never applied to `IntegrateHubService.cpp`), and both `coordinationFor()` and `listIssues()` now tiebreak `ORDER BY created_at, rowid`. Verified: 25/25 standalone runs and 4/4 full 52-test `ctest` runs clean (was failing ~3/5 full runs before). See `core/integratehub/IntegrateHubService.cpp`.
- **`ci/Jenkinsfile` is code-complete but pipeline-unverified.** Fixed to run on Windows and use the `windows` CMake preset; every command it runs was verified directly in this session. Not yet run through an actual Jenkins Windows agent — do that before calling S3.1 fully done, not just "done in code."

## Working rules

- Build with a hard timeout; run tests one at a time.
- Every phase's diff gets a named human reviewer before merging to `main` (S3.5) — this sprint has no engineer/tester/devops agent loop merging unreviewed.
- Commit as `chore(s3): ...` / `feat(s3): ...`; keep commits scoped to one phase.
- No new per-phase task/test/devops doc files in `docs/` — track scope and status in this file only. Keep the repo as source code plus the handful of docs that describe the product (`architecture.md`, `user-guide.md`, `support.md`, `assurecheck-standards-checklist.md`), not the process that built it.
