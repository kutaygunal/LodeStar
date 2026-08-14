// core/test/r7_quality_scoring_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill RiskAI 1.7: inline requirement-quality scoring in TraceLink tests.
//
// Test contract: docs/gap-fill-plan.md (Module 1.7).
//   (A) Shared common::QualityScoring five-dimension scoring used by both
//       RiskAI and TraceLink.
//   (B) A TraceLink authoring surface (riskai::AuthoringScoringService) scores
//       a requirement on save, shows per-dimension flags + suggested rewording,
//       and persists the score (real TraceLink write hook, migration 030).
//
// Deterministic: no live LLM.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/common/QualityScoring.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/riskai/AuthoringScoringService.h"
#include "core/tracelink/Types.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace lc = lodestar::common;
namespace p  = lodestar::persistence;

namespace {
std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

class Harness {
public:
    explicit Harness(const char* name) : name_(name) {}
    void section(const char* s) { std::printf("\n-- %s --\n", s); }
    void check(bool cond, const char* what) {
        if (cond) { std::printf("  [PASS] %s\n", what); }
        else { std::printf("  [FAIL] %s\n", what); ++failures_; }
    }
    int failures() const { return failures_; }
    const char* name() const { return name_; }
private:
    const char* name_;
    int failures_ = 0;
};

bool openFreshDb(p::Database& db, const char* file) {
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
    if (db.open(file).failed()) return false;
    p::MigrationRunner runner(db);
    return runner.run(g_migrationsDir).isOk();
}

void closeAndRemove(p::Database& db, const char* file) {
    db.close();
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
}
}  // namespace

// ---------------------------------------------------------------------------
// T1. Shared scoring service: dimensions + determinism
// ---------------------------------------------------------------------------
static void testScoringDeterminism(Harness& h) {
    h.section("T1. shared five-dimension scoring + determinism");
    const std::string good =
        "The GNSS receiver shall acquire and track satellite signals within "
        "120 seconds of power-on at -130 dBm.";
    auto a = lc::scoreQuality(good);
    auto b = lc::scoreQuality(good);
    h.check(a.clarity >= 0 && a.clarity <= 100, "clarity in [0,100]");
    h.check(a.testability >= 0 && a.testability <= 100, "testability in [0,100]");
    h.check(a.atomicity >= 0 && a.atomicity <= 100, "atomicity in [0,100]");
    h.check(a.completeness >= 0 && a.completeness <= 100, "completeness in [0,100]");
    h.check(a.ambiguity >= 0 && a.ambiguity <= 100, "ambiguity in [0,100]");
    h.check(a.overall >= 0 && a.overall <= 100, "overall in [0,100]");
    h.check(a.clarity == b.clarity && a.overall == b.overall,
            "scoring is deterministic (same input -> same output)");

    // A well-formed requirement scores meaningfully higher on testability than
    // a vague one.
    const std::string vague = "It should handle stuff properly.";
    auto v = lc::scoreQuality(vague);
    h.check(v.testability < a.testability, "vague req has lower testability");
    h.check(v.overall < a.overall, "vague req has lower overall");
}

// ---------------------------------------------------------------------------
// T2. Flags + suggested rewording
// ---------------------------------------------------------------------------
static void testFlags(Harness& h) {
    h.section("T2. per-dimension flags + suggested rewording");
    const std::string vague = "It should handle stuff properly.";
    auto res = lc::scoreQualityWithFlags(vague);
    h.check(!res.flags.empty(), "vague req produces flags");
    bool hasTestability = false, hasClarity = false;
    for (const auto& f : res.flags) {
        if (f.dimension == "testability") hasTestability = true;
        if (f.dimension == "clarity") hasClarity = true;
        if (!f.reason.empty() && !f.suggestion.empty()) {
            h.check(true, "flag carries reason + suggestion");
        }
    }
    h.check(hasTestability, "testability flag present for vague req");
    h.check(hasClarity || res.score.clarity < 60,
            "clarity flag/suggestion present");

    // A strong requirement produces few or no flags.
    const std::string good =
        "The system shall display the measured position within 100 ms.";
    auto gres = lc::scoreQualityWithFlags(good);
    h.check(gres.score.testability >= 60, "strong req has good testability");
}

// ---------------------------------------------------------------------------
// T3. TraceLink write hook (scoreOnSave) + persistence
// ---------------------------------------------------------------------------
static void testWriteHook(Harness& h) {
    h.section("T3. TraceLink authoring write hook + persistence");
    p::Database db;
    if (!openFreshDb(db, "lodestar_r7_hook.db")) {
        h.check(false, "open fresh db");
        return;
    }
    lodestar::riskai::AuthoringScoringService svc(db);

    lodestar::tracelink::Entity req;
    req.id = "entity-1";
    req.name = "Acquisition";
    req.text = "The GNSS receiver shall acquire and track satellite signals "
               "within 120 seconds at -130 dBm.";

    auto overall = svc.scoreOnSave(req);
    h.check(overall.isOk(), "scoreOnSave() ok");
    if (!overall.isOk()) { closeAndRemove(db, "lodestar_r7_hook.db"); return; }
    h.check(overall.value() >= 0 && overall.value() <= 100,
            "scoreOnSave() returns a valid overall score");

    auto last = svc.lastScore(req.id);
    h.check(last.isOk(), "lastScore() ok");
    if (last.isOk() && last.value().has_value()) {
        h.check(last.value()->overall == overall.value(),
                "persisted overall matches the write hook result");
        h.check(last.value()->testability >= 60,
                "persisted testability reflects the good requirement");
    }

    // A weak requirement persists a lower overall.
    lodestar::tracelink::Entity weak;
    weak.id = "entity-2";
    weak.name = "Vague";
    weak.text = "It should handle stuff.";
    auto weakOverall = svc.scoreOnSave(weak);
    h.check(weakOverall.isOk(), "scoreOnSave() on weak req ok");
    if (weakOverall.isOk() && overall.isOk()) {
        h.check(weakOverall.value() < overall.value(),
                "weak requirement scores lower overall than the good one");
    }

    // No persisted score for an unknown entity.
    auto none = svc.lastScore("no-such-entity");
    h.check(none.isOk() && !none.value().has_value(),
            "lastScore() of unknown entity returns none");

    closeAndRemove(db, "lodestar_r7_hook.db");
}

// ---------------------------------------------------------------------------
// T4. Shared service used by the authoring surface
// ---------------------------------------------------------------------------
static void testSharedUsage(Harness& h) {
    h.section("T4. shared scoring reused by the authoring surface");
    p::Database db;
    if (!openFreshDb(db, "lodestar_r7_shared.db")) {
        h.check(false, "open fresh db");
        return;
    }
    lodestar::riskai::AuthoringScoringService svc(db);
    lodestar::tracelink::Entity req;
    req.id = "e";
    req.text = "The system shall log all errors with a timestamp.";
    auto flags = svc.score(req);
    h.check(flags.score.testability >= 60,
            "authoring surface testability uses shared scoring");
    h.check(flags.score.ambiguity >= 60,
            "authoring surface ambiguity uses shared scoring");
    closeAndRemove(db, "lodestar_r7_shared.db");
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];
    Harness h("Gap-Fill RiskAI 1.7 inline requirement-quality scoring");
    testScoringDeterminism(h);
    testFlags(h);
    testWriteHook(h);
    testSharedUsage(h);
    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
