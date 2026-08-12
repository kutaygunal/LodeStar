// core/test/wp10_doc_tests.cpp
// ---------------------------------------------------------------------------
// WP-10 unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-10 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-10): document-style authoring — author requirements in a
// document context with atomic traceability.
//
// WP-10 is a Qt Widgets UI work package. Following the WP-6 precedent, this
// contract verifies the QT-INDEPENDENT wiring the Qt views consume (pure C++,
// testable without a display) and documents the UI build acceptance step.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-10 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Extend core/tracelink/UiWiringService.h (namespace lodestar::tracelink):
//
//   // One section of a document (a container of ordered requirements).
//   struct DocumentSection {
//       std::string id;
//       std::string title;
//       std::vector<Entity> requirements;  // ordered by sortOrder then id
//   };
//
//   // A document: a root container with ordered sections.
//   struct DocumentModel {
//       std::string id;
//       std::string title;
//       std::vector<DocumentSection> sections;
//   };
//
//   class UiWiringService {
//       // ... existing refreshAll(), impact(), projectTree(), detail(), ...
//
//       // Builds a document model from the hierarchy rooted at `docId` (a
//       // requirement-type root whose children are sections, whose children
//       // are requirements). Fails cleanly if the document root is missing.
//       common::Result<DocumentModel> document(const std::string& docId);
//
//       // Creates a requirement and attaches it to a section with atomic
//       // traceability (the requirement is created AND linked to the section
//       // in one operation). Returns the created requirement.
//       common::Result<Entity> addRequirementToDocument(
//           const std::string& docId, const std::string& sectionId,
//           const Entity& req);
//
//       // Reorders the requirements within a section to the given id order.
//       common::Result<void> reorderRequirements(
//           const std::string& docId, const std::string& sectionId,
//           const std::vector<std::string>& orderedIds);
//   };
//
// (B) ui/DocumentView renders the document (sections + requirements) from
//     document() and calls addRequirementToDocument() / reorderRequirements().
//     Not compiled here (Qt absent); the wiring it calls is what this contract
//     verifies.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"
#include "core/tracelink/UiWiringService.h"

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
// Factories.
// ---------------------------------------------------------------------------
tl::Entity makeReq(const std::string& extId, const std::string& status = "Draft") {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Requirement;
    e.name = extId;
    e.text = "Body of " + extId;
    e.status = status;
    e.owner = "engineer";
    return e;
}

// Builds a document root D with a section S (child of D). Returns the ids.
struct DocFixture {
    std::string docId;
    std::string sectionId;
};

DocFixture buildDoc(tl::TraceLinkService& svc) {
    DocFixture f;
    auto doc = svc.addEntity(makeReq("DOC-1", "Approved"));
    auto sec = svc.addEntity(makeReq("SEC-1", "Approved"));
    f.docId = doc.value().id;
    f.sectionId = sec.value().id;
    svc.setParent(tl::EntityType::Requirement, f.sectionId, f.docId);
    return f;
}

bool sectionHasReq(const tl::DocumentModel& m, const std::string& sectionId,
                   const std::string& reqId) {
    for (const auto& s : m.sections) {
        if (s.id != sectionId) continue;
        for (const auto& r : s.requirements) {
            if (r.id == reqId) return true;
        }
    }
    return false;
}

bool hasActiveLinkTo(tl::TraceLinkService& svc, const std::string& reqId,
                     const std::string& sectionId) {
    auto links = svc.linksFrom(tl::EntityType::Requirement, reqId);
    if (links.failed()) return false;
    for (const auto& l : links.value()) {
        if (l.status == "Active" && l.targetId == sectionId) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// T1. document() builds the model from the hierarchy
// ---------------------------------------------------------------------------
void testDocumentBuilds(Harness& h) {
    h.section("T1. document() builds the model from the hierarchy");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp10_t1.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    DocFixture f = buildDoc(svc);

    // Add two requirements R1, R2 as children of section S. Give them explicit
    // sortOrder so the document order is deterministic (sortOrder then id).
    auto r1 = makeReq("R1");
    auto r2 = makeReq("R2");
    r1.sortOrder = 0;
    r2.sortOrder = 1;
    auto r1r = svc.addEntity(r1);
    auto r2r = svc.addEntity(r2);
    h.check(r1r.isOk() && r2r.isOk(), "add R1 and R2 ok");
    svc.setParent(tl::EntityType::Requirement, r1r.value().id, f.sectionId);
    svc.setParent(tl::EntityType::Requirement, r2r.value().id, f.sectionId);

    auto doc = wiring.document(f.docId);
    h.check(doc.isOk(), "document() ok");
    if (!doc.isOk()) {
        db.close();
        return;
    }
    const tl::DocumentModel& m = doc.value();
    h.check(m.id == f.docId, "document id matches root");
    h.check(m.sections.size() == 1, "document has one section");
    if (m.sections.size() == 1) {
        h.check(m.sections[0].id == f.sectionId, "section id matches");
        h.check(m.sections[0].requirements.size() == 2,
                "section has 2 requirements");
        if (m.sections[0].requirements.size() == 2) {
            h.check(m.sections[0].requirements[0].id == r1r.value().id &&
                        m.sections[0].requirements[1].id == r2r.value().id,
                    "requirements in order (R1 then R2)");
        }
    }

    db.close();
    std::remove("lodestar_wp10_t1.db");
    std::remove("lodestar_wp10_t1.db-wal");
    std::remove("lodestar_wp10_t1.db-shm");
}

// ---------------------------------------------------------------------------
// T2. document() on a missing root fails cleanly
// ---------------------------------------------------------------------------
void testDocumentMissing(Harness& h) {
    h.section("T2. document() on a missing root fails cleanly");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp10_t2.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);

    auto doc = wiring.document("does-not-exist");
    h.check(doc.failed(), "document() on missing root fails");

    db.close();
    std::remove("lodestar_wp10_t2.db");
    std::remove("lodestar_wp10_t2.db-wal");
    std::remove("lodestar_wp10_t2.db-shm");
}

// ---------------------------------------------------------------------------
// T3. addRequirementToDocument() creates + links atomically
// ---------------------------------------------------------------------------
void testAddRequirement(Harness& h) {
    h.section("T3. addRequirementToDocument() creates + links atomically");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp10_t3.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    DocFixture f = buildDoc(svc);

    auto created = wiring.addRequirementToDocument(f.docId, f.sectionId,
                                                   makeReq("R-NEW"));
    h.check(created.isOk(), "addRequirementToDocument() ok");
    if (!created.isOk()) {
        db.close();
        return;
    }
    const std::string newId = created.value().id;

    // Returns the created requirement.
    h.check(!newId.empty(), "created requirement has an id");

    // document(D) now shows it in section S.
    auto doc = wiring.document(f.docId);
    h.check(doc.isOk(), "document() after add ok");
    if (doc.isOk()) {
        h.check(sectionHasReq(doc.value(), f.sectionId, newId),
                "document shows the new requirement in section S");
    }

    // The requirement is traceable to the section (a link exists).
    h.check(hasActiveLinkTo(svc, newId, f.sectionId),
            "requirement is traceable to the section (Active link exists)");

    db.close();
    std::remove("lodestar_wp10_t3.db");
    std::remove("lodestar_wp10_t3.db-wal");
    std::remove("lodestar_wp10_t3.db-shm");
}

// ---------------------------------------------------------------------------
// T4. reorderRequirements() changes the order
// ---------------------------------------------------------------------------
void testReorder(Harness& h) {
    h.section("T4. reorderRequirements() changes the order");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp10_t4.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    DocFixture f = buildDoc(svc);

    auto r1 = svc.addEntity(makeReq("R1"));
    auto r2 = svc.addEntity(makeReq("R2"));
    svc.setParent(tl::EntityType::Requirement, r1.value().id, f.sectionId);
    svc.setParent(tl::EntityType::Requirement, r2.value().id, f.sectionId);

    // Reorder to [R2, R1].
    std::vector<std::string> order{r2.value().id, r1.value().id};
    auto re = wiring.reorderRequirements(f.docId, f.sectionId, order);
    h.check(re.isOk(), "reorderRequirements() ok");

    auto doc = wiring.document(f.docId);
    h.check(doc.isOk(), "document() after reorder ok");
    if (doc.isOk() && doc.value().sections.size() == 1) {
        const auto& reqs = doc.value().sections[0].requirements;
        h.check(reqs.size() == 2, "section still has 2 requirements");
        if (reqs.size() == 2) {
            h.check(reqs[0].id == r2.value().id && reqs[1].id == r1.value().id,
                    "requirements reordered to R2 then R1");
        }
    }

    db.close();
    std::remove("lodestar_wp10_t4.db");
    std::remove("lodestar_wp10_t4.db-wal");
    std::remove("lodestar_wp10_t4.db-shm");
}

// ---------------------------------------------------------------------------
// T5. Acceptance: authoring flow
// ---------------------------------------------------------------------------
void testAcceptance(Harness& h) {
    h.section("T5. Acceptance: authoring flow");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp10_t5.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    DocFixture f = buildDoc(svc);

    // Add two requirements via the document API.
    auto a1 = wiring.addRequirementToDocument(f.docId, f.sectionId, makeReq("A1"));
    auto a2 = wiring.addRequirementToDocument(f.docId, f.sectionId, makeReq("A2"));
    h.check(a1.isOk() && a2.isOk(), "add two requirements ok");
    const std::string a1Id = a1.value().id;
    const std::string a2Id = a2.value().id;

    // Reorder them.
    std::vector<std::string> order{a2Id, a1Id};
    h.check(wiring.reorderRequirements(f.docId, f.sectionId, order).isOk(),
            "reorder ok");

    // Re-query: document reflects the final order.
    auto doc = wiring.document(f.docId);
    h.check(doc.isOk(), "re-query document ok");
    if (doc.isOk() && doc.value().sections.size() == 1) {
        const auto& reqs = doc.value().sections[0].requirements;
        h.check(reqs.size() == 2, "section has 2 requirements");
        if (reqs.size() == 2) {
            h.check(reqs[0].id == a2Id && reqs[1].id == a1Id,
                    "final order is A2 then A1");
        }
    }

    // Each added requirement is traceable to S.
    h.check(hasActiveLinkTo(svc, a1Id, f.sectionId),
            "A1 traceable to section");
    h.check(hasActiveLinkTo(svc, a2Id, f.sectionId),
            "A2 traceable to section");

    // The model is stable across repeated calls (idempotent).
    auto doc2 = wiring.document(f.docId);
    h.check(doc2.isOk(), "second document() ok");
    if (doc2.isOk() && doc.value().sections.size() == 1 &&
        doc2.value().sections.size() == 1) {
        h.check(doc2.value().sections[0].requirements.size() ==
                    doc.value().sections[0].requirements.size(),
                "model stable across repeated calls");
        bool sameOrder = true;
        for (size_t i = 0; i < doc.value().sections[0].requirements.size(); ++i) {
            if (doc.value().sections[0].requirements[i].id !=
                doc2.value().sections[0].requirements[i].id) {
                sameOrder = false;
            }
        }
        h.check(sameOrder, "model order stable across repeated calls");
    }

    db.close();
    std::remove("lodestar_wp10_t5.db");
    std::remove("lodestar_wp10_t5.db-wal");
    std::remove("lodestar_wp10_t5.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-10 document-style authoring");
    std::printf("WP-10 DOCUMENT-STYLE AUTHORING TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testDocumentBuilds(h);
    testDocumentMissing(h);
    testAddRequirement(h);
    testReorder(h);
    testAcceptance(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
