# TraceLink — Competitor Research Task

You are a research agent. Your job is to research products similar to **TraceLink** (the
requirements-traceability / systems-engineering module of the Lodestar GNSS/SBAS platform),
identify their core and selling functionalities, and collect **screenshots of their UIs** from
the web. Produce a written report.

## What TraceLink is (context)

TraceLink is a commercial-grade requirements traceability and systems-engineering tool. It
provides: typed requirements/design/interface/test entities, a typed directed trace graph,
impact analysis, coverage analysis, a traceability matrix, compliance rules (ARP4754A,
ARP4761, DO-178C, DO-254), baselines/versioning, change-request workflow, import/export
(ReqIF/CSV/HTML), a REST API, and a Qt desktop UI with matrix/graph/impact/coverage views.

## Your task

### 1. Find similar products
Research the leading commercial and open-source requirements-traceability / ALM / systems-
engineering tools. At minimum cover these (add more if you find them):
- IBM DOORS Next (DOORS NG)
- Jama Connect
- Siemens Polarion ALM
- PTC Codebeamer
- Perforce Helix ALM
- Visure Requirements
- ReqSuite RM
- Innoslate (SPEC Innovations)
- Capella / Eclipse
- Cameo Systems Modeler / MagicDraw (Dassault)
- Enterprise Architect (Sparx)
- Open-source: OpenReq, ReqIF Studio, Polarion, redmine-based tools

### 2. For each product, capture:
- **Core functionality** — what it does (traceability, impact, coverage, baselines, workflow, etc.)
- **Selling points / differentiators** — why customers choose it (certification support, ease of use, integration, AI, etc.)
- **Target market** — aerospace/defense, automotive, medical, general software
- **UI/UX style** — layout, navigation, key views (matrix, graph, dashboard)
- **Pricing model** (if public) — per-seat, subscription, open-source

### 3. Collect UI screenshots
- Use web search / web fetch to find official product screenshots, marketing pages, and
  documentation images.
- **Download the actual image files** into `docs/research/screenshots/` (create the folder).
  Use a tool like `curl`/`Invoke-WebRequest`/`wget` to save the images. Name them clearly,
  e.g. `jama-connect-matrix.png`, `polarion-traceability.png`.
- Aim for at least **8–12 screenshots** across the different products, focusing on the
  traceability matrix, trace graph, coverage dashboard, and requirements views.
- If a direct image URL is not obvious, use the `website-screenshot` skill or the
  `screenshot_page` tool to capture a product's marketing/demo page.

### 4. Write the report
Write a comprehensive report to **`docs/tracelink-competitor-research.md`** with:
- A comparison table of all products (columns: product, core functionality, selling points,
  target market, UI style, pricing).
- A section per product with the details from step 2.
- A "UI/UX patterns" section summarizing the common UI patterns (matrix view, graph view,
  dashboard, left-nav, etc.) that TraceLink should adopt.
- A "recommendations for TraceLink" section: which features/UI patterns to add next to be
  competitive.
- A list of the screenshots you saved (with file paths) and what each shows.

## Working rules
- Use `web_search` / `web_explore` / `web_fetch` for research. Do NOT run `find /`.
- Save screenshots to `docs/research/screenshots/`.
- When done, write the report and notify the orchestrator:
  `herdr agent prompt orchestrator 'DONE tracelink-research'`
