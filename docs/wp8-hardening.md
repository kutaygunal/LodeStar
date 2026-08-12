# WP-8 Hardening — WAL, Transactions, Indexes, Performance

**Applies to:** Lodestar C++ core (database, service layer)
**Scope:** Commercial-grade durability, atomicity, and performance for the
tracelink persistence and graph engine.
**Reference:** `docs/tracelink-plan.md` WP-8 / sections 7.2, 7.3 and 8.

This document is the user and administrator guide for the WP-8 hardening
work. It describes the four changes shipped: WAL journaling, `BEGIN IMMEDIATE`
transactions, performance indexes, and the 10k-node performance test.

---

## 1. WAL journaling (write-ahead log)

`Database::open` now enables SQLite write-ahead log mode with
`PRAGMA journal_mode = WAL`. Together with `PRAGMA foreign_keys = ON`, every
database opened by the core runs in WAL mode.

### What WAL gives you

* **Durability without sacrificing reads.** Readers do not block writers and
  writers do not block readers, so a busy import does not stall the graph
  queries.
* **Faster commits.** Multiple writes are grouped into a single `-wal` file
  and checkpointed, reducing fsync cost versus the default rollback journal.
* **Crash safety.** A transaction is either fully applied or fully discarded,
  and the WAL is replayed/recovered automatically on the next open.

### Admin notes

* Three files exist per database while it is open: `name.db`, `name.db-wal`,
  and `name.db-shm`. The `-wal` and `-shm` files are transient; it is safe to
  delete them only when the database is closed and no process holds it.
* Verify mode from a shell (or the test harness) with:
  `PRAGMA journal_mode;`  → returns `wal`.
* Backup with `sqlite3 name.db ".backup out.db"` while WAL is active; do not
  copy the `-wal`/`-shm` files by hand.

---

## 2. `BEGIN IMMEDIATE` transactions

Every multi-row mutation is wrapped in a single **`BEGIN IMMEDIATE`**
transaction and rolled back as a unit if any step fails. This guarantees there
is never a partial write (for example, an entity created but its audit row
missing, or a batch half-imported).

`BEGIN IMMEDIATE` acquires the write lock immediately at `BEGIN` time instead
of waiting for the first write. This avoids the "write lock upgrade"
deadlock a deferred `BEGIN` can cause when two transactions both start by
reading and then try to write.

### Covered operations

The following now run in one `BEGIN IMMEDIATE ... COMMIT` block with
rollback-on-failure:

* `addEntity` — entity row + its `create` audit row
* `updateEntity` — updated row + one audit row per changed field
* `removeEntity` (soft delete) — status change + audit row
* `addLink` / `updateLink` / `removeLink` — link row + link audit row
* status transition — status change + audit row
* baseline snapshot write (entity + link snapshots)
* rules-engine `runValidation` — validation run + all violations
* import batch (import/export service)

### API

`Database` exposes the transaction helpers directly (used by the service
layer and by the hardening tests):

| Method               | SQL                  | Returns        |
|----------------------|----------------------|----------------|
| `beginImmediate()`   | `BEGIN IMMEDIATE;`    | `Result<void>` |
| `commit()`           | `COMMIT;`             | `Result<void>` |
| `rollback()`         | `ROLLBACK;`           | `Result<void>` |
| `queryScalar(sql)`   | first col of first row| `std::string`  |

On any failure inside a block the service rolls back and returns the error;
no partial write survives.

---

## 3. Performance indexes

Migration `009_tracelink_perf.sql` adds the indexes that support the hot
queries (transitive closure, impact analysis, coverage, and filtering). All
statements are `CREATE INDEX IF NOT EXISTS`, so they are safe to re-run and
coexist with the earlier index set.

### Requirements
| Index                          | Columns            | Serves |
|--------------------------------|--------------------|--------|
| `idx_requirements_external_id` | `(external_id)`    | external-id lookup |
| `idx_requirements_status`      | `(status)`         | status filter |
| `idx_requirements_type`        | `(type)`           | type filter |
| `idx_requirements_type_status` | `(type, status)`   | type+status filter |
| `idx_requirements_type_parent` | `(type, parent_id)`| hierarchy traversal |

### Trace links (closure / impact / coverage)
| Index                     | Columns                    | Serves |
|---------------------------|----------------------------|--------|
| `idx_trace_links_source`  | `(source_type, source_id)` | downstream traversal |
| `idx_trace_links_target`  | `(target_type, target_id)` | upstream traversal |
| `idx_trace_links_relation`| `(relation)`               | relation filter |

### Audit log
| Index               | Columns                    | Serves |
|---------------------|----------------------------|--------|
| `idx_audit_entity`  | `(entity_type, entity_id)` | per-entity audit trail |
| `idx_audit_timestamp`| `(timestamp)`             | time-range audit queries |

> `idx_requirements_external_id`, `idx_requirements_status`,
> `idx_trace_links_source`, `idx_trace_links_target`,
> `idx_trace_links_relation`, `idx_audit_entity`, and `idx_audit_timestamp`
> are the seven index names asserted by the WP-8 hardening tests.

---

## 4. 10k-node performance test

`core/test/wp8_hardening_tests.cpp` (test-first, written by the scrum-master)
validates all four hardening features and the WP-8 acceptance:

1. **WAL mode** — opens a DB and asserts `journal_mode` returns `wal`.
2. **Transactions** — writes a row inside a `BEGIN IMMEDIATE` block, rolls
   back, and asserts the row is gone (no partial write); then commits a row
   and asserts it persists.
3. **Indexes** — asserts all seven required indexes exist in `sqlite_master`.
4. **Perf / acceptance** — loads **10,000 entities + 50,000 links** inside a
   single `BEGIN IMMEDIATE` bulk-load transaction, then verifies the load
   counts, runs `coverage()`, `impactAnalysis()`, and a full rules-engine
   `runValidation()`. The whole load + traverse + validate pipeline must
   complete within a generous finite budget (`< 60 s`).

### Running
```bash
# from the repo root, after configuring with -DLODESTAR_BUILD_TESTS=ON
./build/core/test/lodestar_wp8_tests
```

### Acceptance (plan WP-8)
10,000 entities + 50,000 links load, traverse, and validate within the time
budget.

---

## 5. Operational summary

* **Do not** copy a live database plus its `-wal`/`-shm` files; use
  `sqlite3` backup or `VACUUM INTO` for a consistent snapshot.
* **Migrate** existing databases by running the migration runner once; the
  new indexes are added by `009_tracelink_perf.sql` and existing rows need no
  backfill.
* **Tuning:** on very large graphs the closure budget and WAL `wal_autocheckpoint`
  can be raised; defaults are sufficient for the 10k/50k acceptance load.
