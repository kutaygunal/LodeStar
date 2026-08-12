// core/scenario/pvt/Solver.h
// Weighted least-squares PVT solver (Item 7, R3).

#pragma once

#include <vector>

#include "core/scenario/Types.h"

namespace lodestar::scenario {

// Compute receiver position/velocity and clock state from ≥4 pseudoranges.
// Each measurement provides a satellite state and a measured pseudorange.
class PvSolver {
public:
    struct Measurement {
        SvState sv;
        double pseudorange;  // m
        double weight = 1.0; // 1/sigma^2
    };

    // Iterate the weighted least-squares linearization until convergence or
    // max iterations. Returns an error for insufficient measurements or
    // non-convergence.
    static Result<PvtResult> solve(const std::vector<Measurement>& meas,
                                   int maxIter = 15);
};

}  // namespace lodestar::scenario
