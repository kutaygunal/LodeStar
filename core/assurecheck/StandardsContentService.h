// core/assurecheck/StandardsContentService.h
// Gap-Fill AssureCheck 2.2: vetted standards-content library.
//
// Curates DO-178C/DO-254/ARP4754A/ARP4761/DO-278A objectives into a
// structured, versioned content bundle (objectives + sub-objectives, DAL
// applicability, evidence requirements) loaded from data files rather than
// code, so content can be updated without a rebuild.
//
// Provides a loader + validator + DAL A-E mapping used by the SAS/PSAC
// generator (2.1) and the live coverage view (2.3).

#pragma once

#include <string>
#include <vector>

#include "core/common/Result.h"

namespace lodestar::assurecheck {

// One sub-objective (the leaf checklist item).
struct StandardSubObjective {
    std::string code;
    std::string objective;
    std::string dal;       // e.g. "A-D", "A-C", "A"
    std::string evidence;  // required evidence
};

// One objective (a grouping with sub-objectives).
struct StandardObjective {
    std::string code;      // e.g. "A1"
    std::string title;
    std::string dalApplicability;
    std::vector<StandardSubObjective> subObjectives;
};

// A versioned standard bundle as loaded from a data file.
struct StandardBundle {
    std::string schemaVersion;
    std::string bundleVersion;
    std::string standardCode;  // DO-178C | ...
    std::string name;
    std::vector<StandardObjective> objectives;
};

class StandardsContentService {
public:
    // Load + validate a versioned bundle from a JSON data file. Returns the
    // validated bundle.
    common::Result<StandardBundle> loadBundle(const std::string& path) const;

    // Pure validation: structural + version checks. Returns an error listing
    // problems (empty error = valid).
    common::Result<void> validateBundle(const StandardBundle& bundle) const;

    // Whether a sub-objective with `dal` applies to the project DAL level
    // `projectDal` (A-E). Pure function, unit-tested at every boundary.
    static bool appliesToDal(const std::string& range, const std::string& projectDal);

    // All leaf sub-objectives across the bundle (flattened), used by the
    // coverage view and SAS generator.
    static std::vector<StandardSubObjective> flatten(const StandardBundle& bundle);
};

}  // namespace lodestar::assurecheck
