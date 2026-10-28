#pragma once
#include <expected>
#include <string>
#include <utility>

#include "app/domain.hpp"
#include "http_client.hpp"

namespace app {

class WeatherClient {
   public:
    WeatherClient(IHttpClient& client, std::string api_key)
        : client_(client), api_key_(std::move(api_key)) {}

    std::expected<WeatherInfo, std::string> FetchWeather(
        const Coordinates& coords);

   private:
    std::expected<WeatherInfo, std::string> ParseWeather(
        const std::string& body);

    IHttpClient& client_;
    std::string api_key_;
};

}  // namespace app
