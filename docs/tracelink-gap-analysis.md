# TraceLink — Gap Analysis vs. Competitors

**Date:** 2026-08-12
**Basis:** `docs/tracelink-competitor-research.md` (12 products) + verified current TraceLink
capabilities (WP-1..WP-8 + v2 WP-A..WP-G, all tests green).

---

## 1. TraceLink current capability inventory (verified)

| Area | What TraceLink has today |
|---|---|
| Domain model | 7 entity types (requirement, design, interface, test_case, hazard, decision, assumption), rich attributes, hierarchy (parent/child) |
| Relations | 10 typed relations + reverse mapping, integrity-on-write (no dangling/self-loop/duplicate, relation-type validation) |
| Lifecycle | Status state machines + legal-transition enforcement |
| Audit | Append-only audit trail (actor, change-request id, field-level) |
| Versioning | Baselines, field-level diff, entity-at-baseline, restore/rollback |
| Change mgmt | Change-request workflow (Open→InReview→Approved/Rejected→Implemented), CRs linked to audit |
| Graph engine | Up/downstream closure, impact analysis, coverage, coverage-by-verification-method, trace matrix |
| Compliance | Rules engine (9 built-in rules), standards tagging (ARP4754A/ARP4761/DO-178C/DO-254), validation runs + violations |
| Search | FTS5 ranked full-text search + pagination |
| Duplicates | Duplicate/similarity detection (threshold-based) |
| Import/Export | CSV (matrix/entities/links), HTML report, ReqIF, DO-178C evidence package |
| API | Full `/tracelink` REST API + API-key auth |
| UI | Qt view models + Qt UI wiring service (views exist, app NOT built) |
| Hardening | WAL, BEGIN IMMEDIATE, indexes, 10k perf (5.5s), DB backup/restore, structured logging, input validation, typed error codes |

---

## 2. Gap matrix

Legend: ✅ = has it · 🟡 = partial · ❌ = missing · Priority H/M/L

| Capability | TraceLink | Competitor benchmark | Gap | Priority |
|---|---|---|---|---|
| **Suspect-link / impact workflow** | 🟡 impact analysis exists, but NO automatic suspect propagation | DOORS, Jama, Polarion, Codebeamer, Perforce, Visure, ReqSuite, Innoslate all auto-flag downstream artifacts on change + review/clear | **Auto-flag downstream as "suspect" on change; add review/clear queue** | **H** |
| **Coverage dashboard (live UI)** | 🟡 coverage + evidence package exist, but no live dashboard view | DOORS metrics, Jama Coverage Explorer, Polarion, Visure, Cameo all show live % covered with gap highlighting | **Live coverage dashboard with red/green gap indicators + export** | **H** |
| **Web / browser presentation** | 🟡 REST API exists, but no web UI | All leading tools are browser-based, collaborative | **Browser read/review layer over the REST API** | **H** |
| **AI quality scoring** | 🟡 duplicate detection exists, but no quality scoring | DOORS (Watson), Jama, Visure, ReqSuite, Innoslate all ship AI quality scores + suggestions | **Inline quality score + suspect/duplicate assistant** | **H** |
| **Interactive traceability matrix** | 🟡 matrix + CSV/HTML export exist, but no saved views / interactivity | Jama, Polarion, Innoslate, EA support search, relationship toggling, saved views | **Interactive matrix: search, filter, saved views, export** | **M** |
| **Baseline visual diff + rollback UI** | 🟡 diff + restore exist, but no visual compare view | DOORS, EA, ReqSuite, Innoslate have compare views + per-item rollback | **Visual baseline compare + per-item rollback UI** | **M** |
| **Left-nav project tree + detail panel** | 🟡 hierarchy tree exists, but no UI | Universal pattern (DOORS, Jama, Polarion, Codebeamer, EA, Capella, Cameo) | **Left-nav tree + right-side properties/traceability panel** | **M** |
| **Dashboards / charts** | ❌ none | DOORS, Polarion, EA, Innoslate have status/priority/coverage charts | **Manager dashboards + charts** | **M** |
| **Document-style authoring** | ❌ none | Polarion LiveDocs, Visure, Innoslate author in document context | **Document-style authoring with atomic traceability** | **M** |
| **General review / comment / vote** | 🟡 change-request review exists, but no general artifact review/comments | DOORS, Jama, Polarion have reviews, comments, votes, approval | **General review + comment + approval on artifacts** | **M** |
| **Compliance templates / checklists** | 🟡 rules exist, but no guided OOTB templates/checklists | Codebeamer, Visure ship OOTB regulatory templates | **Guided ARP4754A/DO-178C templates + checklists** | **M** |
| **ReqIF interchange** | ✅ full ReqIF import/export | De-facto standard; all leading tools support it | **None — already a strength** | - |
| **Test management integration** | 🟡 TestForge exists, but not deeply wired to coverage | Jama, Polarion, Codebeamer integrate test mgmt with coverage | **Wire TestForge runs into live coverage** | **M** |
| **Multi-user / RBAC / permissions** | 🟡 API keys exist, but no user roles/permissions | All commercial tools have roles, permissions, concurrent editing | **User roles + permissions + concurrent editing** | **M** |
| **OSLC integration** | ❌ none | DOORS, Polarion, Codebeamer expose OSLC | **OSLC REST integration** | **L** |
| **Product-line / variants** | ❌ none | Codebeamer, Polarion support variants/branching | **Variants / branching** | **L** |
| **MBSE / SysML modeling** | ❌ none (out of scope for a traceability tool) | Capella, Cameo, EA | **Not recommended — different product** | - |

---

## 3. Priority roadmap (recommended next round)

### Round 1 — the "killer features" (H)
1. **Suspect-link workflow** — when a requirement changes, auto-flag all downstream
   artifacts as `suspect`; add a review/clear queue. This is the #1 differentiator across
   every competitor. TraceLink already has impact analysis + change-request workflow; add
   automatic suspect propagation on top.
2. **Live coverage dashboard** — a view showing % of requirements covered by
   verification/test, with red/green gap highlighting and Excel/CSV export. Universal audit aid.
3. **Web read/review layer** — a browser-based view over the existing REST API so
   stakeholders can review traceability, coverage, and suspect links without installing Qt.

### Round 2 — UX parity (M)
4. **Interactive traceability matrix** — search, filter, saved views, relationship toggling,
   export.
5. **Left-nav project tree + right-side detail panel** — the standard navigation pattern.
6. **Baseline visual compare + per-item rollback** UI.
7. **Dashboards / charts** for managers.
8. **General review + comment + approval** on artifacts (beyond change requests).

### Round 3 — enterprise (M/L)
9. **User roles + permissions + concurrent editing** (multi-user).
10. **Guided compliance templates/checklists** for ARP4754A/DO-178C.
11. **Wire TestForge runs into live coverage**.
12. **OSLC integration** (L).

---

## 4. Where TraceLink is already strong (defend these)

- **ReqIF import/export** — de-facto standard; a headline feature competitors charge for.
- **Compliance rules engine** with ARP4754A/ARP4761/DO-178C/DO-254 tagging + evidence package
  export — matches Visure/Codebeamer certification story.
- **Full audit trail + baselines + change-request workflow** — strong change-management story.
- **Typed graph + integrity-on-write** — robust data model.
- **REST API + API-key auth** — integrable and secure.
- **Performance** — 10k entities + 50k links in 5.5s.

---

## 5. Bottom line

TraceLink's **data/engine layer is already at or above commercial parity** (typed graph,
impact, coverage, compliance rules, ReqIF, audit, baselines, change workflow, REST API). The
**gaps are concentrated in two places**:

1. **The "suspect-link" workflow** — the single most-cited killer feature TraceLink lacks.
2. **The presentation layer** — TraceLink has no live UI (Qt app not built, no web layer),
   while every competitor is browser-based with dashboards, matrices, and detail panels.

**Recommendation:** the next round should focus on (a) suspect-link propagation + review
queue, and (b) a live presentation layer (coverage dashboard + interactive matrix + web
read/review) — these close the biggest competitive gaps with the least new engine work.
