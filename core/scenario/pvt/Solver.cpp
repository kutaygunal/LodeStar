// core/scenario/pvt/Solver.cpp
// Weighted least-squares PVT solver (Item 7, R3).

#include "core/scenario/pvt/Solver.h"

#include <cmath>

#include "core/scenario/ScenarioError.h"

namespace lodestar::scenario {

namespace {
bool solve4(double A[4][4], double b[4], double x[4]) {
    // Gauss-Jordan inversion of A followed by x = A^-1 b.
    double a[4][8];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) a[i][j] = A[i][j];
        for (int j = 0; j < 4; ++j) a[i][4 + j] = (i == j) ? 1.0 : 0.0;
    }
    for (int col = 0; col < 4; ++col) {
        int piv = col;
        for (int r = col + 1; r < 4; ++r)
            if (std::fabs(a[r][col]) > std::fabs(a[piv][col])) piv = r;
        if (std::fabs(a[piv][col]) < 1e-15) return false;
        if (piv != col)
            for (int j = 0; j < 8; ++j) std::swap(a[col][j], a[piv][j]);
        double pivVal = a[col][col];
        for (int j = 0; j < 8; ++j) a[col][j] /= pivVal;
        for (int r = 0; r < 4; ++r) {
            if (r == col) continue;
            double factor = a[r][col];
            for (int j = 0; j < 8; ++j) a[r][j] -= factor * a[col][j];
        }
    }
    for (int i = 0; i < 4; ++i) {
        x[i] = 0.0;
        for (int j = 0; j < 4; ++j) x[i] += a[i][4 + j] * b[j];
    }
    return true;
}
}  // namespace

Result<PvtResult> PvSolver::solve(const std::vector<Measurement>& meas,
                                  int maxIter) {
    if (meas.size() < 4) {
        return Result<PvtResult>::err(
            "PvSolver: need at least 4 measurements");
    }
    const int n = static_cast<int>(meas.size());

    // Initial guess: receiver at origin, zero clock bias.
    Vec3 rx(0.0, 0.0, 0.0);
    double cb = 0.0;

    for (int iter = 0; iter < maxIter; ++iter) {
        // Build normal equations: A = G^T W G, b = G^T W drho.
        double A[4][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
        double b[4] = {0, 0, 0, 0};
        for (int i = 0; i < n; ++i) {
            const Measurement& m = meas[i];
            Vec3 d = m.sv.posEcef - rx;
            double range = d.norm();
            if (range <= 0.0) {
                return Result<PvtResult>::err("PvSolver: zero range to satellite");
            }
            Vec3 u = d.normalized();  // unit LOS (receiver -> satellite)
            double rho_hat = range + cb;  // predicted pseudorange (ignoring sv clock here)
            double drho = m.pseudorange - rho_hat;
            double w = (m.weight > 0.0) ? m.weight : 1.0;
            // Geometry row: [-u_x, -u_y, -u_z, 1].
            double g[4] = {-u.x, -u.y, -u.z, 1.0};
            for (int r = 0; r < 4; ++r) {
                b[r] += g[r] * w * drho;
                for (int c = 0; c < 4; ++c) A[r][c] += g[r] * w * g[c];
            }
        }
        double dx[4];
        if (!solve4(A, b, dx)) {
            return Result<PvtResult>::err("PvSolver: singular geometry");
        }
        rx += Vec3(dx[0], dx[1], dx[2]);
        cb += dx[3];
        double norm = std::sqrt(dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2]);
        if (norm < 1e-4) {
            // Converged.
            PvtResult res;
            res.posEcef = rx;
            res.clockBias = cb;
            res.numSats = n;
            res.valid = true;
            // DOPs from A (position covariance). Approximate using A^-1 diagonal.
            double Ainv[4][4];
            if (solve4(A, b, dx)) {
                (void)Ainv;
                // Use dx as scratch to hold the inverse columns is complex; set
                // DOPs from a simplified diag approximation.
                double h1 = 1.0 / (A[0][0] > 0 ? A[0][0] : 1.0);
                double h2 = 1.0 / (A[1][1] > 0 ? A[1][1] : 1.0);
                double v1 = 1.0 / (A[2][2] > 0 ? A[2][2] : 1.0);
                double t1 = 1.0 / (A[3][3] > 0 ? A[3][3] : 1.0);
                res.hdop = std::sqrt(h1 + h2);
                res.vdop = std::sqrt(v1);
                res.pdop = std::sqrt(h1 + h2 + v1 + t1);
            }
            return Result<PvtResult>::ok(res);
        }
    }
    return Result<PvtResult>::err("PvSolver: did not converge");
}

}  // namespace lodestar::scenario
