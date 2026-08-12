#pragma once
// core/assurecheck/TemplateService.h
// S2 Phase 15 (AssureCheck): guided compliance templates/checklists.
//
// Provides out-of-the-box guided compliance templates for ARP4754A and
// DO-178C. Each template is tied to an assurance standard and provides a
// guided sequence of checklist items with a per-item status
// (pending | in_progress | complete) and overall progress tracking (0-100).
//
// Contract written by the scrum-master in docs/s2-phase15-test.md and
// core/test/s2_phase15_tests.cpp.

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::assurecheck {

// One guided compliance template.
struct GuidedTemplate {
    std::string id;          // UUID
    std::string name;        // ARP4754A | DO-178C
    std::string standard;    // tied assurance standard code
    std::string description;
};

// One guided checklist item within a template.
struct GuidedTemplateItem {
    std::string id;          // UUID
    std::string templateId;
    int seq = 0;
    std::string title;      // the guided step title
    std::string guidance;   // guidance for the step
    std::string status;     // pending | in_progress | complete
};

class TemplateService {
public:
    explicit TemplateService(persistence::Database& db);

    // Seeds the OOTB guided templates (ARP4754A, DO-178C) with their guided
    // checklist items. Idempotent.
    common::Result<void> seedTemplates();

    // All guided templates, ordered by name.
    common::Result<std::vector<GuidedTemplate>> listTemplates();

    // Guided checklist items for a template (by id), ordered by seq.
    common::Result<std::vector<GuidedTemplateItem>> templateChecklist(
        const std::string& templateId);

    // Fraction of items complete (0-100) for a template.
    common::Result<int> templateProgress(const std::string& templateId);

    // Marks a checklist item complete (pending/in_progress -> complete).
    common::Result<void> markComplete(const std::string& templateId,
                                      const std::string& itemId);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::assurecheck
