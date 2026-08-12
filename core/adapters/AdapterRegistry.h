// core/adapters/AdapterRegistry.h
// Registry that holds named IAdapter instances and looks them up by name
// (Phase 5, P5-1.1). Looking up an unknown name throws AdapterError::Unsupported.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/adapters/Adapter.h"

namespace lodestar::adapters {

class AdapterRegistry {
public:
    // Register an adapter under its IAdapter::name(). Re-registering an existing
    // name replaces the previous instance.
    void registerAdapter(std::shared_ptr<IAdapter> adapter);

    // Look up an adapter by name. Throws AdapterError(Unsupported) when the name
    // is not registered.
    std::shared_ptr<IAdapter> get(const std::string& name) const;

    // Look up without throwing; returns nullptr when unknown.
    std::shared_ptr<IAdapter> getOrNull(const std::string& name) const;

    // True when an adapter with the given name is registered.
    bool contains(const std::string& name) const;

    // All registered adapter names (sorted).
    std::vector<std::string> names() const;

    std::size_t size() const { return adapters_.size(); }

private:
    std::vector<std::shared_ptr<IAdapter>> adapters_;
};

}  // namespace lodestar::adapters
