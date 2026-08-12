// core/scenario/rinex/RinexNav.cpp
// RINEX 3.x navigation file parser (Item 2.1). Parses GPS broadcast ephemeris
// records with fixed-width field decoding and line-numbered error reporting.

#include "core/scenario/rinex/RinexNav.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/scenario/ScenarioError.h"

namespace lodestar::scenario {

namespace {
std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

double parseFieldDouble(const std::string& line, int col, int width) {
    if (col + width > static_cast<int>(line.size())) {
        return 0.0;
    }
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

RinexNavParser::RinexNavParser(std::istream& in) : in_(in) {}

Result<RinexNavHeader> RinexNavParser::readHeader() {
    RinexNavHeader hdr;
    std::string line;

    // First line: version, file type.
    if (!std::getline(in_, line)) {
        return Result<RinexNavHeader>::err("RinexNav: empty file");
    }
    line_++;
    hdr.version = trim(line.substr(0, 9));
    if (line.size() >= 20) {
        hdr.fileType = trim(line.substr(20, 1));
    }

    while (std::getline(in_, line)) {
        line_++;
        if (line.size() >= 60) {
            std::string label = line.substr(60);
            if (label.find("END OF HEADER") != std::string::npos) {
                headerDone_ = true;
                return Result<RinexNavHeader>::ok(hdr);
            }
            if (label.find("ION ALPHA") != std::string::npos) {
                hdr.ionoAlpha[0] = parseFieldDouble(line, 3, 12);
                hdr.ionoAlpha[1] = parseFieldDouble(line, 15, 12);
                hdr.ionoAlpha[2] = parseFieldDouble(line, 27, 12);
                hdr.ionoAlpha[3] = parseFieldDouble(line, 39, 12);
            } else if (label.find("ION BETA") != std::string::npos) {
                hdr.ionoBeta[0] = parseFieldDouble(line, 3, 12);
                hdr.ionoBeta[1] = parseFieldDouble(line, 15, 12);
                hdr.ionoBeta[2] = parseFieldDouble(line, 27, 12);
                hdr.ionoBeta[3] = parseFieldDouble(line, 39, 12);
            } else if (label.find("PGM / RUN BY / DATE") != std::string::npos) {
                if (line.size() >= 20) hdr.program = trim(line.substr(0, 20));
            }
        }
    }

    return Result<RinexNavHeader>::err("RinexNav: header END OF HEADER not found");
}

Result<BroadcastEphemeris> RinexNavParser::nextEphemeris() {
    if (!headerDone_) {
        return Result<BroadcastEphemeris>::err("RinexNav: header not read");
    }

    std::string line;
    while (std::getline(in_, line)) {
        line_++;
        if (line.size() < 4) continue;

        // RINEX 3 NAV record starts with a satellite system + PRN in cols 0-2.
        // Skip non-GPS records for this parser (GPS "G" records only).
        std::string sys = trim(line.substr(0, 1));
        int prn = parseFieldInt(line, 1, 2);
        if (sys.empty()) continue;
        if (sys != "G" && sys != "") {
            warnings_.push_back("RinexNav: skipped non-GPS record on line " +
                                std::to_string(line_));
            continue;
        }

        // First record line: epoch (year, month, day, hour, min, sec) and
        // clock fields af0, af1, af2. We only need af0/af1/af2 (cols 22-73).
        BroadcastEphemeris e;
        e.prn = prn;
        // Epoch year/month/day at cols 4-21 (we skip time conversion here).
        if (line.size() >= 22) {
            e.af0 = parseFieldDouble(line, 22, 19);
        }
        if (line.size() >= 41) {
            e.af1 = parseFieldDouble(line, 41, 19);
        }
        if (line.size() >= 60) {
            e.af2 = parseFieldDouble(line, 60, 19);
        }

        // Read the ephemeris body (7 more lines for GPS).
        std::vector<std::string> body;
        for (int i = 0; i < 7; ++i) {
            std::string bl;
            if (!std::getline(in_, bl)) {
                return Result<BroadcastEphemeris>::err(
                    "RinexNav: truncated ephemeris record after line " +
                    std::to_string(line_));
            }
            line_++;
            body.push_back(bl);
        }
        if (body.size() < 7) {
            return Result<BroadcastEphemeris>::err("RinexNav: truncated ephemeris record");
        }

        // Body line 1 (index 0): IODE, Crs, deltaN, M0.
        if (body[0].size() >= 76) {
            e.iodc = parseFieldInt(body[0], 3, 2);
            e.crs = parseFieldDouble(body[0], 5, 19);
            e.deltaN = parseFieldDouble(body[0], 24, 19);
            e.M0 = parseFieldDouble(body[0], 43, 19);
        }
        // Body line 2 (index 1): Cuc, e, Cus, sqrtA.
        if (body[1].size() >= 76) {
            e.cuc = parseFieldDouble(body[1], 5, 19);
            e.e = parseFieldDouble(body[1], 24, 19);
            e.cus = parseFieldDouble(body[1], 43, 19);
            e.sqrtA = parseFieldDouble(body[1], 62, 19);
        }
        // Body line 3 (index 2): toe, Cic, omega0, Cis.
        if (body[2].size() >= 76) {
            e.toe = parseFieldDouble(body[2], 5, 19);
            e.cic = parseFieldDouble(body[2], 24, 19);
            e.omega0 = parseFieldDouble(body[2], 43, 19);
            e.cis = parseFieldDouble(body[2], 62, 19);
        }
        // Body line 4 (index 3): i0, Crc, argp, omegadot.
        if (body[3].size() >= 76) {
            e.i0 = parseFieldDouble(body[3], 5, 19);
            e.crc = parseFieldDouble(body[3], 24, 19);
            e.argp = parseFieldDouble(body[3], 43, 19);
            e.omegadot = parseFieldDouble(body[3], 62, 19);
        }
        // Body line 5 (index 4): idot, (spare).
        if (body[4].size() >= 24) {
            e.idot = parseFieldDouble(body[4], 5, 19);
        }

        if (!(e.sqrtA > 0.0)) {
            warnings_.push_back("RinexNav: invalid sqrtA for PRN " +
                                std::to_string(prn) + " (line " +
                                std::to_string(line_) + ")");
        }

        return Result<BroadcastEphemeris>::ok(e);
    }

    return Result<BroadcastEphemeris>::eof();
}

}  // namespace lodestar::scenario
