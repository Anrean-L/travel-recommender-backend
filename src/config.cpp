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
    auto ip2_api_key = GetEnv("TWO_IP_API_KEY");
    auto ors_api_key = GetEnv("ORS_API_KEY");
    auto openweather_api_key = GetEnv("OPENWEATHER_API_KEY");
    if (!openweather_api_key) {
        auto alternate = GetEnv("OPEN_WEATHER_API_KEY");
        if (alternate) openweather_api_key = std::move(alternate);
    }
    auto port = GetEnv("PORT");
    auto geoapify_key = GetEnv("GEOAPIFY_KEY");
    if (!ip2_api_key) {
        return std::unexpected(ip2_api_key.error());
    }
    if (!ors_api_key) {
        return std::unexpected(ors_api_key.error());
    }
    if (!geoapify_key) {
        return std::unexpected(geoapify_key.error());
    }
    if (!openweather_api_key) {
        return std::unexpected(openweather_api_key.error());
    }
    int parsed_port = 8080;
    try {
        if (port) parsed_port = std::stoi(*port);
    } catch (...) {
        parsed_port = 8080;
    }
    return Config{
        .two_ip_api_key = *ip2_api_key,
        .ors_api_key = *ors_api_key,
        .port = parsed_port,
        .geoapify_key = *geoapify_key,
        .openweather_api_key = *openweather_api_key,
    };
}

}  // namespace app
