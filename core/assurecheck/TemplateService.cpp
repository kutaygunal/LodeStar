// core/assurecheck/TemplateService.cpp
// S2 Phase 15 (AssureCheck): guided compliance templates/checklists
// implementation.

#include "core/assurecheck/TemplateService.h"

#include <cstdlib>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "core/common/Uuid.h"

namespace lodestar::assurecheck {

using lodestar::common::newUuid;

namespace {

void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

std::string colText(sqlite3_stmt* stmt, int col) {
    const unsigned char* t = sqlite3_column_text(stmt, col);
    return t ? reinterpret_cast<const char*>(t) : std::string();
}

common::Result<void> exec(sqlite3* db, const std::string& sql,
                          const std::vector<std::string>& params = {}) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err("prepare failed: " +
                                         std::string(sqlite3_errmsg(db)));
    }
    for (size_t i = 0; i < params.size(); ++i) {
        bindText(stmt, static_cast<int>(i + 1), params[i]);
    }
    int rc = sqlite3_step(stmt);
    std::string msg = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err("step failed: " + msg);
    }
    return common::Result<void>::ok();
}

common::Result<std::vector<std::vector<std::string>>> query(
    sqlite3* db, const std::string& sql, const std::vector<std::string>& params,
    int ncols) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::vector<std::vector<std::string>>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db)));
    }
    for (size_t i = 0; i < params.size(); ++i) {
        bindText(stmt, static_cast<int>(i + 1), params[i]);
    }
    std::vector<std::vector<std::string>> out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::vector<std::string> row;
        for (int c = 0; c < ncols; ++c) row.push_back(colText(stmt, c));
        out.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<std::vector<std::string>>>::ok(
        std::move(out));
}

struct SeedStep {
    const char* title;
    const char* guidance;
};

struct SeedTemplateDef {
    const char* name;
    const char* standard;
    const char* description;
    std::vector<SeedStep> steps;
};

const std::vector<SeedTemplateDef>& seedData() {
    static const std::vector<SeedTemplateDef> data = {
        {"ARP4754A",
         "ARP4754A",
         "Guided compliance through the ARP4754A development assurance "
         "process (V-cycle) for civil aircraft and systems.",
         {
             {"Establish development planning",
              "Define the development plan, the development assurance level "
              "assignment, and the planning data required by ARP4754A."},
             {"Assign development assurance level",
              "Assign the Development Assurance Level (DAL) to each function "
              "based on the failure condition classification."},
             {"Define aircraft/system functions",
              "Capture the aircraft and system functions that the system must "
              "provide."},
             {"Develop system requirements",
              "Derive system requirements from the aircraft/system functions."},
             {"Validate system requirements",
              "Validate that the system requirements are correct, complete, "
              "and consistent."},
             {"Develop system architecture",
              "Develop the system architecture and show it is consistent with "
              "the system requirements."},
             {"Develop system implementation",
              "Develop the system implementation (hardware/software) from the "
              "architecture."},
             {"Verify system requirements",
              "Verify that the implemented system satisfies the system "
              "requirements."},
             {"Integrate safety assessment",
              "Integrate the safety assessment (ARP4761) with the development "
              "process."},
             {"Apply configuration management",
              "Apply configuration management to all development data."},
             {"Apply process assurance",
              "Apply process assurance to the development and verification "
              "activities."},
             {"Produce certification evidence",
              "Produce the certification evidence required for the system."},
             {"Trace requirements through the V-cycle",
              "Maintain traceability of requirements through the V-cycle from "
              "aircraft functions to verification."},
         }},
        {"DO-178C",
         "DO-178C",
         "Guided compliance through the DO-178C software life cycle for "
         "airborne systems and equipment certification.",
         {
             {"Define software life cycle processes",
              "Define the software life cycle processes and the plan for "
              "software aspects of certification (PSAC)."},
             {"Define software life cycle data",
              "Define the software life cycle data and the software "
              "development plan (SDP)."},
             {"Define software life cycle environment",
              "Define the software life cycle environment used for "
              "development and verification."},
             {"Define software development standards",
              "Define the software development standards (coding, design, "
              "requirements)."},
             {"Develop high-level requirements",
              "Develop high-level requirements that are verifiable and "
              "traceable to system requirements."},
             {"Develop low-level requirements",
              "Develop low-level requirements that are verifiable and "
              "traceable to high-level requirements."},
             {"Develop software architecture",
              "Develop the software architecture and show consistency with "
              "the high-level requirements."},
             {"Develop source code",
              "Develop source code that is traceable to and consistent with "
              "the low-level requirements."},
             {"Build executable object code",
              "Build the executable object code and show traceability to the "
              "source code."},
             {"Verify high-level requirements",
              "Verify the high-level requirements (accuracy, completeness, "
              "verifiability, consistency)."},
             {"Verify low-level requirements",
              "Verify the low-level requirements (accuracy, completeness, "
              "verifiability, consistency)."},
             {"Verify software architecture",
              "Verify the software architecture (compatibility, consistency, "
              "partitioning)."},
             {"Verify source code",
              "Verify the source code (accuracy, completeness, standards "
              "compliance, partitioning)."},
             {"Test the integration process",
              "Test the executable object code against the high-level and "
              "low-level requirements."},
             {"Achieve structural coverage",
              "Achieve statement, decision, and MC/DC coverage as required "
              "by the DAL."},
             {"Verify verification results",
              "Verify that the verification process results are correct, "
              "complete, and consistent."},
             {"Apply configuration management",
              "Apply configuration management to the software life cycle "
              "data."},
             {"Apply software quality assurance",
              "Apply software quality assurance (process assurance) to the "
              "software life cycle."},
             {"Establish certification liaison",
              "Establish certification liaison and produce the certification "
              "evidence."},
         }},
    };
    return data;
}

}  // namespace

TemplateService::TemplateService(persistence::Database& db) : db_(db) {}

common::Result<void> TemplateService::seedTemplates() {
    // Idempotent: if the two OOTB templates already exist, do nothing.
    auto existing = query(db_.handle(),
                          "SELECT COUNT(*) FROM guided_templates;", {}, 1);
    if (existing.failed()) {
        return common::Result<void>::err(existing.error());
    }
    if (!existing.value().empty() && existing.value().front()[0] != "0") {
        return common::Result<void>::ok();
    }

    for (const auto& t : seedData()) {
        const std::string tid = newUuid();
        auto ins = exec(db_.handle(),
                        "INSERT INTO guided_templates "
                        "(id, name, standard, description) VALUES (?,?,?,?);",
                        {tid, t.name, t.standard, t.description});
        if (ins.failed()) return ins;

        int seq = 0;
        for (const auto& step : t.steps) {
            const std::string iid = newUuid();
            auto ires = exec(db_.handle(),
                             "INSERT INTO guided_template_items "
                             "(id, template_id, seq, title, guidance, status) "
                             "VALUES (?,?,?,?,?,?);",
                             {iid, tid, std::to_string(seq), step.title,
                              step.guidance, "pending"});
            if (ires.failed()) return ires;
            ++seq;
        }
    }
    return common::Result<void>::ok();
}

common::Result<std::vector<GuidedTemplate>> TemplateService::listTemplates() {
    auto rows = query(db_.handle(),
                      "SELECT id, name, standard, description "
                      "FROM guided_templates ORDER BY name;",
                      {}, 4);
    if (rows.failed()) {
        return common::Result<std::vector<GuidedTemplate>>::err(rows.error());
    }
    std::vector<GuidedTemplate> out;
    for (const auto& r : rows.value()) {
        GuidedTemplate t;
        t.id = r[0];
        t.name = r[1];
        t.standard = r[2];
        t.description = r[3];
        out.push_back(std::move(t));
    }
    return common::Result<std::vector<GuidedTemplate>>::ok(std::move(out));
}

common::Result<std::vector<GuidedTemplateItem>>
TemplateService::templateChecklist(const std::string& templateId) {
    auto rows = query(db_.handle(),
                      "SELECT id, template_id, seq, title, guidance, status "
                      "FROM guided_template_items WHERE template_id=? "
                      "ORDER BY seq;",
                      {templateId}, 6);
    if (rows.failed()) {
        return common::Result<std::vector<GuidedTemplateItem>>::err(
            rows.error());
    }
    std::vector<GuidedTemplateItem> out;
    for (const auto& r : rows.value()) {
        GuidedTemplateItem it;
        it.id = r[0];
        it.templateId = r[1];
        it.seq = std::atoi(r[2].c_str());
        it.title = r[3];
        it.guidance = r[4];
        it.status = r[5];
        out.push_back(std::move(it));
    }
    return common::Result<std::vector<GuidedTemplateItem>>::ok(std::move(out));
}

common::Result<int> TemplateService::templateProgress(
    const std::string& templateId) {
    auto rows = query(db_.handle(),
                      "SELECT COUNT(*), "
                      "SUM(CASE WHEN status='complete' THEN 1 ELSE 0 END) "
                      "FROM guided_template_items WHERE template_id=?;",
                      {templateId}, 2);
    if (rows.failed()) {
        return common::Result<int>::err(rows.error());
    }
    if (rows.value().empty()) {
        return common::Result<int>::err("template not found: " + templateId);
    }
    const int total = std::atoi(rows.value().front()[0].c_str());
    const int complete = std::atoi(rows.value().front()[1].c_str());
    if (total <= 0) return common::Result<int>::ok(0);
    return common::Result<int>::ok(complete * 100 / total);
}

common::Result<void> TemplateService::markComplete(
    const std::string& templateId, const std::string& itemId) {
    auto up = exec(db_.handle(),
                   "UPDATE guided_template_items SET status='complete' "
                   "WHERE id=? AND template_id=?;",
                   {itemId, templateId});
    if (up.failed()) return up;
    return common::Result<void>::ok();
}

}  // namespace lodestar::assurecheck
