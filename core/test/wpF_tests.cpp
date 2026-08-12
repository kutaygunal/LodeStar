// core/test/wpF_tests.cpp
// ---------------------------------------------------------------------------
// WP-F unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-F engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-F / B2..B8):
//   B2. Typed error codes.
//   B3. Input validation & size limits.
//   B4. DB backup / restore.
//   B5. Migration safety (dry-run / checksum / verify).
//   B6. Fuzz / edge-case tests.
//   B7. Concurrency stress test.
//   B8. Structured logging.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8. Each
// DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-F engineer must provide.
// ---------------------------------------------------------------------------
// (A) Typed error codes (core/common/Result.h):
//
//   namespace lodestar::common {
//   enum class ErrorCode {
//       None = 0, InvalidArgument, NotFound, Duplicate, IntegrityViolation,
//       IllegalTransition, ValidationFailed, DatabaseError, IoError,
//       MigrationError, BackupError, ConcurrencyError, LimitExceeded, Internal
//   };
//   }
//
//   Every Result<T> gains:
//     ErrorCode errorCode() const;   // ErrorCode::None when isOk()
//   and a new factory:
//     static Result err(ErrorCode code, std::string message);
//   The existing err(std::string) factory maps to ErrorCode::Internal.
//   Service-layer failures must carry the correct code (see tests below).
//
// (B) Input validation & size limits (TraceLinkService):
//   - empty/whitespace external_id or name        -> InvalidArgument
//   - name longer than 256 chars                  -> LimitExceeded
//   - text longer than 65536 chars                -> LimitExceeded
//   - unknown status string                       -> InvalidArgument
//   - unknown relation string on a link           -> InvalidArgument
//   - negative limit/offset in a filter           -> InvalidArgument
//
// (C) DB backup / restore (core/persistence/Database.h):
//   common::Result<void> backup(const std::string& destPath);
//   common::Result<void> restore(const std::string& srcPath);
//   Database::open() also sets PRAGMA busy_timeout = 5000 (for B7).
//
// (D) Migration safety (core/persistence/MigrationRunner.h):
//   common::Result<bool> dryRun(const std::string& migrationsDir);
//       // true if there are pending migrations, false if up to date
//   std::string checksum() const;
//       // stable checksum of the applied migration set (non-empty)
//   common::Result<bool> verify(const std::string& migrationsDir);
//       // true if the applied schema matches the migration files
//
// (E) Structured logging (core/common/Logger.h):
//   void structured(LogLevel level, const std::string& event,
//                   const std::string& fieldsJson);
//       // writes one JSON line: {"ts":...,"level":...,"event":...,"fields":{...}}
//   void flush();   // flushes the log file
// ---------------------------------------------------------------------------

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "core/common/Logger.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace p  = lodestar::persistence;
namespace c  = lodestar::common;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

// Opens a fresh throwaway DB for one test, runs migrations, returns true on ok.
bool openFreshDb(p::Database& db, const char* file) {
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
    if (db.open(file).failed()) return false;
    p::MigrationRunner runner(db);
    auto mig = runner.run(g_migrationsDir);
    return mig.isOk();
}

// ---------------------------------------------------------------------------
// Lightweight test harness.
// ---------------------------------------------------------------------------
class Harness {
public:
    explicit Harness(const char* name) : name_(name) {}

    void section(const char* s) { std::printf("\n-- %s --\n", s); }

    void check(bool cond, const char* what) {
        if (cond) {
            std::printf("  [PASS] %s\n", what);
        } else {
            std::printf("  [FAIL] %s\n", what);
            ++failures_;
        }
    }

    int failures() const { return failures_; }
    const char* name() const { return name_; }

private:
    const char* name_;
    int failures_ = 0;
};

tl::Entity makeReq(const std::string& extId, const std::string& name = "",
                   const std::string& body = "") {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Requirement;
    e.name = name.empty() ? extId : name;
    e.text = body.empty() ? "Body of " + extId : body;
    e.status = "Draft";
    return e;
}

// ---------------------------------------------------------------------------
// B2. Typed error codes
// ---------------------------------------------------------------------------
void testErrorCodes(Harness& h) {
    h.section("B2. Typed error codes");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpF_codes.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);

    auto r1 = svc.addEntity(makeReq("REQ-EC1"));
    h.check(r1.isOk(), "add first entity ok");
    const std::string reqId = r1.value().id;

    // Duplicate external id -> Duplicate.
    auto dup = svc.addEntity(makeReq("REQ-EC1"));
    h.check(dup.failed() && dup.errorCode() == c::ErrorCode::Duplicate,
            "duplicate external id -> ErrorCode::Duplicate");

    // Missing entity -> NotFound.
    auto missing = svc.getEntity(tl::EntityType::Requirement, "does-not-exist");
    h.check(missing.isOk() && !missing.value().has_value(),
            "getEntity missing returns empty optional");
    auto missingLink = svc.addLink([&] {
        tl::Link l;
        l.sourceType = tl::EntityType::Requirement;
        l.sourceId = reqId;
        l.targetType = tl::EntityType::Requirement;
        l.targetId = "ghost";
        l.relation = "refines";
        return l;
    }());
    h.check(missingLink.failed() &&
                missingLink.errorCode() == c::ErrorCode::IntegrityViolation,
            "dangling link -> ErrorCode::IntegrityViolation");

    // Illegal transition -> IllegalTransition.
    auto badT = svc.transition(tl::EntityType::Requirement, reqId, "Verified");
    h.check(badT.failed() && badT.errorCode() == c::ErrorCode::IllegalTransition,
            "illegal status transition -> ErrorCode::IllegalTransition");

    // Invalid input -> InvalidArgument.
    tl::Entity bad;
    bad.externalId = "REQ-BAD";
    bad.type = tl::EntityType::Requirement;
    bad.name = "   ";
    bad.text = "x";
    bad.status = "Draft";
    auto invalid = svc.addEntity(bad);
    h.check(invalid.failed() && invalid.errorCode() == c::ErrorCode::InvalidArgument,
            "whitespace-only name -> ErrorCode::InvalidArgument");

    db.close();
    std::remove("lodestar_wpF_codes.db");
    std::remove("lodestar_wpF_codes.db-wal");
    std::remove("lodestar_wpF_codes.db-shm");
}

// ---------------------------------------------------------------------------
// B3. Input validation & size limits
// ---------------------------------------------------------------------------
void testInputValidation(Harness& h) {
    h.section("B3. Input validation & size limits");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpF_input.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);

    // Empty external id -> InvalidArgument.
    tl::Entity noExt;
    noExt.externalId = "";
    noExt.type = tl::EntityType::Requirement;
    noExt.name = "REQ-NOEXT";
    noExt.text = "x";
    noExt.status = "Draft";
    auto e1 = svc.addEntity(noExt);
    h.check(e1.failed() && e1.errorCode() == c::ErrorCode::InvalidArgument,
            "empty external id -> InvalidArgument");

    // Empty name -> InvalidArgument.
    tl::Entity noName;
    noName.externalId = "REQ-NONAME";
    noName.type = tl::EntityType::Requirement;
    noName.name = "";
    noName.text = "x";
    noName.status = "Draft";
    auto e2 = svc.addEntity(noName);
    h.check(e2.failed() && e2.errorCode() == c::ErrorCode::InvalidArgument,
            "empty name -> InvalidArgument");

    // Name longer than 256 chars -> LimitExceeded.
    tl::Entity longName;
    longName.externalId = "REQ-LONGNAME";
    longName.type = tl::EntityType::Requirement;
    longName.name = std::string(300, 'A');
    longName.text = "x";
    longName.status = "Draft";
    auto e3 = svc.addEntity(longName);
    h.check(e3.failed() && e3.errorCode() == c::ErrorCode::LimitExceeded,
            "name > 256 chars -> LimitExceeded");

    // Text longer than 65536 chars -> LimitExceeded.
    tl::Entity longText;
    longText.externalId = "REQ-LONGTEXT";
    longText.type = tl::EntityType::Requirement;
    longText.name = "REQ-LONGTEXT";
    longText.text = std::string(70000, 'x');
    longText.status = "Draft";
    auto e4 = svc.addEntity(longText);
    h.check(e4.failed() && e4.errorCode() == c::ErrorCode::LimitExceeded,
            "text > 65536 chars -> LimitExceeded");

    // Unknown status -> InvalidArgument.
    tl::Entity badStatus;
    badStatus.externalId = "REQ-BADSTATUS";
    badStatus.type = tl::EntityType::Requirement;
    badStatus.name = "REQ-BADSTATUS";
    badStatus.text = "x";
    badStatus.status = "NotARealStatus";
    auto e5 = svc.addEntity(badStatus);
    h.check(e5.failed() && e5.errorCode() == c::ErrorCode::InvalidArgument,
            "unknown status -> InvalidArgument");

    // Negative limit in a filter -> InvalidArgument.
    tl::EntityFilter f;
    f.limit = -1;
    auto neg = svc.listEntities(tl::EntityType::Requirement, f);
    h.check(neg.failed() && neg.errorCode() == c::ErrorCode::InvalidArgument,
            "negative limit -> InvalidArgument");

    // A valid entity still succeeds after all the rejections.
    auto ok = svc.addEntity(makeReq("REQ-OK"));
    h.check(ok.isOk(), "valid entity accepted after validation checks");

    db.close();
    std::remove("lodestar_wpF_input.db");
    std::remove("lodestar_wpF_input.db-wal");
    std::remove("lodestar_wpF_input.db-shm");
}

// ---------------------------------------------------------------------------
// B4. DB backup / restore
// ---------------------------------------------------------------------------
void testBackupRestore(Harness& h) {
    h.section("B4. DB backup / restore");

    const char* dbfile = "lodestar_wpF_backup.db";
    const char* bakfile = "lodestar_wpF_backup.bak";
    std::remove(dbfile);
    std::remove(bakfile);

    {
        p::Database db;
        if (!openFreshDb(db, dbfile)) {
            h.check(false, "open fresh db");
            return;
        }
        tl::TraceLinkService svc(db);
        auto r = svc.addEntity(makeReq("REQ-BK1"));
        h.check(r.isOk(), "seed entity before backup ok");
        auto b = db.backup(bakfile);
        h.check(b.isOk(), "backup ok");
        // Mutate after backup.
        auto r2 = svc.addEntity(makeReq("REQ-BK2"));
        h.check(r2.isOk(), "seed entity after backup ok");
        db.close();
    }

    // Restore from the backup into a fresh DB and verify only REQ-BK1 exists.
    {
        p::Database db;
        std::remove(dbfile);
        if (db.open(dbfile).failed()) {
            h.check(false, "open db for restore");
            return;
        }
        auto rest = db.restore(bakfile);
        h.check(rest.isOk(), "restore ok");
        tl::TraceLinkService svc(db);
        auto all = svc.listEntities(tl::EntityType::Requirement, tl::EntityFilter{});
        h.check(all.isOk() && all.value().size() == 1,
                "restored DB has exactly the pre-backup entity");
        if (all.isOk() && all.value().size() == 1) {
            h.check(all.value().front().externalId == "REQ-BK1",
                    "restored entity is the pre-backup one");
        }
        db.close();
    }

    std::remove(dbfile);
    std::remove(bakfile);
}

// ---------------------------------------------------------------------------
// B5. Migration safety (dry-run / checksum / verify)
// ---------------------------------------------------------------------------
void testMigrationSafety(Harness& h) {
    h.section("B5. Migration safety (dry-run / checksum / verify)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpF_mig.db")) {
        h.check(false, "open fresh db");
        return;
    }
    p::MigrationRunner runner(db);

    // After a full run the schema is up to date.
    auto dry = runner.dryRun(g_migrationsDir);
    h.check(dry.isOk() && dry.value() == false,
            "dryRun reports no pending migrations after full run");

    // Checksum is non-empty and stable across calls.
    std::string cs1 = runner.checksum();
    std::string cs2 = runner.checksum();
    h.check(!cs1.empty(), "checksum is non-empty");
    h.check(cs1 == cs2, "checksum is stable across calls");

    // Verify reports the applied schema is consistent with the files.
    auto verify = runner.verify(g_migrationsDir);
    h.check(verify.isOk() && verify.value() == true,
            "verify reports consistent schema");

    db.close();
    std::remove("lodestar_wpF_mig.db");
    std::remove("lodestar_wpF_mig.db-wal");
    std::remove("lodestar_wpF_mig.db-shm");
}

// ---------------------------------------------------------------------------
// B6. Fuzz / edge-case tests
// ---------------------------------------------------------------------------
void testFuzz(Harness& h) {
    h.section("B6. Fuzz / edge-case robustness");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpF_fuzz.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);

    // Special characters in name/text must not crash and must round-trip.
    tl::Entity special;
    special.externalId = "REQ-SPECIAL";
    special.type = tl::EntityType::Requirement;
    special.name = "Quote \" backslash \\ newline \n tab \t";
    special.text = "Unicode: \u00e9\u00e8\u00ea. Symbols: <>&;'()[]{}";
    special.status = "Draft";
    auto s = svc.addEntity(special);
    h.check(s.isOk(), "entity with special characters accepted");
    if (s.isOk()) {
        auto got = svc.getEntity(tl::EntityType::Requirement, s.value().id);
        h.check(got.isOk() && got.value().has_value() &&
                    got.value()->name == special.name,
                "special-character name round-trips");
    }

    // A very long but within-limit name is accepted.
    tl::Entity longOk;
    longOk.externalId = "REQ-LONGOK";
    longOk.type = tl::EntityType::Requirement;
    longOk.name = std::string(256, 'B');
    longOk.text = "x";
    longOk.status = "Draft";
    auto lo = svc.addEntity(longOk);
    h.check(lo.isOk(), "256-char name (at limit) accepted");

    // Empty search text must not crash.
    auto emptySearch = svc.search(tl::EntityType::Requirement, "");
    h.check(emptySearch.isOk(), "empty search text does not crash");

    // A filter with only offset (no limit) must not crash.
    tl::EntityFilter f;
    f.offset = 5;
    auto offOnly = svc.listEntities(tl::EntityType::Requirement, f);
    h.check(offOnly.isOk(), "offset-only filter does not crash");

    // A link with an unknown relation must be rejected cleanly.
    auto r = svc.addEntity(makeReq("REQ-FUZZ"));
    h.check(r.isOk(), "seed fuzz entity ok");
    tl::Link badRel;
    badRel.sourceType = tl::EntityType::Requirement;
    badRel.sourceId = r.value().id;
    badRel.targetType = tl::EntityType::Requirement;
    badRel.targetId = r.value().id;
    badRel.relation = "not_a_relation";
    auto br = svc.addLink(badRel);
    h.check(br.failed(), "unknown relation rejected cleanly");

    db.close();
    std::remove("lodestar_wpF_fuzz.db");
    std::remove("lodestar_wpF_fuzz.db-wal");
    std::remove("lodestar_wpF_fuzz.db-shm");
}

// ---------------------------------------------------------------------------
// B7. Concurrency stress test
// ---------------------------------------------------------------------------
void testConcurrency(Harness& h) {
    h.section("B7. Concurrency stress (WAL, concurrent writers)");

    const char* dbfile = "lodestar_wpF_conc.db";
    std::remove(dbfile);
    std::remove((std::string(dbfile) + "-wal").c_str());
    std::remove((std::string(dbfile) + "-shm").c_str());

    // Main connection: open + migrate once so the schema exists on disk.
    {
        p::Database db;
        if (!openFreshDb(db, dbfile)) {
            h.check(false, "open fresh db");
            return;
        }
        db.close();
    }

    const int kThreads = 4;
    const int kPerThread = 25;
    std::atomic<int> failures{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            // Each thread opens its own connection to the same file.
            p::Database db;
            if (db.open(dbfile).failed()) {
                ++failures;
                return;
            }
            tl::TraceLinkService svc(db);
            for (int i = 0; i < kPerThread; ++i) {
                auto r = svc.addEntity(makeReq("CONC-" + std::to_string(t) + "-" +
                                               std::to_string(i)));
                if (r.failed()) {
                    ++failures;
                }
            }
            db.close();
        });
    }
    for (auto& th : threads) {
        th.join();
    }

    h.check(failures.load() == 0, "all concurrent writes succeeded");

    // Verify every write persisted and no duplicates.
    p::Database db;
    if (db.open(dbfile).failed()) {
        h.check(false, "reopen db for verification");
        return;
    }
    std::string count = db.queryScalar("SELECT count(*) FROM requirements;");
    h.check(count == std::to_string(kThreads * kPerThread),
            "all concurrent writes persisted (no lost updates)");
    std::string distinct = db.queryScalar(
        "SELECT count(DISTINCT external_id) FROM requirements;");
    h.check(distinct == std::to_string(kThreads * kPerThread),
            "no duplicate external ids under concurrency");
    db.close();

    std::remove(dbfile);
    std::remove((std::string(dbfile) + "-wal").c_str());
    std::remove((std::string(dbfile) + "-shm").c_str());
}

// ---------------------------------------------------------------------------
// B8. Structured logging
// ---------------------------------------------------------------------------
void testStructuredLogging(Harness& h) {
    h.section("B8. Structured logging");

    const char* logfile = "lodestar_wpF_log.jsonl";
    std::remove(logfile);

    c::Logger& logger = c::Logger::instance();
    logger.setFile(logfile);
    logger.structured(c::LogLevel::Info, "entity.created",
                      "{\"type\":\"requirement\",\"external_id\":\"REQ-LOG\"}");
    logger.structured(c::LogLevel::Error, "validation.failed",
                      "{\"rule\":\"REQ_MUST_BE_VERIFIED\"}");
    logger.flush();

    std::ifstream in(logfile);
    std::stringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();
    in.close();

    h.check(content.find("\"event\":\"entity.created\"") != std::string::npos,
            "log contains the event name");
    h.check(content.find("\"level\":\"INFO\"") != std::string::npos,
            "log contains the level");
    h.check(content.find("\"external_id\":\"REQ-LOG\"") != std::string::npos,
            "log contains the structured fields");
    h.check(content.find("\"event\":\"validation.failed\"") != std::string::npos,
            "log contains the second event");

    std::remove(logfile);
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-F robustness hardening");
    std::printf("WP-F ROBUSTNESS HARDENING TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testErrorCodes(h);
    testInputValidation(h);
    testBackupRestore(h);
    testMigrationSafety(h);
    testFuzz(h);
    testConcurrency(h);
    testStructuredLogging(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
