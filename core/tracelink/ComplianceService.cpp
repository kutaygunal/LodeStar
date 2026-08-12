#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// core/tracelink/ComplianceService.cpp
// WP-3 (Phase 10): guided OOTB compliance templates/checklists.

#include "core/tracelink/ComplianceService.h"

#include <ctime>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "core/common/Uuid.h"

namespace lodestar::tracelink {

using lodestar::common::newUuid;

namespace {

std::string now() {
    char buf[32];
    const auto t = std::time(nullptr);
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return buf;
}

void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

void bindInt(sqlite3_stmt* stmt, int index, int value) {
    sqlite3_bind_int(stmt, index, value);
}

std::string colText(sqlite3_stmt* stmt, int col) {
    const unsigned char* t = sqlite3_column_text(stmt, col);
    return t ? reinterpret_cast<const char*>(t) : std::string();
}

int colInt(sqlite3_stmt* stmt, int col) {
    return sqlite3_column_int(stmt, col);
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
    return common::Result<std::vector<std::vector<std::string>>>::ok(std::move(out));
}

// One OOTB template definition: name, description, and its checklist items.
struct SeedTemplate {
    const char* name;
    const char* description;
    std::vector<std::pair<const char*, const char*>> items;  // (title, guidance)
};

// The four guided OOTB templates. Each item is (title, guidance).
const std::vector<SeedTemplate>& seedData() {
    static const std::vector<SeedTemplate> data = {
        {"ARP4754A",
         "Development of civil aircraft and systems (guidelines for system "
         "development, requirements, and certification).",
         {
             {"Define aircraft/system development plan",
              "Establish the development plan covering the system lifecycle, "
              "phases, and responsibilities."},
             {"Capture system requirements",
              "Derive and document system-level requirements from the aircraft "
              "functions and certification basis."},
             {"Allocate requirements to items",
              "Allocate system requirements to hardware/software items and "
              "maintain traceability."},
             {"Perform safety assessment",
              "Conduct the system safety assessment and feed results into the "
              "development process."},
             {"Verify requirements",
              "Verify each requirement is met by the implementing item and "
              "record evidence."},
             {"Manage configuration",
              "Control configuration of the system and its items throughout "
              "development."},
         }},
        {"ARP4761",
         "Guidelines and methods for conducting the safety assessment process "
         "on civil airborne systems and equipment.",
         {
             {"Functional hazard assessment (FHA)",
              "Identify and classify the effects of system/function failures "
              "on the aircraft."},
             {"Preliminary system safety assessment (PSSA)",
              "Allocate safety requirements and derive failure conditions for "
              "the system architecture."},
             {"System safety assessment (SSA)",
              "Verify the implemented system meets the safety requirements."},
             {"Common cause analysis (CCA)",
              "Analyze common-mode and common-cause failures across the "
              "system."},
             {"Fault tree analysis (FTA)",
              "Model and quantify failure paths leading to top-level failure "
              "conditions."},
             {"Failure modes and effects analysis (FMEA)",
              "Identify component failure modes and their effects on the "
              "system."},
         }},
        {"DO-178C",
         "Software considerations in airborne systems and equipment "
         "certification.",
         {
             {"Define software level",
              "Determine the software level (A-E) from the failure condition "
              "classification."},
             {"Establish software lifecycle data",
              "Define the software planning documents and lifecycle data "
              "required for the level."},
             {"Develop high-level requirements",
              "Derive high-level software requirements from system "
              "requirements."},
             {"Develop low-level requirements",
              "Derive low-level software requirements and architecture from "
              "high-level requirements."},
             {"Verify software requirements",
              "Verify the software satisfies its requirements with the "
              "required independence."},
             {"Perform configuration management",
              "Control software baselines, changes, and problem reporting."},
             {"Produce software accomplishment summary",
              "Summarize the software lifecycle data and compliance for "
              "certification."},
         }},
        {"DO-254",
         "Design assurance guidance for airborne electronic hardware.",
         {
             {"Define hardware level",
              "Determine the hardware design assurance level from the failure "
              "condition classification."},
             {"Establish hardware lifecycle data",
              "Define the hardware planning documents and lifecycle data."},
             {"Develop hardware requirements",
              "Derive hardware requirements from system requirements and "
              "architecture."},
             {"Develop hardware design",
              "Develop the hardware design and implementation from the "
              "requirements."},
             {"Verify hardware",
              "Verify the hardware satisfies its requirements with the "
              "required independence."},
             {"Perform configuration management",
              "Control hardware baselines, changes, and problem reporting."},
             {"Produce hardware accomplishment summary",
              "Summarize the hardware lifecycle data and compliance for "
              "certification."},
         }},
    };
    return data;
}

}  // namespace

ComplianceService::ComplianceService(persistence::Database& db) : db_(db) {}

common::Result<void> ComplianceService::seedTemplates() {
    // Idempotent: if the four templates already exist, do nothing.
    auto existing = query(db_.handle(),
                          "SELECT COUNT(*) FROM compliance_templates;", {}, 1);
    if (existing.failed()) {
        return common::Result<void>::err(existing.error());
    }
    if (!existing.value().empty() && existing.value().front()[0] != "0") {
        return common::Result<void>::ok();
    }

    const std::string ts = now();
    for (const auto& t : seedData()) {
        const std::string tid = newUuid();
        auto ins = exec(db_.handle(),
                        "INSERT INTO compliance_templates "
                        "(id, name, description, created_at) VALUES (?,?,?,?);",
                        {tid, t.name, t.description, ts});
        if (ins.failed()) return ins;

        int seq = 0;
        for (const auto& item : t.items) {
            const std::string iid = newUuid();
            auto ires = exec(db_.handle(),
                             "INSERT INTO compliance_checklist_items "
                             "(id, template_id, seq, title, guidance, checked) "
                             "VALUES (?,?,?,?,?,0);",
                             {iid, tid, std::to_string(seq), item.first,
                              item.second});
            if (ires.failed()) return ires;
            ++seq;
        }
    }
    return common::Result<void>::ok();
}

common::Result<std::vector<ComplianceTemplate>>
ComplianceService::listTemplates() {
    auto rows = query(db_.handle(),
                      "SELECT id, name, description, created_at "
                      "FROM compliance_templates ORDER BY name;",
                      {}, 4);
    if (rows.failed()) {
        return common::Result<std::vector<ComplianceTemplate>>::err(rows.error());
    }
    std::vector<ComplianceTemplate> out;
    for (const auto& r : rows.value()) {
        ComplianceTemplate t;
        t.id = r[0];
        t.name = r[1];
        t.description = r[2];
        t.createdAt = r[3];
        out.push_back(std::move(t));
    }
    return common::Result<std::vector<ComplianceTemplate>>::ok(std::move(out));
}

common::Result<std::optional<ComplianceTemplate>>
ComplianceService::getTemplate(const std::string& id) {
    auto rows = query(db_.handle(),
                      "SELECT id, name, description, created_at "
                      "FROM compliance_templates WHERE id=?;",
                      {id}, 4);
    if (rows.failed()) {
        return common::Result<std::optional<ComplianceTemplate>>::err(rows.error());
    }
    if (rows.value().empty()) {
        return common::Result<std::optional<ComplianceTemplate>>::ok(
            std::optional<ComplianceTemplate>());
    }
    const auto& r = rows.value().front();
    ComplianceTemplate t;
    t.id = r[0];
    t.name = r[1];
    t.description = r[2];
    t.createdAt = r[3];
    return common::Result<std::optional<ComplianceTemplate>>::ok(
        std::optional<ComplianceTemplate>(std::move(t)));
}

common::Result<std::vector<ChecklistItem>> ComplianceService::checklistFor(
    const std::string& templateId) {
    auto rows = query(db_.handle(),
                      "SELECT id, template_id, seq, title, guidance, checked "
                      "FROM compliance_checklist_items "
                      "WHERE template_id=? ORDER BY seq;",
                      {templateId}, 6);
    if (rows.failed()) {
        return common::Result<std::vector<ChecklistItem>>::err(rows.error());
    }
    std::vector<ChecklistItem> out;
    for (const auto& r : rows.value()) {
        ChecklistItem it;
        it.id = r[0];
        it.templateId = r[1];
        it.seq = std::atoi(r[2].c_str());
        it.title = r[3];
        it.guidance = r[4];
        it.checked = r[5] == "1";
        out.push_back(std::move(it));
    }
    return common::Result<std::vector<ChecklistItem>>::ok(std::move(out));
}

common::Result<void> ComplianceService::setChecked(const std::string& itemId,
                                                   bool checked) {
    auto res = exec(db_.handle(),
                    "UPDATE compliance_checklist_items SET checked=? WHERE id=?;",
                    {checked ? "1" : "0", itemId});
    if (res.failed()) return res;
    return common::Result<void>::ok();
}

common::Result<ComplianceStatus> ComplianceService::complianceStatus(
    const std::string& templateId) {
    auto rows = query(db_.handle(),
                      "SELECT COUNT(*), COALESCE(SUM(checked),0) "
                      "FROM compliance_checklist_items WHERE template_id=?;",
                      {templateId}, 2);
    if (rows.failed()) {
        return common::Result<ComplianceStatus>::err(rows.error());
    }
    ComplianceStatus st;
    if (!rows.value().empty()) {
        st.total = std::atoi(rows.value().front()[0].c_str());
        st.checked = std::atoi(rows.value().front()[1].c_str());
    }
    st.percent = st.checked > 0 ? (st.checked * 100 / st.total) : 0;
    return common::Result<ComplianceStatus>::ok(st);
}

}  // namespace lodestar::tracelink
