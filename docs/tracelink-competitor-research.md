# TraceLink — Competitor Research Report

**Date:** 2026-08-12
**Author:** tracelink-research (research subagent)
**Scope:** Requirements-traceability / systems-engineering / ALM tools similar to TraceLink
**Screenshots:** `docs/research/screenshots/` (16 images)

---

## 1. Executive summary

TraceLink is a commercial-grade requirements-traceability and systems-engineering module
(typed entities, typed directed trace graph, impact analysis, coverage analysis, traceability
matrix, compliance rules for ARP4754A/ARP4761/DO-178C/DO-254, baselines/versioning,
change-request workflow, ReqIF/CSV/HTML import-export, REST API, and a Qt desktop UI with
matrix/graph/impact/coverage views).

The competitive landscape is dominated by large commercial ALM suites (IBM DOORS Next,
Siemens Polarion, PTC Codebeamer, Jama Connect, Perforce Helix ALM), specialist
requirements tools (Visure, ReqSuite RM, Innoslate), MBSE/modeling tools with requirements
capabilities (Capella, Cameo Systems Modeler, Enterprise Architect), and open-source tools
(ReqIF Studio, ProR/RMF, OpenReq, redmine-based).

**Key takeaways for TraceLink:**
- The market is converging on **browser-based, collaborative, single-source-of-truth**
  platforms. TraceLink's Qt desktop UI is a differentiator for offline/embedded work but
  should be complemented by a web/API-first story.
- **AI assistance** (quality scoring, suspect/duplicate detection, requirement generation)
  is now table stakes across DOORS, Jama, Visure, ReqSuite, Innoslate, and Polarion.
- **Suspect-link / impact analysis** is the single most common "killer feature" — every
  leading tool flags downstream artifacts when a requirement changes.
- **Coverage dashboards** (percentage of requirements covered by tests) are a universal
  pattern and a strong audit aid.
- **ReqIF** is the de-facto interchange standard; TraceLink already supports it, which is a
  strong competitive point.

---

## 2. Comparison table

| Product | Core functionality | Selling points / differentiators | Target market | UI style | Pricing |
|---|---|---|---|---|---|
| **IBM DOORS Next (DOORS NG)** | Requirements storage/linking, traceability, impact analysis (suspect links), reviews, metrics/dashboards, baselines, config mgmt, OSLC+ReqIF, AI (Watson) | AI-assisted quality, enterprise governance, Jazz platform, OSLC integration, concurrent editing, scalability | Aerospace/defense, regulated, large enterprise | Web (Jazz), module tables, Links Explorer graph, dashboards, left-nav | Per-user subscription (not public) |
| **Jama Connect** | Requirements, Trace View (up/downstream), Coverage Explorer, Impact Analysis, Live Trace Explorer, relationship rules, baselines, reviews, test mgmt | Ease of use, Live Trace Explorer, coverage-gap detection, cloud + self-hosted | Medical devices, automotive, regulated | Web, project tree left-nav, Trace View columns, Live Trace Explorer diagram | Subscription per-user |
| **Siemens Polarion ALM** | Requirements (LiveDocs), traceability, workflow, test mgmt, change/config mgmt, risk, planning, build/release, audits/metrics, reuse/branching, variants | LiveDocs, 100% browser, unified ALM, "time machine", open APIs, SPICE support | Automotive (SPICE), aerospace, general ALM | Browser, LiveDocs documents, work-item tables, traceability matrix, dashboards | Subscription per-user (REQ/QA/ALM editions) |
| **PTC Codebeamer** | Requirements, risk, test, validation, product-line engineering, MBSE, source code, change mgmt, OSLC, ReqIF | All-in-one ALM, OOTB regulatory templates, OSLC, V-cycle traceability, DOORS bridge | Automotive, medical, aerospace, complex products | Web, tracker items, traceability section (up/downstream levels), matrix | Subscription, free trial |
| **Perforce Helix ALM** | Requirements, test-case mgmt, issue tracking, traceability, impact analysis, traceability diagrams, baselines, workflow, reports | Integrated with Helix Core (SCM), traceability diagrams, impact analysis | General software, regulated | Web client, Traceability tab, diagram (Stress/Hierarchy), impact table | Per-user subscription |
| **Visure Requirements** | Requirements, traceability, risk, test, baselines, reuse, AI quality analysis, DO-178C/DO-254, ReqIF/Word/Excel | DO-178C/DO-254 certification, cost-effective, AI, easy UX, 14-day trial | Aerospace/defense, medical, automotive, safety-critical | Document-style authoring, traceability dashboard, matrix | Subscription ("fraction of competitors") |
| **ReqSuite RM (Osseno)** | Requirements, change mgmt, traceability, baselines, versioning, impact analysis, consistency checks, reuse, AI, ReqIF/Word/Excel, REST API, integrations | Smart/intelligent assistance, intuitive, flexible customization, fast implementation, ISO 26262/DO-178C/ISO 62304 | SMEs, automotive, medical, public sector | Web, structured forms, dependency diagrams, matrix | One-off + maintenance or subscription; named/floating licenses |
| **Innoslate (SPEC Innovations)** | Requirements, traceability matrix, quality check (NLP), baselines, Tree/Spider diagrams, MBSE (SysML/LML/DoDAF/UAF), test center, simulation, project mgmt, AI | Cloud-native, AI-powered, real-time collaboration, connected engineering, Traceability/Suspect Assist | Systems engineering, aerospace, defense, general | Web browser, Documents View, Traceability Matrix, Tree/Spider diagrams, Charts Dashboard | Subscription |
| **Capella / Eclipse (Arcadia)** | MBSE modeling, architecture diagrams, requirements viewpoint (ReqIF import), traceability links, model validation, semantic browser, computed links | Open source (EPL), Arcadia method, extensible, free | Aerospace, defense, energy, transportation (MBSE) | Eclipse desktop, diagrams, semantic browser, project explorer | Open source (free); commercial add-ons |
| **Cameo Systems Modeler / MagicDraw (Dassault)** | SysML modeling, requirement diagrams, requirement tables, traceability (Dependency Matrix, Relation Map), ReqIF, validation, suspect links, publish | SysML standard, MBSE, ReqIF interchange, predefined traceability suites | Systems engineering, aerospace, defense | Desktop (MagicDraw), Model Browser, Requirement Diagram, matrices | Commercial per-user |
| **Enterprise Architect (Sparx)** | Requirements modeling, Specification Manager, Relationship Matrix, Traceability window, baselines, auditing, dashboard diagrams, CSV/DOORS import-export, SysML/UML | Comprehensive modeling, low cost, flexible, MDG Link for DOORS | General software, systems, business analysis | Desktop, Project Browser, Specification Manager (spreadsheet), diagrams, matrix | Per-user license (one-time), editions |
| **Open-source (ReqIF Studio, ProR/RMF, OpenReq, redmine)** | ReqIF editing, traceability matrix (Deduct), validation (Consequent), requirements platform, recommendation/decision, issue tracking | Free, open standards (ReqIF), extensible, community | Research, SMEs, cost-sensitive teams | Desktop (Eclipse) / web | Open source (free) |

---

## 3. Product deep-dives

### 3.1 IBM DOORS Next (DOORS NG) — IBM Engineering Lifecycle Management

**Core functionality**
- Store, categorize, link, and share requirements across stakeholder/business/system/product/
  hardware/software levels in a common repository.
- Typed, directional traceability links ("Satisfies", "Validated by", "Tracked by", etc.).
- **Links Explorer** graphically displays links; when an artifact changes, linked artifacts are
  automatically flagged **suspect** for impact analysis.
- Review workflows (reviews, comments, votes, approval), project dashboards, and metrics
  (e.g., "Requirements covered by test cases").
- Baselines, configuration management (streams, change sets, global configurations), and
  full change history with undo/restore.
- OSLC integration and ReqIF import/export.
- AI (Watson) automations: quality scores, wording recommendations, conversational queries,
  summaries, translations.

**Selling points / differentiators**
- AI-assisted requirements quality with auditable outcomes.
- Enterprise governance, scalability, and concurrent editing.
- Deep OSLC integration across the IBM ELM suite (test, change, architecture).
- Proven in high-compliance aerospace/defense programs for decades.

**Target market:** Aerospace/defense, regulated industries, large enterprise.

**UI/UX style:** Web client on the Jazz platform. Module/table views, grid/tree traceability
views, Links Explorer graph, project dashboards, left navigation, right-side artifact panel.

**Pricing:** Per-user subscription (IBM ELM); not publicly listed.

### 3.2 Jama Connect

**Core functionality**
- Requirements management with relationship rules between item types.
- **Trace View** (upstream + downstream, 2 levels, real-time editing), **Coverage Explorer**
  (downstream coverage across sets), **Impact Analysis** (single-item quick check).
- **Live Trace Explorer** — an at-a-glance traceability diagram with coverage percentage and
  suspect-link quality indicators.
- Baselines, saved views, review workflows, test management.

**Selling points / differentiators**
- Ease of use and low learning curve.
- Live Trace Explorer for interactive coverage verification and gap detection.
- Cloud and self-hosted deployment.

**Target market:** Medical devices, automotive, regulated product development.

**UI/UX style:** Web-based. Project tree left nav, Trace View as columns with blue arrows,
Live Trace Explorer as a generated diagram, red exclamation marks for missing coverage.

**Pricing:** Subscription per-user.

### 3.3 Siemens Polarion ALM

**Core functionality**
- Requirements authoring via **Polarion LiveDocs** (online structured specification documents
  with uniquely identifiable, traceable paragraphs).
- End-to-end traceability, workflow automation, change and configuration management,
  test management, risk management, planning/resource management, build/release management,
  audits/metrics/reports, reuse and branching, variants.
- "Time machine" to browse/search/report any historical state.
- ReqIF, Word/Excel import-export, OSLC, open Java/REST/Webservices APIs.

**Selling points / differentiators**
- LiveDocs innovation (document-centric authoring with traceability).
- 100% browser-based unified ALM; single source of truth.
- Strong automotive SPICE support and compliance templates.
- Extensions platform (10,000 members, 150 extensions).

**Target market:** Automotive (SPICE), aerospace, general ALM.

**UI/UX style:** Browser-based. LiveDocs documents, work-item tables, traceability matrix,
impact/traceability tree tables, dashboards, role-based "hats".

**Pricing:** Subscription per-user; editions (Reviewer/REQ/QA/ALM).

### 3.4 PTC Codebeamer

**Core functionality**
- Requirements management plus built-in risk, test, and validation management.
- Full traceability from requirements through testing and validation (V-cycle).
- Product-line engineering, MBSE, source-code management, change management.
- OSLC integrations, ReqIF support, DOORS bridge for migration.
- OOTB regulatory templates and workflows.

**Selling points / differentiators**
- All-in-one ALM platform (single license) vs. standalone RM tools.
- OOTB regulatory templates reduce time to compliance.
- Real-time V-cycle traceability; strong automotive/medical adoption (VW, Medtronic).

**Target market:** Automotive, medical, aerospace, complex software-defined products.

**UI/UX style:** Web-based. Tracker items with a Traceability section showing upstream/
downstream references in configurable levels (up to 10), matrix views.

**Pricing:** Subscription; free trial.

### 3.5 Perforce Helix ALM

**Core functionality**
- Requirements, test-case management, and issue tracking.
- Traceability with impact analysis (forward/backward), suspect marking.
- **Diagramming traceability** — node/line diagrams with Stress and Hierarchy layouts,
  grouping of similar items, save-to-PNG.
- Baselines, snapshots, workflow events, reports (forward/backward traceability, coverage).

**Selling points / differentiators**
- Tight integration with Helix Core (SCM) for code-to-requirement traceability.
- Interactive traceability diagrams and impact analysis.

**Target market:** General software, regulated development.

**UI/UX style:** Web client. Traceability tab, diagram view, impact-analysis table, item
detail pages with tabbed panes (Overview/Detail/Workflow/Traceability/Baselines/History).

**Pricing:** Per-user subscription.

### 3.6 Visure Requirements

**Core functionality**
- Requirements, risk, and test management in one platform.
- End-to-end traceability (including source code), impact analysis, cross-project traceability.
- Baselines, reuse, version comparison, change management.
- AI quality analysis, automated checklists, DO-178C/DO-254 compliance templates.
- ReqIF, Word, Excel import/export; broad tool integrations.

**Selling points / differentiators**
- Strong DO-178C/DO-254 certification support (aerospace).
- Cost-effective ("fraction of competitors"), easy UX, 14-day trial.
- AI-powered requirements quality and test-case generation.

**Target market:** Aerospace/defense, medical, automotive, safety-critical.

**UI/UX style:** Document-style authoring (atomic requirements + document context),
traceability dashboard with coverage percentages, matrix views.

**Pricing:** Subscription; on-premise option.

### 3.7 ReqSuite RM (Osseno)

**Core functionality**
- Requirements management with structured input forms, categories, metadata, search/filter,
  personalized views.
- Change management: baselines, automatic versioning, version comparison, change
  notifications.
- Traceability: typified links, graphical dependency diagrams, automatic impact analysis,
  consistency checks.
- Intelligent assistance: automatic quality checks, link/term suggestions, requirement and
  test-case derivation, translation (20+ languages), reuse libraries, branching.
- ReqIF/Word/Excel import-export, REST API, integrations (Jira, Azure DevOps, GitLab,
  ClickUp, TestRail, Enterprise Architect, Redmine), identity management.

**Selling points / differentiators**
- Smart/intelligent assistance and reuse; intuitive usability.
- Fast implementation; flexible visual customization.
- ISO 26262, DO-178C, ISO 62304, ISO 13485 compliance.

**Target market:** SMEs, automotive, medical, public sector, utilities.

**UI/UX style:** Web-based. Structured forms, dependency diagrams, matrix, personalized views.

**Pricing:** One-off purchase + maintenance or subscription; named and floating licenses;
read-only licenses free.

### 3.8 Innoslate (SPEC Innovations)

**Core functionality**
- Requirements capture, analysis, and management with a relational model.
- **Traceability Matrix** (X/Y axes, relationship toggling, search, export to XLSX/CSV,
  Traceability Assist and Suspect Assist using ML).
- **Quality Check** (NLP-based, aligned with INCOSE rules), baselines with color-coded
  indicators.
- Tree and Spider diagrams for traceability visualization.
- MBSE (SysML, LML, DoDAF, UAF), Test Center, simulation, project management (Kanban, Gantt).
- AI: quality check, translate, summarize, requirements generation, image generation.

**Selling points / differentiators**
- Cloud-native, AI-powered, real-time collaboration.
- Connected engineering (requirements linked to architecture, models, risks, tests).
- Traceability/Suspect Assist machine-learning suggestions.

**Target market:** Systems engineering, aerospace, defense, general.

**UI/UX style:** Web browser. Documents View, Database View, Traceability Matrix, Tree/Spider
diagrams, Charts Dashboard.

**Pricing:** Subscription.

### 3.9 Capella / Eclipse (Arcadia)

**Core functionality**
- Model-based systems engineering (MBSE) with the Arcadia method.
- Architecture diagrams (operational, capabilities, dataflows, architecture, trees, sequence,
  modes/states, classes/interfaces).
- Requirements viewpoint: ReqIF import, traceability links between requirements and model
  elements, mass editing/visualization tables.
- Model validation (integrity, design, completeness, traceability), semantic browser,
  computed links, replicable elements/libraries, multi-viewpoint.

**Selling points / differentiators**
- Open source (EPL), free, extensible.
- Field-proven Arcadia method; strong aerospace/defense adoption.
- Ecosystem of open-source and commercial add-ons (Publication for Capella, Reqtify, etc.).

**Target market:** Aerospace, defense, energy, transportation (MBSE).

**UI/UX style:** Eclipse-based desktop. Diagrams, semantic browser, project explorer,
mass-editing tables.

**Pricing:** Open source (free); commercial add-ons.

### 3.10 Cameo Systems Modeler / MagicDraw (Dassault Systèmes / No Magic)

**Core functionality**
- SysML modeling with requirement diagrams and requirement tables.
- Traceability relations (Specification/Realization), Dependency Matrix, Relation Map.
- ReqIF import/export and synchronization (Cameo DataHub with DOORS).
- Validation suites (find uncovered requirements), suspect links, metric tables.
- Publish: Word requirements reports, web portal for stakeholders.

**Selling points / differentiators**
- Full SysML standard support; MBSE-centric.
- ReqIF interchange with major RM tools.
- Predefined traceability suites and coverage analysis templates.

**Target market:** Systems engineering, aerospace, defense.

**UI/UX style:** Desktop (MagicDraw). Model Browser, Requirement Diagram, Requirement Table,
matrices, Specification window.

**Pricing:** Commercial per-user.

### 3.11 Enterprise Architect (Sparx Systems)

**Core functionality**
- Requirements modeling as elements with properties (status, priority, difficulty, type).
- **Specification Manager** (spreadsheet-like authoring), **Relationship Matrix**,
  **Traceability window**, baselines, auditing, dashboard diagrams.
- Requirements diagrams, use cases, scenario builder, business rules, glossary.
- Import/export (CSV, XMI, DOORS via MDG Link), SysML/UML support.

**Selling points / differentiators**
- Comprehensive, low-cost modeling platform.
- Flexible and configurable; supports any process/standard.
- MDG Link for DOORS for bidirectional DOORS interchange.

**Target market:** General software, systems, business analysis.

**UI/UX style:** Desktop. Project Browser, Specification Manager (spreadsheet), diagrams,
Relationship Matrix, Traceability window.

**Pricing:** Per-user license (one-time), multiple editions.

### 3.12 Open-source tools

**ReqIF Studio** — free ReqIF requirements editor; components for traceability matrix
(Deduct), ReqIF validation (Consequent), rich-text editing (Extend). Based on Eclipse RMF.

**ProR / Eclipse Requirements Modeling Framework (RMF)** — open-source reference
implementation of ReqIF; GUI for authoring/editing ReqIF files; extensible.

**OpenReq** — open-source requirements-engineering platform (OpenReq Live) with
recommendation/decision support, requirements intelligence, dependency management, and
plugins for Eclipse/Jira/GitHub/Redmine.

**Redmine-based tools** — issue trackers extended with requirements plugins.

**Selling points:** Free, open standards (ReqIF), extensible, community-driven.
**Target market:** Research, SMEs, cost-sensitive teams.
**UI style:** Desktop (Eclipse) or web.
**Pricing:** Open source (free).

---

## 4. UI/UX patterns to adopt

Across all leading tools, the following UI patterns recur. TraceLink should adopt them.

1. **Left navigation / project tree** — a hierarchical tree of projects, modules/folders, and
   requirement documents (DOORS, Jama, Polarion, Codebeamer, EA, Capella, Cameo).
2. **Table / spreadsheet requirement view** — requirements as rows with in-line editable
   attributes, filters, and saved views (DOORS modules, Jama List View, Polarion work items,
   EA Specification Manager, Capella mass-editing).
3. **Traceability matrix** — X/Y grid with relationship markers, search, and export to
   Excel/CSV (Jama, Polarion, Codebeamer, Innoslate, EA Relationship Matrix, ReqIF Deduct).
4. **Trace graph / diagram view** — node-and-line visualization of upstream/downstream
   relationships with expand/collapse and layout options (DOORS Links Explorer, Jama Live
   Trace Explorer, Perforce diagram, Innoslate Tree/Spider, Cameo Relation Map).
5. **Impact analysis view** — forward/backward impact with suspect flags and expandable
   indirect impacts (DOORS, Jama, Polarion, Codebeamer, Perforce, Visure, ReqSuite).
6. **Coverage dashboard** — percentage of requirements covered by tests/verification, with
   gap highlighting (DOORS metrics, Jama Coverage Explorer, Polarion coverage reports,
   Visure traceability dashboard, Cameo coverage analysis).
7. **Suspect-link indicators** — visual markers (icons/colors) flagging artifacts affected by
   a change, with a review/clear workflow (DOORS, Jama, Polarion, Codebeamer, Cameo,
   Innoslate Suspect Assist).
8. **Baseline / version comparison** — snapshot and diff views with rollback (DOORS, Jama,
   Polarion, EA, ReqSuite, Innoslate).
9. **Right-side detail / properties panel** — selected item's attributes, links, and
   traceability in a slide-out panel (DOORS, Jama, Perforce, Cameo).
10. **Dashboards / charts** — status, priority, and coverage charts (DOORS, Polarion, EA,
    Innoslate Charts Dashboard).
11. **Document-style authoring** — author requirements in a document context while keeping
    atomic traceability (Polarion LiveDocs, Visure, Innoslate Documents View).
12. **AI assistance** — quality scoring, suspect/duplicate detection, and requirement
    suggestions surfaced inline (DOORS Watson, Jama, Visure, ReqSuite, Innoslate).

---

## 5. Recommendations for TraceLink

To remain competitive, TraceLink should prioritize the following (roughly in order of impact):

1. **Suspect-link / impact-analysis workflow.** This is the single most common "killer
   feature" across all competitors. When a requirement changes, automatically flag downstream
   artifacts as suspect and provide a review/clear workflow. TraceLink already has impact
   analysis; add automatic suspect propagation and a review queue.

2. **Coverage dashboard with gap highlighting.** Add a live coverage view showing the
   percentage of requirements covered by verification/test entities, with red/green gap
   indicators and export to Excel/CSV. This is a universal audit aid.

3. **Web/API-first presentation.** The market is converging on browser-based collaboration.
   TraceLink's Qt desktop UI is a differentiator for offline/embedded work, but a
   browser-based read/review layer (or a richer web UI) over the existing REST API would
   broaden adoption and enable stakeholder review without installation.

4. **AI-assisted quality and duplicate detection.** Competitors (DOORS, Jama, Visure,
   ReqSuite, Innoslate) all ship AI quality scoring and suspect/duplicate detection.
   TraceLink already has duplicate/similarity detection (WP-E); surface it as an inline
   "quality score" and "suspect" assistant in the UI.

5. **Traceability matrix as a first-class, interactive view.** Ensure the matrix view supports
   search, relationship toggling, saved views, and Excel/CSV export (matching Jama, Polarion,
   Innoslate, EA). TraceLink has a matrix view; add interactivity and export.

6. **Baseline comparison with visual diff and rollback.** Strengthen the baseline/versioning
   story with a compare view and per-item rollback (matching DOORS, EA, ReqSuite).

7. **ReqIF as a headline feature.** TraceLink already supports ReqIF/CSV/HTML import-export.
   Market this explicitly — ReqIF is the de-facto interchange standard and a strong
   differentiator against tools that lack it.

8. **Compliance templates and checklists.** Add OOTB templates/checklists for ARP4754A,
   ARP4761, DO-178C, DO-254 (TraceLink already has compliance rules) and present them as
   guided workflows with automated evidence packages (TraceLink already exports DO-178C
   evidence bundles — surface this as a selling point).

9. **Left-nav project tree + right-side detail panel.** Adopt the standard navigation pattern
   (hierarchical tree + slide-out properties/traceability panel) to reduce learning curve and
   match user expectations.

10. **Dashboards/charts.** Add status, priority, and coverage charts to give managers an
    at-a-glance view (matching DOORS, Polarion, EA, Innoslate).

---

## 6. Screenshots collected

All screenshots are in `docs/research/screenshots/`.

| File | Product | What it shows |
|---|---|---|
| `polarion-core-functionality.png` | Siemens Polarion ALM | Core ALM functionality UI (work items / traceability) |
| `polarion-requirements-mgmt.png` | Siemens Polarion ALM | Requirements management (LiveDocs) view |
| `polarion-change-config.png` | Siemens Polarion ALM | Change & configuration management view |
| `polarion-test-quality.png` | Siemens Polarion ALM | Test & quality management view |
| `polarion-audits-metrics.png` | Siemens Polarion ALM | Audits, metrics & reports view |
| `polarion-issue-risk.png` | Siemens Polarion ALM | Issue & risk management view |
| `ibm-doors-next.png` | IBM DOORS Next | Product/marketing page (requirements management) |
| `jama-connect.png` | Jama Connect | Product/marketing page |
| `ptc-codebeamer.png` | PTC Codebeamer | Product/marketing page |
| `perforce-helix-alm.png` | Perforce Helix ALM | Product/marketing page |
| `visure-requirements.png` | Visure Requirements | Product/marketing page |
| `reqsuite-rm.png` | ReqSuite RM | Product/marketing page |
| `innoslate.png` | Innoslate | Requirements management product page |
| `capella-mbse.png` | Eclipse Capella | MBSE tool product page |
| `cameo-systems-modeler.png` | Cameo Systems Modeler | Product/marketing page |
| `enterprise-architect.png` | Sparx Enterprise Architect | Requirements management page |

> Note: The Polarion images are official product UI screenshots (1280x720). The remaining
> images are captures of official product/marketing pages (1280x900) because direct UI
> screenshot URLs were not publicly available for those products. For deeper UI reference,
> the product documentation pages cited in this report contain additional UI diagrams.

---

## 7. Sources

- IBM DOORS Next documentation and product page (ibm.com, jazz.net, sodiuswillert.com)
- Jama Connect help/support (jamasoftware.com, help.jamasoftware.com)
- Siemens Polarion product pages and feature matrix (siemens.com, plm.automation.siemens.com)
- PTC Codebeamer product and support pages (ptc.com, support.ptc.com)
- Perforce Helix ALM help (help.perforce.com)
- Visure Solutions product pages (visuresolutions.com)
- ReqSuite RM product pages (reqsuite.io)
- Innoslate / SPEC Innovations help and product pages (specinnovations.com, help.specinnovations.com)
- Eclipse Capella (mbse-capella.org, iexcelarc.com)
- Cameo Systems Modeler / No Magic docs (docs.nomagic.com)
- Sparx Enterprise Architect (sparxsystems.com, sparxsystems.org)
- ReqIF.academy, Eclipse RMF/ProR, OpenReq (reqif.academy, eclipse.dev/rmf, openreq.eu, github.com)
