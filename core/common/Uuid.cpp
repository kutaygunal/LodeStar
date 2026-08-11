// core/common/Uuid.cpp
#include "core/common/Uuid.h"

#include <cstdio>
#include <random>

namespace lodestar::common {

std::string newUuid() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<unsigned long long> dist(0, 0xFFFFFFFFFFFFFFFFULL);

    unsigned long long a = dist(gen);
    unsigned long long b = dist(gen);

    // Set version (4) and variant bits for a UUID v4.
    a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    char buf[40] = {0};
    std::snprintf(buf, sizeof(buf),
                  "%08llx-%04llx-%04llx-%04llx-%012llx",
                  (a >> 32) & 0xFFFFFFFFULL,
                  (a >> 16) & 0xFFFFULL,
                  a & 0xFFFFULL,
                  (b >> 48) & 0xFFFFULL,
                  b & 0xFFFFFFFFFFFFULL);
    return std::string(buf);
}

}  // namespace lodestar::common
