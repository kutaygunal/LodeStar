// core/test/r5_knowledge_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill RiskAI 1.5: multi-document knowledge input tests.
//
// Test contract: docs/gap-fill-plan.md (Module 1.5).
//   (A) Migration 029 creates riskai_knowledge_doc / riskai_knowledge_chunk.
//   (B) core/riskai/KnowledgeService.h (+ .cpp) ingests multi-type source
//       documents and retrieves deterministic context (keyword + type
//       weighting) to feed the LLM prompt, with a working no-LLM fallback.
//
// Deterministic: no live LLM.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/riskai/KnowledgeService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ra = lodestar::riskai;
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

bool tableExists(p::Database& db, const std::string& table) {
    return db.queryScalar(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='" +
        table + "';") == "1";
}

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

// ---------------------------------------------------------------------------
// T1. Migration 029 + ingestion round-trip
// ---------------------------------------------------------------------------
void testIngest(Harness& h) {
    h.section("T1. migration 029 + ingestion round-trip");
    p::Database db;
    if (!openFreshDb(db, "lodestar_r5_ingest.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    h.check(tableExists(db, "riskai_knowledge_doc"),
            "riskai_knowledge_doc table exists");
    h.check(tableExists(db, "riskai_knowledge_chunk"),
            "riskai_knowledge_chunk table exists");

    ra::KnowledgeService svc(db);
    ra::KnowledgeDoc doc;
    doc.type = ra::KnowledgeDocType::Standard;
    doc.title = "DO-178C";
    doc.content = "Software considerations in airborne systems certification.";
    doc.source = "do178c.pdf";
    auto id = svc.ingest(doc);
    h.check(id.isOk(), "ingest() ok");
    if (!id.isOk()) { closeAndRemove(db, "lodestar_r5_ingest.db"); return; }
    h.check(!id.value().empty(), "ingest() returns a non-empty id");

    auto list = svc.list();
    h.check(list.isOk() && list.value().size() == 1,
            "list() returns 1 doc");
    if (list.isOk() && list.value().size() == 1) {
        h.check(list.value()[0].title == "DO-178C", "title == \"DO-178C\"");
        h.check(list.value()[0].type == ra::KnowledgeDocType::Standard,
                "type == Standard");
        h.check(list.value()[0].source == "do178c.pdf", "source preserved");
    }

    // Filter by type.
    auto stds = svc.list(ra::KnowledgeDocType::Standard);
    h.check(stds.isOk() && stds.value().size() == 1,
            "list(Standard) returns 1");
    auto reqs = svc.list(ra::KnowledgeDocType::Requirement);
    h.check(reqs.isOk() && reqs.value().empty(),
            "list(Requirement) returns 0 (none ingested)");

    closeAndRemove(db, "lodestar_r5_ingest.db");
}

// ---------------------------------------------------------------------------
// T2. Retrieval: keyword scoring + type weighting
// ---------------------------------------------------------------------------
void testRetrieval(Harness& h) {
    h.section("T2. deterministic retrieval + type weighting");
    p::Database db;
    if (!openFreshDb(db, "lodestar_r5_retrieve.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ra::KnowledgeService svc(db);

    // Two docs mentioning "antenna" — the standard should rank above the other
    // (type weighting) when both match.
    ra::KnowledgeDoc standard;
    standard.type = ra::KnowledgeDocType::Standard;
    standard.title = "Antenna Standard";
    standard.content = "Antenna gain requirements for airborne receivers.";
    svc.ingest(standard);

    ra::KnowledgeDoc drawing;
    drawing.type = ra::KnowledgeDocType::Drawing;
    drawing.title = "Antenna Drawing";
    drawing.content = "Mechanical drawing of the antenna mount.";
    svc.ingest(drawing);

    auto hits = svc.retrieve("antenna", 5);
    h.check(hits.isOk(), "retrieve() ok");
    if (!hits.isOk()) { closeAndRemove(db, "lodestar_r5_retrieve.db"); return; }
    h.check(hits.value().size() == 2, "both antenna docs retrieved");
    if (hits.value().size() == 2) {
        // Standard (weight 3) ranks above drawing (weight 1).
        auto st = svc.list(ra::KnowledgeDocType::Standard);
        const std::string stdId = st.value()[0].id;
        h.check(hits.value()[0].docId == stdId,
                "standard doc ranks first (type weighting)");
        h.check(hits.value()[0].score > hits.value()[1].score,
                "standard score > drawing score");
    }

    // No match -> empty.
    auto none = svc.retrieve("nonsense_zzz", 5);
    h.check(none.isOk() && none.value().empty(), "no match returns empty");
    // Empty query -> empty.
    auto empty = svc.retrieve("", 5);
    h.check(empty.isOk() && empty.value().empty(), "empty query returns empty");

    closeAndRemove(db, "lodestar_r5_retrieve.db");
}

// ---------------------------------------------------------------------------
// T3. buildContext feeds the LLM prompt (deterministic fallback)
// ---------------------------------------------------------------------------
void testBuildContext(Harness& h) {
    h.section("T3. buildContext (no-LLM fallback)");
    p::Database db;
    if (!openFreshDb(db, "lodestar_r5_ctx.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ra::KnowledgeService svc(db);
    ra::KnowledgeDoc doc;
    doc.type = ra::KnowledgeDocType::LessonsLearned;
    doc.title = "Past failures";
    doc.content = "Loss of lock occurred under RF interference in prior flights.";
    svc.ingest(doc);

    auto hits = svc.retrieve("RF interference", 3);
    h.check(hits.isOk() && !hits.value().empty(), "retrieve() finds the lesson");
    std::string ctx = svc.buildContext(hits.value());
    h.check(!ctx.empty(), "buildContext() returns a non-empty string");
    h.check(ctx.find("[1]") != std::string::npos, "context is numbered");
    h.check(ctx.find("RF interference") != std::string::npos,
            "context contains the retrieved content");

    // Empty chunks -> empty context.
    std::vector<ra::KnowledgeChunk> none;
    h.check(svc.buildContext(none).empty(),
            "buildContext() of empty chunks is empty");

    closeAndRemove(db, "lodestar_r5_ctx.db");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];

    Harness h("Gap-Fill RiskAI 1.5 multi-document knowledge input");
    testIngest(h);
    testRetrieval(h);
    testBuildContext(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
