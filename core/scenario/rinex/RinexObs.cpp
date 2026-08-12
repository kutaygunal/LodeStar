// core/scenario/rinex/RinexObs.cpp
// RINEX 3.x observation file parser (Item 2.2). Handles epoch records and
// per-satellite observation decoding with warning/skip for unknown codes.

#include "core/scenario/rinex/RinexObs.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>

#include "core/scenario/ScenarioError.h"
#include "core/scenario/frames/TimeSystem.h"

namespace lodestar::scenario {

namespace {
std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

double parseFieldDouble(const std::string& line, int col, int width) {
    if (col + width > static_cast<int>(line.size())) return 0.0;
    std::string f = trim(line.substr(col, width));
    if (f.empty()) return 0.0;
    return std::atof(f.c_str());
}

int parseFieldInt(const std::string& line, int col, int width) {
    if (col + width > static_cast<int>(line.size())) return 0;
    std::string f = trim(line.substr(col, width));
    if (f.empty()) return 0;
    return std::atoi(f.c_str());
}
}  // namespace

RinexObsParser::RinexObsParser(std::istream& in) : in_(in) {}

Result<RinexObsHeader> RinexObsParser::readHeader() {
    RinexObsHeader hdr;
    std::string line;
    if (!std::getline(in_, line)) {
        return Result<RinexObsHeader>::err("RinexObs: empty file");
    }
    line_++;
    hdr.version = trim(line.substr(0, 9));
    if (line.size() >= 20) hdr.fileType = trim(line.substr(20, 1));

    while (std::getline(in_, line)) {
        line_++;
        if (line.size() >= 60) {
            std::string label = line.substr(60);
            if (label.find("END OF HEADER") != std::string::npos) {
                headerDone_ = true;
                obsTypes_ = hdr.obsTypes;
                return Result<RinexObsHeader>::ok(hdr);
            }
            if (label.find("SYS / # / OBS TYPES") != std::string::npos) {
                // Parse up to 13 obs type codes of 4 chars each, starting col 7.
                for (int i = 0; i < 13; ++i) {
                    int col = 7 + i * 4;
                    if (col + 3 > static_cast<int>(line.size())) break;
                    std::string code = trim(line.substr(col, 3));
                    if (!code.empty()) hdr.obsTypes.push_back(code);
                }
            } else if (label.find("PGM / RUN BY / DATE") != std::string::npos) {
                if (line.size() >= 20) hdr.program = trim(line.substr(0, 20));
            }
        }
    }
    return Result<RinexObsHeader>::err("RinexObs: header END OF HEADER not found");
}

Result<ObsEpoch> RinexObsParser::nextEpoch() {
    if (!headerDone_) {
        return Result<ObsEpoch>::err("RinexObs: header not read");
    }

    std::string line;
    while (std::getline(in_, line)) {
        line_++;
        std::string t = trim(line);
        if (t.empty()) continue;

        // Epoch line: '> YYYY MM DD HH MM SS.sssss [flag] [numSats] ...'
        if (line.size() < 3 || line[0] != '>') continue;

        ObsEpoch epoch;
        int year = parseFieldInt(line, 2, 4);
        int mon = parseFieldInt(line, 7, 2);
        int day = parseFieldInt(line, 10, 2);
        int hour = parseFieldInt(line, 13, 2);
        int min = parseFieldInt(line, 16, 2);
        double sec = parseFieldDouble(line, 18, 9);
        int numSats = parseFieldInt(line, 32, 3);

        // Convert to GPS time via std::tm.
        std::tm utc{};
        utc.tm_year = year - 1900;
        utc.tm_mon = mon - 1;
        utc.tm_mday = day;
        utc.tm_hour = hour;
        utc.tm_min = min;
        utc.tm_sec = static_cast<int>(sec);
        epoch.time = TimeSystem::utcToGps(utc);

        // Read the observation records.
        for (int s = 0; s < numSats; ++s) {
            std::string rline;
            if (!std::getline(in_, rline)) {
                return Result<ObsEpoch>::err(
                    "RinexObs: truncated observation data after line " +
                    std::to_string(line_));
            }
            line_++;
            if (rline.size() < 3) continue;

            ObsRecord rec;
            // Sat identifier: system (col 0) + PRN (cols 1-2).
            std::string sys = trim(rline.substr(0, 1));
            rec.prn = parseFieldInt(rline, 1, 2);
            if (sys != "G" && !sys.empty()) {
                warnings_.push_back("RinexObs: skipped non-GPS sat on line " +
                                    std::to_string(line_));
                continue;
            }

            // Decode each observation value (14 chars each, starting col 3).
            int fieldIdx = 0;
            for (int col = 3; col + 13 <= static_cast<int>(rline.size()) &&
                             fieldIdx < static_cast<int>(obsTypes_.size());
                 col += 14, ++fieldIdx) {
                std::string code = (fieldIdx < static_cast<int>(obsTypes_.size()))
                                       ? obsTypes_[fieldIdx] : "";
                std::string val = trim(rline.substr(col, 13));
                if (val.empty()) continue;
                double v = std::atof(val.c_str());
                if (code.size() >= 2) {
                    char band = code[0];
                    char type = code[1];
                    if (type == 'C') { rec.pseudorange = v; rec.hasPseudorange = true; }
                    else if (type == 'L') { rec.carrierPhase = v; rec.hasCarrierPhase = true; }
                    else if (type == 'D') { rec.doppler = v; rec.hasDoppler = true; }
                    else if (type == 'S') { rec.snr = v; rec.hasSnr = true; }
                    (void)band;
                }
            }
            epoch.records.push_back(rec);
        }

        return Result<ObsEpoch>::ok(epoch);
    }

    return Result<ObsEpoch>::eof();
}

}  // namespace lodestar::scenario
