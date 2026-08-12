# Plan — Sprint 3: "Prove it, don't just build it"

Purpose: close every gap raised by the **2026-08-12 independent PM gap analysis**
(verified against the live build/test run and source tree, not against the
project's own prior status reports — see git history for that analysis if
needed). Sprint 1 made the product runnable; Sprint 2 made it broad
(RBAC, a web layer, workflow/evidence, structural-coverage/cert-export/OSLC/
baseband first slices — all real code, all tested). **Sprint 3 makes the
broad parts credible**: real CI, real coverage instrumentation, a real
multi-user web/data path, and a review process with a human in the loop.

Status: **NOT STARTED**
Context: Lodestar C++17 CMake monorepo (MSVC/Windows, vcpkg `x64-windows`).
Build: `cmake --build build --config Release`. Self-verify:
`./build/core/Release/lodestar_smoke.exe`. TraceLink, ScenarioForge, and
AssureCheck's compliance engine are the mature, well-tested core; treat them
as stable and avoid churn there unless a phase below specifically touches
them.

## Sprint 3 scope

| # | Work item | Area | Priority | Effort |
|---|-----------|------|----------|--------|
| S3.1 | Reconcile CI with the actual build platform; get one pipeline run genuinely green | Platform/CI | P0 | 1–2 wks |
| S3.2 | Wire the 21 phase-test suites into CTest with pass/fail + JUnit/XML output | Platform/CI | P0 | ~1 wk |
| S3.3 | Real structural code coverage (compiler instrumentation, not a stored percentage) | TestForge | P0 | 6–10 wks |
| S3.4 | Production-grade certification exports (templated, multi-page, evidence-embedded PDF/Word/ReQIF) | AssureCheck | P0 | 4–6 wks |
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

- **Phase 1 — CI/build reconciliation (S3.1).** `ci/Jenkinsfile` currently runs `sh` steps against a project that only builds via MSVC/vcpkg on Windows — it has never actually gone green. Either retarget it to call `ci/run_all_tests.ps1` on a Windows agent, or add a genuine cross-platform (Linux+CMake) build leg and fix whichever is intended to be canonical. Definition of done: one real CI run, visible pass/fail, not a claim.
- **Phase 2 — CTest wiring (S3.2).** Register all 21 `*_tests` targets with `add_test()`; emit JUnit/XML so pass rate is visible over time and in an IDE, not just via a shell script grepping exit codes.
- **Phase 3 — Real structural coverage (S3.3).** Replace the stored-percentage model (`core/testforge/Coverage.h`, migration 026) with actual compiler-instrumented coverage (source-based coverage via MSVC/LLVM instrumentation, or integration with an external engine) for statement/decision first, MC/DC next.
- **Phase 4 — Certification-grade exports (S3.4).** Move `CertReportService` from single-page/basic-XML output to templated, multi-page, evidence-embedded PDF/Word/ReQIF suitable for an actual audit package.
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

## Definition of done (Sprint 3)

- Phases 1–10 (all P0 + P1) implemented, tested, human-reviewed, and merged.
- A real CI run is green — not a claim, a link.
- Coverage numbers come from instrumentation, not a stored field.
- The web layer supports at least one real concurrent multi-user edit flow.
- P2 items (11–15) tracked but may roll into Sprint 4 if capacity is tight; Phase 15 (pilot) should start in parallel regardless of engineering capacity — it does not block on the others.

## Working rules

- Build with a hard timeout; run tests one at a time.
- Every phase's diff gets a named human reviewer before merging to `main` (S3.5) — this sprint has no engineer/tester/devops agent loop merging unreviewed.
- Commit as `chore(s3): ...` / `feat(s3): ...`; keep commits scoped to one phase.
- No new per-phase task/test/devops doc files in `docs/` — track scope and status in this file only. Keep the repo as source code plus the handful of docs that describe the product (`architecture.md`, `user-guide.md`, `support.md`, `assurecheck-standards-checklist.md`), not the process that built it.
