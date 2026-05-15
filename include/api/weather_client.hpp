#pragma once
#include <expected>

#include "app/domain.hpp"
#include "http_client.hpp"

namespace app {

class WeatherClient {
   public:
    explicit WeatherClient(IHttpClient& client) : client_(client) {}

    std::expected<WeatherInfo, std::string> FetchWeather(
        const Coordinates& coords);

   private:
    std::expected<WeatherInfo, std::string> ParseWeather(
        const std::string& body);

    IHttpClient& client_;
};

}  // namespace app
