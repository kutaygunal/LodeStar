// core/scenario/errors/Troposphere.h
// Tropospheric delay model (Saastamoinen / Hopfield) (Item 5.3).

#pragma once

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class Troposphere {
public:
    enum class Model { Saastamoinen, Hopfield };

    void setModel(Model m);
    // Standard atmosphere: pressure (hPa), temperature (C), relative humidity (%).
    void setMeteo(double pressureHpa, double tempC, double humidityPct);

    // Tropospheric delay (meters) along the path from receiver to satellite.
    Result<double> delay(const SvState& sv, const Vec3& rxEcef) const;

private:
    double saastamoinen(double elRad) const;
    double hopfield(double elRad, double latRad) const;

    Model model_ = Model::Saastamoinen;
    double p_ = 1013.25;   // hPa
    double T_ = 15.0;      // C
    double H_ = 50.0;      // %
};

}  // namespace lodestar::scenario
