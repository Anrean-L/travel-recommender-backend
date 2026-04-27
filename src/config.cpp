#include "app/config.hpp"

#include <cstdlib>
#include <expected>
#include <string>

namespace app {

std::expected<std::string, std::string> GetEnv(std::string_view key) {
    const char* value = std::getenv(key.data());
    if (value == nullptr || std::string_view(value).empty()) {
        return std::unexpected(std::string(key) +
                               " environment variable is not set");
    }
    return std::string(value);
}

std::expected<Config, std::string> LoadConfig() {
    auto ip2_api_key = GetEnv("IP2_API_KEY");
    auto ors_api_key = GetEnv("ORS_API_KEY");
    if (!ip2_api_key) {
        return std::unexpected(ip2_api_key.error());
    }
    if (!ors_api_key) {
        return std::unexpected(ors_api_key.error());
    }
    return Config{
        *ip2_api_key,
        *ors_api_key,
    };
}

}  // namespace app
