// core/scenario/sbas/Integrity.cpp
// Protection level computation (HPL/VPL) per RTCA DO-229 (Item 6.4).

#include "core/scenario/sbas/Integrity.h"

#include <cmath>

#include "core/scenario/ScenarioError.h"
#include "core/scenario/frames/Frames.h"

namespace lodestar::scenario {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kK_h = 6.18;   // K_HPL (95%, HPL = K_H * d_major)
constexpr double kK_v = 5.33;   // K_VPL (99.9%, VPL = K_V * d_U)

// Invert a 4x4 matrix in place; returns false if singular.
bool invert4(double m[4][4], double inv[4][4]) {
    double a[4][8];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) a[i][j] = m[i][j];
        for (int j = 0; j < 4; ++j) a[i][4 + j] = (i == j) ? 1.0 : 0.0;
    }
    for (int col = 0; col < 4; ++col) {
        // Pivot.
        int piv = col;
        for (int r = col + 1; r < 4; ++r) {
            if (std::fabs(a[r][col]) > std::fabs(a[piv][col])) piv = r;
        }
        if (std::fabs(a[piv][col]) < 1e-15) return false;
        if (piv != col) {
            for (int j = 0; j < 8; ++j) std::swap(a[col][j], a[piv][j]);
        }
        double pivVal = a[col][col];
        for (int j = 0; j < 8; ++j) a[col][j] /= pivVal;
        for (int r = 0; r < 4; ++r) {
            if (r == col) continue;
            double factor = a[r][col];
            for (int j = 0; j < 8; ++j) a[r][j] -= factor * a[col][j];
        }
    }
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) inv[i][j] = a[i][4 + j];
    return true;
}
}  // namespace

void Integrity::setUdre(int prn, double udre) { udre_[prn] = udre; }
void Integrity::setGive(double give) { give_ = give; haveGive_ = true; }

Result<ProtectionLevels> Integrity::compute(
    const std::vector<SatelliteView>& views,
    const std::vector<double>& udrePerPrn, double give) const {
    // Collect visible satellites.
    std::vector<const SatelliteView*> vis;
    for (const auto& v : views) if (v.visible) vis.push_back(&v);
    if (vis.size() < 4) {
        return Result<ProtectionLevels>::err(
            "Integrity: insufficient satellites for protection levels");
    }

    // Build the weighted geometry matrix G (rows = unit LOS), W = 1/sigma^2.
    const int n = static_cast<int>(vis.size());
    double G[16][4];
    double W[16];
    for (int i = 0; i < n; ++i) {
        // LOS from receiver (origin for protection-level geometry).
        Vec3 los = vis[i]->state.posEcef.normalized();
        G[i][0] = -los.x;
        G[i][1] = -los.y;
        G[i][2] = -los.z;
        G[i][3] = 1.0;
        double udre = (i < static_cast<int>(udrePerPrn.size())) ? udrePerPrn[i]
                                                                : udre_.at(vis[i]->prn);
        double sigma = std::sqrt(udre * udre + give * give);
        W[i] = 1.0 / (sigma * sigma);
    }

    // A = G^T W G (4x4).
    double A[4][4];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            double s = 0.0;
            for (int i = 0; i < n; ++i)
                s += G[i][r] * W[i] * G[i][c];
            A[r][c] = s;
        }

    double Ainv[4][4];
    if (!invert4(A, Ainv)) {
        return Result<ProtectionLevels>::err(
            "Integrity: singular geometry matrix");
    }

    // d_major = sqrt( largest eigenvalue of the horizontal 2x2 of (G^T W G)^-1 ).
    // Use the largest diagonal + off-diagonal combination as an approximation.
    double a = Ainv[0][0];
    double b = Ainv[0][1] + Ainv[1][0];
    double c = Ainv[1][1];
    double trace = a + c;
    double det = a * c - (Ainv[0][1] * Ainv[1][0]);
    double disc = std::sqrt(std::max(0.0, trace * trace - 4.0 * det));
    double dMajorSq = (trace + disc) / 2.0;
    double dU = std::sqrt(std::max(0.0, Ainv[2][2]));

    ProtectionLevels pl;
    pl.hpl = kK_h * std::sqrt(dMajorSq);
    pl.vpl = kK_v * dU;
    pl.valid = std::isfinite(pl.hpl) && std::isfinite(pl.vpl);
    return Result<ProtectionLevels>::ok(pl);
}

}  // namespace lodestar::scenario
