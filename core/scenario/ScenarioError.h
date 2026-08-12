// core/scenario/ScenarioError.h
// Shared error type for ScenarioForge (Phase 4).
//
// ScenarioError is a std::runtime_error subclass carrying a machine-readable
// error code plus a human-readable message. Every public entry point that can
// fail returns a Result<T> (see Types.h) rather than throwing, except for
// low-level range-check helpers that throw ScenarioError directly (documented
// per function). No silent NaN propagation (R8).

#pragma once

#include <stdexcept>
#include <string>

namespace lodestar::scenario {

enum class ErrorCode {
    None = 0,
    InvalidArgument,     // out-of-range / malformed input
    NonConvergence,      // iterative solver did not converge
    ParseError,          // RINEX / TLE / SBAS parse failure
    CrcFailure,          // SBAS CRC mismatch
    InsufficientSats,    // <4 pseudoranges for PVT
    SingularGeometry,    // geometry matrix not invertible
    MissingData,         // required correction / IGP / ephemeris missing
    StaleData,           // correction age exceeded
    Unsupported,         // feature not implemented (e.g. deep-space SDP4)
    Internal             // unexpected internal error
};

class ScenarioError : public std::runtime_error {
public:
    ScenarioError(ErrorCode code, const std::string& message)
        : std::runtime_error(message), code_(code) {}

    ErrorCode code() const { return code_; }
    const char* codeName() const { return codeName(code_); }

    static const char* codeName(ErrorCode c) {
        switch (c) {
            case ErrorCode::None: return "None";
            case ErrorCode::InvalidArgument: return "InvalidArgument";
            case ErrorCode::NonConvergence: return "NonConvergence";
            case ErrorCode::ParseError: return "ParseError";
            case ErrorCode::CrcFailure: return "CrcFailure";
            case ErrorCode::InsufficientSats: return "InsufficientSats";
            case ErrorCode::SingularGeometry: return "SingularGeometry";
            case ErrorCode::MissingData: return "MissingData";
            case ErrorCode::StaleData: return "StaleData";
            case ErrorCode::Unsupported: return "Unsupported";
            case ErrorCode::Internal: return "Internal";
        }
        return "Unknown";
    }

private:
    ErrorCode code_;
};

}  // namespace lodestar::scenario
