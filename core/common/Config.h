#pragma once
// core/common/Config.h
// Simple key/value configuration for the Lodestar core. Loads from a
// "key=value" file and resolves the user data directory and DB path.

#include <map>
#include <string>

namespace lodestar::common {

class Config {
public:
    static Config& instance();

    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    void load(const std::string& path);
    void set(const std::string& key, const std::string& value);
    std::string get(const std::string& key, const std::string& def = "") const;

    // Resolved at runtime from the OS user-data location.
    std::string userDataDir() const;
    std::string databasePath() const;

private:
    Config() = default;
    std::map<std::string, std::string> values_;
};

}  // namespace lodestar::common
