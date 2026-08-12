#pragma once
// core/tracelink/ComplianceService.h
// WP-3 (Phase 10): guided OOTB compliance templates/checklists.
//
// Stores and tracks guided out-of-the-box templates/checklists for the four
// aerospace/avionics standards ARP4754A, ARP4761, DO-178C and DO-254. Each
// template carries an ordered checklist of items with guidance text and a
// per-item checked (progress) state. Progress is reported as checked/total
// plus a percentage.
//
// Contract written by the scrum-master in core/test/wp3_compliance_tests.cpp.

#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::tracelink {

// One compliance template (a standard, e.g. ARP4754A).
struct ComplianceTemplate {
    std::string id;
    std::string name;
    std::string description;
    std::string createdAt;
};

// One checklist item within a template.
struct ChecklistItem {
    std::string id;
    std::string templateId;
    int seq = 0;
    std::string title;
    std::string guidance;
    bool checked = false;
};

// Progress summary for a template.
struct ComplianceStatus {
    int total = 0;
    int checked = 0;
    int percent = 0;  // checked>0 ? (checked*100/total) : 0
};

class ComplianceService {
public:
    explicit ComplianceService(persistence::Database& db);

    // Seeds the four OOTB templates (ARP4754A, ARP4761, DO-178C, DO-254) with
    // their checklist items. Idempotent: safe to call any time.
    common::Result<void> seedTemplates();

    // All templates, ordered by name.
    common::Result<std::vector<ComplianceTemplate>> listTemplates();

    // A template with its checklist items (ordered by seq); nullopt if missing.
    common::Result<std::optional<ComplianceTemplate>> getTemplate(
        const std::string& id);

    // Checklist items for a template, ordered by seq.
    common::Result<std::vector<ChecklistItem>> checklistFor(
        const std::string& templateId);

    // Sets the checked state of one checklist item.
    common::Result<void> setChecked(const std::string& itemId, bool checked);

    // Progress for a template (checked/total).
    common::Result<ComplianceStatus> complianceStatus(
        const std::string& templateId);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
