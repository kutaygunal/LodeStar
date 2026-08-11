# Phase 3 Task — Phase 1 Vertical Slice

**Agent:** senior-engineer-3
**Project:** C:/Users/kutay/Desktop/Projects/Lodestar
**Deliverable:** working C++ code for the first three core modules, self-verified by build.

## Context

Read `docs/architecture.md` (Phase 1) and the existing monorepo scaffold (Phase 2). This
phase builds the first vertical slice: core/common, core/persistence, core/tracelink.

## What to produce

Working C++ code for the first three core modules:

- **core/common** — logging, configuration, common utilities used by the other modules.
- **core/persistence** — SQLite schema (tables for requirements, tests, trace links,
  scenarios) + DAO layer; schema creation/migration on startup.
- **core/tracelink** — graph model linking requirements, design, interfaces, and test
  cases; basic add/query operations backed by persistence.

## Acceptance criteria

- The three modules compile and link into the core target.
- A small self-verifying smoke path (e.g. a main/CLI that opens the DB, creates the
  schema, and inserts/queries a trace link) runs successfully.
- No test agents — verification is by build + smoke run.

## Working rules

- Follow `docs/working-rules.md`: no full-filesystem scans, hard timeouts, no commits
  (devops commits).
- Self-verify: build with a HARD TIMEOUT and run the smoke path with a HARD TIMEOUT.
- Reply **DONE** when complete, or a concise error on failure.
