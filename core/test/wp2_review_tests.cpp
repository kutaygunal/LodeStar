// core/test/wp2_review_tests.cpp
// ---------------------------------------------------------------------------
// WP-2 (Phase 10) unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-2 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-2): general artifact review / comment / approval
// (beyond the change-request workflow); migration 014.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-2 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 014 (core/persistence/migrations/014_*.sql) creates `comments`
//     and `reviews` tables (append-only, idempotent).
//
// (B) New ReviewService (core/tracelink/ReviewService.h):
//
//   struct Comment {
//       std::string id;
//       std::string entityType;
//       std::string entityId;
//       std::string author;
//       std::string body;
//       std::string createdAt;
//   };
//
//   struct Review {
//       std::string id;
//       std::string entityType;
//       std::string entityId;
//       std::string reviewer;
//       std::string verdict;   // Approve | Reject | RequestChanges
//       std::string comment;
//       std::string createdAt;
//   };
//
//   class ReviewService {
//   public:
//       explicit ReviewService(persistence::Database& db);
//
//       common::Result<Comment> addComment(const std::string& entityType,
//                                          const std::string& entityId,
//                                          const std::string& author,
//                                          const std::string& body);
//
//       common::Result<std::vector<Comment>> commentsFor(
//           const std::string& entityType, const std::string& entityId);
//
//       common::Result<Review> submitReview(const std::string& entityType,
//                                           const std::string& entityId,
//                                           const std::string& reviewer,
//                                           const std::string& verdict,
//                                           const std::string& comment);
//
//       common::Result<std::vector<Review>> reviewsFor(
//           const std::string& entityType, const std::string& entityId);
//
//       common::Result<std::string> approvalStatus(
//           const std::string& entityType, const std::string& entityId);
//   };
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/ReviewService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace p  = lodestar::persistence;

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

// ---------------------------------------------------------------------------
// T1. Migration 014 applies
// ---------------------------------------------------------------------------
void testMigration014(Harness& h) {
    h.section("T1. Migration 014 applies");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_t1.db")) {
        h.check(false, "open fresh db");
        return;
    }

    // The comments and reviews tables must exist after migrations run.
    auto comments = db.queryScalar(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='comments';");
    h.check(comments == "comments", "comments table exists");

    auto reviews = db.queryScalar(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='reviews';");
    h.check(reviews == "reviews", "reviews table exists");

    // Idempotency: re-running migrations must not fail.
    p::MigrationRunner runner(db);
    auto again = runner.run(g_migrationsDir);
    h.check(again.isOk(), "re-running migrations is idempotent");

    db.close();
    std::remove("lodestar_wp2_t1.db");
    std::remove("lodestar_wp2_t1.db-wal");
    std::remove("lodestar_wp2_t1.db-shm");
}

// ---------------------------------------------------------------------------
// T2. addComment + commentsFor roundtrip
// ---------------------------------------------------------------------------
void testCommentRoundtrip(Harness& h) {
    h.section("T2. addComment + commentsFor roundtrip");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_t2.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::ReviewService svc(db);

    auto c1 = svc.addComment("requirement", "R1", "alice", "First comment");
    h.check(c1.isOk(), "add first comment ok");
    h.check(c1.value().author == "alice" && c1.value().body == "First comment",
            "first comment author/body preserved");
    h.check(!c1.value().id.empty(), "first comment got a UUID");

    auto c2 = svc.addComment("requirement", "R1", "bob", "Second comment");
    h.check(c2.isOk(), "add second comment ok");

    auto all = svc.commentsFor("requirement", "R1");
    h.check(all.isOk(), "commentsFor ok");
    h.check(all.value().size() == 2, "commentsFor returns both comments");
    h.check(all.value()[0].body == "First comment" &&
                all.value()[1].body == "Second comment",
            "comments returned oldest first");
    h.check(all.value()[0].author == "alice" && all.value()[1].author == "bob",
            "comment authors preserved in order");

    db.close();
    std::remove("lodestar_wp2_t2.db");
    std::remove("lodestar_wp2_t2.db-wal");
    std::remove("lodestar_wp2_t2.db-shm");
}

// ---------------------------------------------------------------------------
// T3. submitReview Approve -> approvalStatus Approved
// ---------------------------------------------------------------------------
void testApprove(Harness& h) {
    h.section("T3. submitReview Approve -> approvalStatus Approved");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_t3.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::ReviewService svc(db);

    auto r = svc.submitReview("requirement", "R1", "alice", "Approve", "ok");
    h.check(r.isOk(), "submitReview Approve ok");
    h.check(r.value().verdict == "Approve" && r.value().reviewer == "alice",
            "review verdict/reviewer preserved");
    h.check(!r.value().id.empty(), "review got a UUID");

    auto status = svc.approvalStatus("requirement", "R1");
    h.check(status.isOk() && status.value() == "Approved",
            "approvalStatus is Approved");

    auto reviews = svc.reviewsFor("requirement", "R1");
    h.check(reviews.isOk() && reviews.value().size() == 1,
            "reviewsFor returns the review");
    h.check(reviews.value()[0].verdict == "Approve",
            "returned review has Approve verdict");

    db.close();
    std::remove("lodestar_wp2_t3.db");
    std::remove("lodestar_wp2_t3.db-wal");
    std::remove("lodestar_wp2_t3.db-shm");
}

// ---------------------------------------------------------------------------
// T4. submitReview Reject -> approvalStatus Rejected
// ---------------------------------------------------------------------------
void testReject(Harness& h) {
    h.section("T4. submitReview Reject -> approvalStatus Rejected");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_t4.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::ReviewService svc(db);

    auto r = svc.submitReview("requirement", "R2", "carol", "Reject", "not ready");
    h.check(r.isOk(), "submitReview Reject ok");

    auto status = svc.approvalStatus("requirement", "R2");
    h.check(status.isOk() && status.value() == "Rejected",
            "approvalStatus is Rejected");

    db.close();
    std::remove("lodestar_wp2_t4.db");
    std::remove("lodestar_wp2_t4.db-wal");
    std::remove("lodestar_wp2_t4.db-shm");
}

// ---------------------------------------------------------------------------
// T5. Latest verdict wins
// ---------------------------------------------------------------------------
void testLatestVerdictWins(Harness& h) {
    h.section("T5. Latest verdict wins");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_t5.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::ReviewService svc(db);

    auto r1 = svc.submitReview("requirement", "R3", "dave", "RequestChanges", "fix it");
    h.check(r1.isOk(), "submitReview RequestChanges ok");
    auto s1 = svc.approvalStatus("requirement", "R3");
    h.check(s1.isOk() && s1.value() == "RequestChanges",
            "approvalStatus is RequestChanges after first review");

    auto r2 = svc.submitReview("requirement", "R3", "erin", "Approve", "fixed");
    h.check(r2.isOk(), "submitReview Approve ok");

    auto s2 = svc.approvalStatus("requirement", "R3");
    h.check(s2.isOk() && s2.value() == "Approved",
            "latest review governs -> approvalStatus is Approved");

    auto reviews = svc.reviewsFor("requirement", "R3");
    h.check(reviews.isOk() && reviews.value().size() == 2,
            "reviewsFor returns both reviews");
    h.check(reviews.value()[0].verdict == "Approve",
            "reviews returned newest first");

    db.close();
    std::remove("lodestar_wp2_t5.db");
    std::remove("lodestar_wp2_t5.db-wal");
    std::remove("lodestar_wp2_t5.db-shm");
}

// ---------------------------------------------------------------------------
// T6. Per-entity isolation
// ---------------------------------------------------------------------------
void testIsolation(Harness& h) {
    h.section("T6. Per-entity isolation");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_t6.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::ReviewService svc(db);

    // Comment/review artifact A only.
    auto c = svc.addComment("requirement", "A", "frank", "comment on A");
    h.check(c.isOk(), "add comment to A ok");
    auto r = svc.submitReview("requirement", "A", "grace", "Approve", "ok");
    h.check(r.isOk(), "submit review on A ok");

    // Artifact B has nothing.
    auto cb = svc.commentsFor("requirement", "B");
    h.check(cb.isOk() && cb.value().empty(), "commentsFor(B) is empty");

    auto rb = svc.reviewsFor("requirement", "B");
    h.check(rb.isOk() && rb.value().empty(), "reviewsFor(B) is empty");

    auto sb = svc.approvalStatus("requirement", "B");
    h.check(sb.isOk() && sb.value() == "None", "approvalStatus(B) is None");

    // Artifact A still has its own data.
    auto ca = svc.commentsFor("requirement", "A");
    h.check(ca.isOk() && ca.value().size() == 1, "commentsFor(A) still has its comment");
    auto sa = svc.approvalStatus("requirement", "A");
    h.check(sa.isOk() && sa.value() == "Approved", "approvalStatus(A) still Approved");

    db.close();
    std::remove("lodestar_wp2_t6.db");
    std::remove("lodestar_wp2_t6.db-wal");
    std::remove("lodestar_wp2_t6.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-2 review/comment/approval");
    std::printf("WP-2 REVIEW/COMMENT/APPROVAL TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testMigration014(h);
    testCommentRoundtrip(h);
    testApprove(h);
    testReject(h);
    testLatestVerdictWins(h);
    testIsolation(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
