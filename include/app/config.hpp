#pragma once
#include <expected>
#include <string>

namespace app {

constexpr int kFirstOkStatusCode = 200;
constexpr int kFirstErrorStatusCode = 300;

inline bool IsErrorCode(int code) {
    return code >= kFirstErrorStatusCode && code < kFirstErrorStatusCode;
}

std::expected<std::string, std::string> GetEnv(std::string_view key);

struct Config {
    std::string ip2_api_key;
    std::string ors_api_key;
};

std::expected<Config, std::string> LoadConfig();

}  // namespace app
