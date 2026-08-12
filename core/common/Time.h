#pragma once
// core/common/Time.h
// Small header-only time helpers. nowIso() returns the current wall-clock time
// as an ISO-8601-ish string ("YYYY-MM-DDTHH:MM:SS") so persisted timestamps are
// real, parseable date/times rather than placeholder literals like "now".

#include <ctime>
#include <string>

namespace lodestar::common {

// Returns the current local time as "YYYY-MM-DDTHH:MM:SS".
inline std::string nowIso() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_MSC_VER)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return std::string(buf);
}

}  // namespace lodestar::common
