# VeriNav — GNSS/SBAS Integrated Test & Verification Platform

> **Flagship project idea**

One platform that automates the full IT&V lifecycle for GNSS/SBAS systems — from scenario generation to certification-ready reporting.

## Pitch

A single, modular C++/Python platform that plans, executes, analyzes, and reports integration & verification for GPS/Galileo/SBAS hardware and software — with built-in DO-178C/ARP4754A/ARP4761 compliance, FMEA/hazard analysis, and a cross-disciplinary integration hub. It is the tool an IT&V lead would build to run the entire program.

**Stack:** C++ core · Python · Qt dashboard · Jenkins CI/CD · local LLMs (Qwen/Gemma) · parallel execution · .NET service layer

## Modules

### ScenarioForge — GNSS / SBAS
Generates realistic GPS/Galileo/SBAS RF and data scenarios for test injection — covers the GNSS and SBAS experience requirement.

### TestForge — IT&V Plans
Auto-generates, executes, and reports test procedures with data analysis — the core IT&V deliverable, cutting authoring effort ~90%.

### TraceLink — Systems Engineering
Links requirements, design, interfaces, and test cases for full traceability — the systems-engineering backbone.

### AssureCheck — Assurance Standards
Automated compliance checks against ARP4754A, ARP4761, DO-178C, DO-278A, DO-254 for safety, reliability, and airworthiness.

### RiskAI — Risk & Safety
LLM-assisted hazard assessments and FMEA with risk mitigation tracking — the risk & safety analysis requirement.

### IntegrateHub — Cross-Disciplinary
Central hub connecting software, hardware, RF, electrical, and mission-ops data and issue resolution — the coordination requirement.

### CI/CD + Metrics — Software Dev / CI
Jenkins pipeline and a Qt metrics dashboard for continuous integration, test coverage, and program health.

## Requirement → Module Coverage

| Job Requirement | VeriNav Module | How It's Covered |
|---|---|---|
| Develop & execute IT&V plans (test-procedure creation, execution, data analysis, reporting) | TestForge | Auto-generates, runs, and reports DO-178C test procedures with data analysis. |
| Apply ARP4754A, ARP4761, DO-178C, DO-278A, DO-254 | AssureCheck | Automated compliance checks and certification evidence for each standard. |
| Hazard assessments, FMEA, risk mitigation | RiskAI | LLM-assisted FMEA/hazard analysis with risk tracking and mitigation. |
| Cross-disciplinary coordination (software, hardware, RF, electrical, mission-ops) | IntegrateHub | Unified integration hub for cross-team data and issue resolution. |
| GNSS experience (GPS, Galileo) | ScenarioForge | Realistic GPS/Galileo scenario generation for test injection. |
| Complex software dev + CI/CD (C++, Java, Python) | CI/CD + Metrics | Jenkins pipeline, C++/Python core, Qt metrics dashboard. |
| SBAS familiarity | ScenarioForge | SBAS augmentation scenario and integrity verification. |
| Systems engineering (requirements, design, interfaces) | TraceLink | Requirements-to-test traceability and interface management. |

## Market Context (research notes)

- Every individual capability exists as a commercial product, but **no single integrated platform** combines them.
- **GNSS scenario generation:** Spirent (GSS9000 + SimGEN + PNT-Automation), Rohde & Schwarz (SMW200A + GNSS test suites K360–K364), Keysight, u-blox, Skydel (software-defined). Proprietary, closed, expensive.
- **DO-178C test/verification:** LDRA tool suite, Rapita Systems (RapiTestFramework + CBMC), QA Systems, VectorCAST. Software-only, closed, per-seat licensed.
- **Requirements traceability:** IBM DOORS, Jama Connect, Polarion. Mature, entrenched.
- **FMEA / hazard / risk:** APIS IQ, XFMEA, Reliability Workbench. Separate, mostly manual, none LLM-assisted.
- **The gap:** No product owns the *integration layer* and *automation of the IT&V workflow itself*. An IT&V lead must stitch together 4–6 disconnected tools today.

### Strategic recommendation
Position VeriNav as the **orchestration/integration layer** that wraps existing tools via their APIs (Spirent and R&S both expose remote-control/automation interfaces) rather than re-implementing RF signal generation or MC/DC analysis from scratch. Build genuinely novel value in **ScenarioForge** (software-defined GNSS scenario generator) and **RiskAI** (LLM-assisted FMEA).
