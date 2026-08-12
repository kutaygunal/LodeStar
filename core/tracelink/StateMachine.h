#pragma once
// core/tracelink/StateMachine.h
// Lifecycle state machines per entity type with legal transition validation.
// The service rejects illegal transitions (e.g. Draft -> Verified).

#include <string>
#include <vector>

#include "core/tracelink/Types.h"

namespace lodestar::tracelink {

// All legal statuses for the given entity type, in canonical order.
std::vector<std::string> statuses(EntityType t);

// Whether `status` is a legal state for the given entity type.
bool isValidStatus(EntityType t, const std::string& status);

// Whether moving `from` -> `to` is a permitted transition. Any state may move
// to "Obsolete".
bool canTransition(EntityType t, const std::string& from, const std::string& to);

// A human-readable description of why the transition is illegal (empty if legal).
std::string transitionError(EntityType t, const std::string& from,
                            const std::string& to);

}  // namespace lodestar::tracelink
