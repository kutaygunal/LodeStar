// core/scenario/rinex/RinexNav.h
// RINEX 3.x navigation (broadcast ephemeris) parser (Item 2.1).

#pragma once

#include <istream>
#include <string>
#include <vector>

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class RinexNavParser {
public:
    explicit RinexNavParser(std::istream& in);

    // Read and validate the header. Returns the parsed header.
    Result<RinexNavHeader> readHeader();

    // Read the next broadcast ephemeris record. Returns Result::eof() at
    // end-of-file, a parsed ephemeris on success, or an error.
    Result<BroadcastEphemeris> nextEphemeris();

    const std::vector<std::string>& warnings() const { return warnings_; }

private:
    std::istream& in_;
    std::vector<std::string> warnings_;
    bool headerDone_ = false;
    int line_ = 0;
};

}  // namespace lodestar::scenario
