# TraceLink — Commercial-Grade Systems Engineering Platform (Plan)

> Goal: turn the current TraceLink prototype (4 flat entity tables + one link
> table, create/query only) into a commercial-grade traceability, impact,
> coverage, and certification-evidence tool. This is Phase 7 of Lodestar.
>
> RiskAI is intentionally last and is NOT in this plan.

---

## 0. Current state (the gap)

| Capability | Today | Commercial grade needs |
|---|---|---|
| Entity model | Requirement, DesignItem, Interface, TestCase (flat, few fields) | Rich typed entities with lifecycle, attributes, hierarchy |
| Links | One table, `traces_to` relation only | Typed relations, metadata, validation |
| Operations | create, findAll, linksFrom, linksTo | CRUD, query/filter, search, update, delete |
| Graph | Direct neighbors only | Transitive closure, impact, coverage, matrix |
| Compliance | none | Rules engine, DO-178C/ARP4754A validation, reports |
| Versioning | none | Baselines, audit history, diff, change impact |
| Import/Export | none | ReqIF, CSV, Excel, DOORS-compatible export |
| API | one `/scenario` endpoint exists; no tracelink | Full REST resource for TraceLink |
| UI | stub | Matrix view, graph view, coverage dashboard |
| Tests | smoke path only | Unit + integration tests per feature |

---

## 1. Design principles (commercial grade)

1. **One source of truth in SQLite.** TraceLink owns its tables and schema via
   append-only migrations. No other module writes directly.
2. **Typed, directed graph.** All traceability is a typed directed edge between
   typed entities. Relations are explicit, not free text.
3. **Bidirectional by construction.** Every operation supports both directions.
   Coverage and impact are computed by graph traversal, not by ad-hoc queries.
4. **Append-only audit.** Every mutation is recorded. Audit is non-destructive.
   This is the basis for certification evidence and change management.
5. **Integrity on write.** Dangling links, duplicates, and self-loops are
   rejected at the service layer, not discovered later.
6. **Compliance as rules, not code.** Validation rules are data. Users define
   policy (e.g. "every requirement must be verified"). The engine evaluates it.
7. **Everything exposed over REST.** The Qt UI, Python layer, and local LLM all
   reach TraceLink through the thin C++ REST API. No direct DB access outside core.
8. **Versioning without data loss.** Baselines are snapshots; history is
   retained. Diff shows what changed between any two snapshots.
9. **Export = certification evidence.** Matrix, coverage, and validation reports
   export to formats an auditor accepts (CSV, ReqIF, HTML).

---

## 2. Target domain model

### 2.1 Entity types (unified graph node)

All entities share a common identity so the graph engine treats them uniformly.
Each type keeps its own attributes.

| Type | External id pattern | Key attributes (beyond id/name/text) |
|---|---|---|
| `requirement` | REQ-xxx | type, status, priority, source, owner, rationale, verification_method, safety_level, parent_id, sort_order, tags, version |
| `design` | DES-xxx | type (subsystem/component/module), parent_id, owner, tags, version |
| `interface` | IF-xxx | direction, source_entity, target_entity, data_items, protocol, tags |
| `test_case` | TC-xxx | verification_method, result_status, priority, tags, version |
| `hazard` | HAZ-xxx | severity, likelihood, status (shared with RiskAI later; stub now) |
| `decision` | DEC-xxx | decision log / rationale, owner, date (Requirements Decision Log) |
| `assumption` | ASS-xxx | statement, owner, status |

### 2.2 Entity attributes (all nodes)

- `id` (UUID, internal)
- `external_id` (human id, e.g. REQ-100) — unique per type
- `type` (entity kind)
- `name`, `text` (full body)
- `status` (lifecycle state, see 2.3)
- `version` (mutable integer, bumped on update)
- `created_by`, `created_at`, `updated_by`, `updated_at`
- `baseline` (the baseline id it was last released under, nullable)
- `parent_id` (hierarchy within type)

### 2.3 Lifecycle states

Status is a state machine with allowed transitions:

- Requirement: `Draft → Proposed → Approved → Validated → Implemented → Verified → Obsolete`
- Design: `Draft → Reviewed → Released → Obsolete`
- Test case: `Draft → Ready → Executed → Passed → Failed → Obsolete`
- Interface: `Draft → Agreed → Released → Changed → Obsolete`

The service rejects illegal transitions (e.g. `Draft → Verified`).

### 2.4 Relationship types (typed edges)

Enums used for `relation` on links:

| Relation | Meaning | Typical source → target |
|---|---|---|
| `satisfies` | source meets the target | design → requirement |
| `verifies` | source proves target | test_case → requirement |
| `derives` | source comes from target | requirement → higher requirement |
| `allocates` | source is assigned to target | requirement → design |
| `refines` | source is a detail of target | requirement → requirement |
| `decomposes` | source splits into target | design → design |
| `depends_on` | source needs target | design → interface |
| `traces_to` | generic trace (legacy) | any → any |
| `validates` | source checks target | test_case → design |
| `conflicts` | source conflicts with target | any → any |

The engine has a **canonical reverse mapping** (e.g. `verifies` reversed is
`is_verified_by`) so traversal works both directions.

### 2.5 Trace link attributes

- `id` (UUID)
- `source_type`, `source_id`, `target_type`, `target_id`
- `relation`
- `rationale` (why the link exists)
- `status` (Active / Superseded / Proposed)
- `created_by`, `created_at`, `updated_at`
- `version`
- `superseded_by` (link id, for link history)
- `valid_from` (baseline), `valid_to` (baseline, null = still valid)

---

## 3. Schema (migration plan, append-only)

Each capability adds a new migration file. Existing files are never edited.

### 3.1 `003_tracelink_entities.sql`
- `ALTER TABLE requirements` add: external_id, type, priority, source, owner,
  rationale, verification_method, safety_level, parent_id, sort_order, tags,
  version, created_by, created_at, updated_by, updated_at.
- `ALTER TABLE design_items` add: external_id, type, owner, parent_id, tags,
  version, created_by, created_at, updated_by, updated_at.
- `ALTER TABLE interfaces` add: external_id, direction, source_entity,
  target_entity, data_items, protocol, tags, version, created_by, created_at,
  updated_by, updated_at.
- `ALTER TABLE test_cases` add: external_id, verification_method, result_status,
  priority, tags, version, created_by, created_at, updated_by, updated_at.
- New tables: `hazards`, `decisions`, `assumptions`.
- Add indexes on `external_id`, `parent_id`, `status`, `tags`.

### 3.2 `004_tracelink_links.sql`
- `ALTER TABLE trace_links` add: rationale, status, created_by, created_at,
  updated_at, version, superseded_by, valid_from, valid_to.
- Add indexes on `(source_type, source_id)`, `(target_type, target_id)`,
  `(relation)`.
- Add a `link_validation` view (dangling / duplicate detection query).

### 3.3 `005_tracelink_audit.sql`
- `audit_log`: id, entity_type, entity_id, action, field, old_value, new_value,
  actor, timestamp, change_request_id.
- Index on `(entity_type, entity_id)`, `(timestamp)`.

### 3.4 `006_tracelink_baseline.sql`
- `baselines`: id, name, description, created_by, created_at.
- `baseline_entities`: baseline_id, entity_type, entity_id, version, snapshot
  (JSON of full entity at release time).
- `baseline_links`: baseline_id, link_id, snapshot (JSON of link at release time).
- Unique on `(baseline_id, entity_type, entity_id)`.

### 3.5 `007_tracelink_rules.sql`
- `compliance_rules`: id, name, description, rule_type, params (JSON),
  severity, standard, enabled.
- `compliance_violations`: id, run_id, rule_id, entity_type, entity_id,
  message, severity, timestamp.
- `validation_runs`: id, name, started_at, finished_at, status, summary.

### 3.6 `008_tracelink_import_export.sql`
- `import_batches`: id, format, filename, imported_by, imported_at, status,
  result_summary.
- `import_log`: id, batch_id, line, severity, message (non-destructive import log).

---

## 4. Domain / service API (`core/tracelink`)

### 4.1 Entity service
- `addEntity(type, data)` → assigns UUID + external_id, validates required fields.
- `updateEntity(type, id, data)` → bumps version, writes audit, checks status transition.
- `removeEntity(type, id)` → soft delete (marks Obsolete + breaks/records links), never hard delete.
- `getEntity(type, id)` → returns entity.
- `listEntities(type, filter)` → paged list with filters (status, tags, text search).
- `search(text)` → full-text search across name + text.

### 4.2 Link service
- `addLink(srcType, srcId, tgtType, tgtId, relation, rationale)` →
  validates nodes exist, relation allowed for the pair, no duplicate, no self-loop.
- `updateLink(id, rationale/status)` → writes audit, supersedes old if needed.
- `removeLink(id)` → marks Superseded (kept for history).
- `linksFrom(type, id)`, `linksTo(type, id)` → direct neighbors.

### 4.3 Graph engine (new: `GraphEngine` / `TraceGraph` expanded)
- `upstreamClosure(type, id, depth)` — all nodes that influence the target (BFS/DFS over reversed links).
- `downstreamClosure(type, id, depth)` — all nodes affected by a change to the source.
- `impactAnalysis(type, id)` — returns { affected entities, affected links, blocked transitions, downstream test cases }.
- `coverage()` — for each requirement: which design items satisfy it, which test cases verify it; compute `% verified`, `% designed`, `% satisfied`.
- `coverageGap()` — requirements with zero design or zero test coverage.
- `traceMatrix()` — rows = requirements, columns = design/test; cell = relation present.
- `graphQuery(type, id, relation, direction, depth)` — general traversal.

### 4.4 Compliance engine (new: `RulesEngine`)
- `defineRule(...)`, `listRules()`, `enableRule(id)`, `disableRule(id)`.
- `runValidation()` → evaluates all enabled rules, writes violations, returns report.
- Built-in rule templates:
  - `REQ_MUST_BE_VERIFIED` — every active requirement needs ≥1 `verifies`.
  - `REQ_MUST_BE_SATISFIED` — every active requirement needs ≥1 `satisfies`.
  - `NO_DANGLING_LINKS` — no link to a deleted/nonexistent entity.
  - `NO_DUPLICATE_LINKS` — no identical (src, tgt, relation) pairs.
  - `NO_SELF_LINKS` — no link where src == tgt.
  - `BIDIRECTIONAL` — link present requires its reverse relation present (optional).
  - `COVERAGE_MIN` — requirement coverage ≥ N% (param).
  - `NO_ORPHAN_DESIGN` — every design item traces to a requirement.
  - `STATUS_VALID` — every entity status is in a legal state.
- Rules map to assurance standards: ARP4754A, ARP4761, DO-178C, DO-254 (tag each rule with its standard).

### 4.5 Versioning & change (new: `BaselineService`)
- `createBaseline(name, desc)` → snapshot all active entities + links.
- `listBaselines()`, `getBaseline(id)`.
- `diffBaseline(a, b)` → added / removed / modified (field-level) entities and links.
- `history(type, id)` → ordered audit trail for one entity.
- `entityAtBaseline(type, id, baselineId)` → reconstruct entity as it was.
- `changeImpact(type, id, changeRequestId)` → audit entries tagged to a change, plus affected downstream entities.

### 4.6 Import / Export (new: `IoService`)
- Export: `matrixCsv()`, `matrixHtml()` (auditor-ready), `entitiesCsv()`, `reqif()`.
- Import: `importCsv(file)` (non-destructive; dry-run option), `importReqif(file)`.
- Every import writes to `import_batches` + `import_log`; partial failures reported, no partial DB corruption.

---

## 5. REST API (extend `core/api`)

New routes under `/tracelink`:

| Method | Route | Purpose |
|---|---|---|
| GET | `/tracelink/entities?type=&filter=` | list / filter / search |
| POST | `/tracelink/entities` | add entity |
| PUT | `/tracelink/entities/{type}/{id}` | update entity |
| GET | `/tracelink/entities/{type}/{id}` | get entity |
| GET | `/tracelink/entities/{type}/{id}/history` | audit history |
| DELETE | `/tracelink/entities/{type}/{id}` | soft delete |
| POST | `/tracelink/links` | add link |
| PUT | `/tracelink/links/{id}` | update link |
| DELETE | `/tracelink/links/{id}` | soft delete link |
| GET | `/tracelink/links?sourceType=&sourceId=` | links from/to node |
| GET | `/tracelink/impact/{type}/{id}` | impact analysis |
| GET | `/tracelink/coverage` | coverage summary + gaps |
| GET | `/tracelink/matrix` | trace matrix (JSON/CSV) |
| POST | `/tracelink/validate` | run rules engine |
| GET | `/tracelink/rules` | list rules |
| POST | `/tracelink/baselines` | create baseline |
| GET | `/tracelink/baselines` | list baselines |
| GET | `/tracelink/baselines/{a}/diff?against={b}` | diff two baselines |
| POST | `/tracelink/import/{format}` | CSV / ReqIF import |
| GET | `/tracelink/export/{format}` | CSV / ReqIF / HTML export |

All use the existing error model (200 / 400 / 404 / 500 with
`{"error":{"code","message"}}`).

---

## 6. Qt UI (later phase, `ui/`)

The TraceLink dashboard needs four main views:

1. **Matrix view** — grid of requirement rows vs. design/test columns, colored
   cells by relation. Click a cell to jump to the link. This is the classic
   traceability matrix.
2. **Graph view** — interactive node-link diagram (QGraphicsScene). Select a
   node to highlight upstream/downstream; filter by relation type.
3. **Impact view** — enter a changed entity, show affected tree with severity,
   including blocked status transitions and downstream test cases.
4. **Coverage & compliance dashboard** — % verified / designed per requirement,
   rule violation list, export buttons for reports.

UI runs in-process and calls the C++ service API directly (per architecture).
This phase also wires TestForge runs so coverage reflects real executed results.

---

## 7. Cross-cutting: correctness & commercial hardening

### 7.1 Integrity on write
- Validate node existence before adding a link (no dangling).
- Reject self-loops and exact duplicate links.
- Enforce allowed relation type per entity pair.
- Enforce status transition legality.

### 7.2 Transactions
- Each mutation that touches multiple rows (e.g. add entity + audit + link)
  runs in one SQLite transaction. Rollback on any failure.

### 7.3 Concurrency
- SQLite is single-writer; the core is single-process. Use `BEGIN IMMEDIATE`
  transactions for mutations so concurrent readers never see partial state.
- WAL mode on for concurrent read/write (enable in `Database::open`).

### 7.4 Auditing
- Every create/update/delete/link mutation writes one or more `audit_log` rows
  in the same transaction as the change. Never lose an audit entry.

### 7.5 Testing
- Adopt a lightweight test framework (e.g. Catch2) for TraceLink in addition to
  the existing smoke path.
- Unit tests: relation mapping, closure, coverage math, rules engine, diff.
- Integration tests: in-memory SQLite DB, full scenario (import → trace →
  baseline → diff → validate → export).
- Each phase (3.1–3.6) ships with tests before it is marked done.

---

## 8. Work packages (implementation order)

Each work package is independently shippable and has an acceptance check.

### WP-1 — Domain model + schema (migrations 003, 004) [foundation]
- New `TraceGraph` types (rich entities + typed links + metadata).
- DAO expansion: update, delete(soft), findById, findByFilters, search.
- Status state machines + transition validation.
- Integrity checks on write (dangling / duplicate / self-loop / relation-type).
- Tests: DAO CRUD, transition rules, integrity.
- **Accept:** smoke adds a typed requirement with full attributes, links it with
  `verifies`, and rejects a duplicate link.

### WP-2 — Graph engine (closure, impact, coverage, matrix)
- `upstreamClosure` / `downstreamClosure`.
- `impactAnalysis`.
- `coverage` / `coverageGap` / `traceMatrix`.
- Reverse-relation mapping table.
- Tests: closure correctness, coverage math, impact tree.
- **Accept:** a 3-level graph (system req → derived req → design → test) yields
  correct coverage and a correct impact set when a leaf changes.

### WP-3 — Rules engine + validation
- Rule definition + evaluation engine.
- Built-in rule templates (see 4.4).
- `validation_runs` + `compliance_violations` storage.
- Standard tagging (ARP4754A, DO-178C, ...).
- Tests: each rule on crafted data.
- **Accept:** a graph with an unverified requirement triggers `REQ_MUST_BE_VERIFIED` and reports it in a run.

### WP-4 — Audit, baselines, diff, change impact (migrations 005, 006)
- `audit_log` writes on every mutation.
- `BaselineService` snapshot + list + get.
- `diffBaseline(a, b)` field-level.
- `entityAtBaseline`, `history`.
- Tests: audit completeness, diff correctness, restore-at-baseline.
- **Accept:** modify an entity, create two baselines, diff shows the exact field change; history lists every action.

### WP-5 — Import / Export (migration 008)
- CSV export (matrix + entities) and HTML report.
- ReqIF import + export.
- Non-destructive import with batch + log.
- Tests: round-trip export→import→export stable; partial import logged.
- **Accept:** export a 5-requirement graph to CSV and ReqIF; import both back and the graph matches.

### WP-6 — REST API exposure
- All `/tracelink` routes (Section 5).
- Extend `ApiServer`; smoke test each endpoint.
- **Accept:** smoke passes POST entity → link → GET coverage → POST validate → POST baseline → GET diff.

### WP-7 — Qt UI views (matrix, graph, impact, coverage)
- Matrix view + export buttons.
- Graph view (QGraphicsScene).
- Impact view.
- Coverage / compliance dashboard.
- **Accept:** UI shows a loaded graph in all four views and can export the matrix.

### WP-8 — Commercial hardening
- WAL mode, `BEGIN IMMEDIATE` transactions.
- Performance indexes; query profiling on 10k+ nodes.
- Documentation (user + admin) and schema diagrams.
- **Accept:** 10,000 entities + 50,000 links load, traverse, and validate within a target time budget.

---

## 9. Suggested sequence & quick wins

1. **WP-1 first** — everything depends on the rich model. It is the highest-risk
   foundation.
2. **WP-2 next** — closure/impact/coverage is the differentiator vs. plain
   requirement tools. Delivers visible value early.
3. **WP-3** — compliance rules turn data into certification evidence; the
   assurance story is the commercial hook.
4. **WP-4** — versioning is what auditors and change boards require.
5. **WP-5/6** — import/export + API make it usable and integrable (Python/LLM/UI).
6. **WP-7/8** — UI and hardening last.

Dependencies: WP-1 → WP-2 → WP-3 → WP-4 → WP-5/6 → WP-7/8. WP-4 can start in
parallel with WP-3 for the audit table. WP-6 needs WP-1..WP-4.

---

## 10. Out of scope (this plan)

- **RiskAI** — deferred to the final phase by request.
- Actual vendor RF integration (that is adapter/ScenarioForge territory).
- Full-text search engine beyond SQLite FTS (use SQLite FTS5 if needed).
- Multi-user server concurrency across processes (core is single-process; if
  multi-user is needed later, it is a separate architecture decision).
