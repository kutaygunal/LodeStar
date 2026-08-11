// core/common/Config.cpp
#include "core/common/Config.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#else
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lodestar::common {

Config& Config::instance() {
    static Config config;
    return config;
}

void Config::set(const std::string& key, const std::string& value) {
    values_[key] = value;
}

std::string Config::get(const std::string& key, const std::string& def) const {
    auto it = values_.find(key);
    return it == values_.end() ? def : it->second;
}

void Config::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        // Trim surrounding whitespace.
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            continue;
        }
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        // Trim key/value whitespace.
        auto trim = [](std::string s) {
            size_t b = s.find_first_not_of(" \t\r\n");
            size_t e = s.find_last_not_of(" \t\r\n");
            return (b == std::string::npos) ? std::string() : s.substr(b, e - b + 1);
        };
        values_[trim(key)] = trim(value);
    }
}

std::string Config::userDataDir() const {
    std::string dir;
#if defined(_WIN32)
    wchar_t* raw = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw)) && raw != nullptr) {
        char buf[1024] = {0};
        WideCharToMultiByte(CP_UTF8, 0, raw, -1, buf, sizeof(buf), nullptr, nullptr);
        dir = std::string(buf) + "\\Lodestar";
        CoTaskMemFree(raw);
    } else {
        const char* appdata = std::getenv("APPDATA");
        dir = appdata ? std::string(appdata) + "\\Lodestar" : std::string(".lodestar");
    }
    _mkdir(dir.c_str());
#else
    const char* home = std::getenv("HOME");
    dir = home ? std::string(home) + "/.lodestar" : std::string(".lodestar");
    mkdir(dir.c_str(), 0755);
#endif
    return dir;
}

std::string Config::databasePath() const {
    std::string configured = get("database.path");
    if (!configured.empty()) {
        return configured;
    }
    return userDataDir() + "/lodestar.db";
}

}  // namespace lodestar::common
