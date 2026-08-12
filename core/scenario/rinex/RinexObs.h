// core/scenario/rinex/RinexObs.h
// RINEX 3.x observation file parser (Item 2.2).

#pragma once

#include <istream>
#include <string>
#include <vector>

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class RinexObsParser {
public:
    explicit RinexObsParser(std::istream& in);

    Result<RinexObsHeader> readHeader();
    Result<ObsEpoch> nextEpoch();  // Result::eof() at end of file

    const std::vector<std::string>& warnings() const { return warnings_; }

private:
    std::istream& in_;
    std::vector<std::string> warnings_;
    bool headerDone_ = false;
    int line_ = 0;
    std::vector<std::string> obsTypes_;
};

}  // namespace lodestar::scenario
