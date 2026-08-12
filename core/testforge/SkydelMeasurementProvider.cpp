// core/testforge/SkydelMeasurementProvider.cpp
#include "core/testforge/SkydelMeasurementProvider.h"

namespace lodestar::testforge {

common::Result<std::optional<double>> SkydelMeasurementProvider::measure(
    const std::string& metric) {
    lodestar::Json params = lodestar::Json::object();
    params["metric"] = lodestar::Json::string(metric);

    lodestar::Json resp;
    try {
        resp = adapter_.invoke("measure", params);
    } catch (const adapters::AdapterError& e) {
        return common::Result<std::optional<double>>::err(
            "skydel measure failed: " + std::string(e.what()));
    }

    // Extract the numeric measurement from the vendor response.
    if (!resp.has("vendor")) {
        return common::Result<std::optional<double>>::ok(std::nullopt);
    }
    const lodestar::Json& vendor = resp.at("vendor");
    if (!vendor.isObject() || !vendor.has("value") || !vendor.at("value").isNumber()) {
        return common::Result<std::optional<double>>::ok(std::nullopt);
    }
    return common::Result<std::optional<double>>::ok(vendor.at("value").asNumber());
}

}  // namespace lodestar::testforge
