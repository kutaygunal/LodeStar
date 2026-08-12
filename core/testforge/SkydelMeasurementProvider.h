#pragma once
// core/testforge/SkydelMeasurementProvider.h
// IMeasurementProvider backed by the Skydel RF adapter (S2 Phase 6).
//
// measure() drives the adapter via SkydelAdapter::invoke("measure", {metric})
// and returns the numeric value the adapter reports. In simulate mode (CI) the
// adapter returns a deterministic value without hardware, so a TestForge run can
// exercise the full RF-injection path end-to-end.

#include <string>

#include "core/adapters/SkydelAdapter.h"
#include "core/common/Result.h"
#include "core/testforge/TestRunner.h"

namespace lodestar::testforge {

class SkydelMeasurementProvider : public IMeasurementProvider {
public:
    explicit SkydelMeasurementProvider(adapters::SkydelAdapter& adapter)
        : adapter_(adapter) {}

    // Returns the measured value for a named metric, or nullopt if the adapter
    // did not report a numeric value (which marks the step Blocked). A hard
    // adapter failure (e.g. network) is returned as a failed Result.
    common::Result<std::optional<double>> measure(const std::string& metric) override;

private:
    adapters::SkydelAdapter& adapter_;
};

}  // namespace lodestar::testforge
