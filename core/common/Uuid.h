#pragma once
// core/common/Uuid.h
// Generates a random UUID v4 string used as the primary key for core entities.

#include <string>

namespace lodestar::common {

std::string newUuid();

}  // namespace lodestar::common
