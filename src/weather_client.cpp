#include "api/weather_client.hpp"

#include <expected>
#include <stdexcept>
#include <string>

#include "api/http_client.hpp"
#include "app/config.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace app {

namespace {

void SetMood(WeatherInfo& weather) {
    if (weather.snowfall > 2.0 || weather.rain > 4.0 ||
        weather.wind_gusts > 55.0) {
        weather.mood = WeatherMood::kTerrible;
    } else if (weather.rain > 0.0 || weather.snowfall > 0.0 ||
               weather.apparent_temperature > 30 ||
               weather.apparent_temperature < -15 ||
               weather.wind_speed > 30.0) {
        weather.mood = WeatherMood::kBad;
    } else if (weather.temperature < -5.0 || weather.temperature > 25.0 ||
               weather.wind_speed > 20.0) {
        weather.mood = WeatherMood::kOkay;
    } else {
        weather.mood = WeatherMood::kGreatOutdoor;
    }
}

}  // namespace

std::expected<WeatherInfo, std::string> WeatherClient::FetchWeather(
    const Coordinates& coords) {
    QueryParams params{{"lat", std::to_string(coords.lat)},
                       {"lon", std::to_string(coords.lon)},
                       {"appid", api_key_},
                       {"units", "metric"},
                       {"lang", "en"}};
    HttpResponse response = client_.Get("/data/2.5/weather", params);
    if (IsErrorCode(response.status)) {
        return std::unexpected("OpenWeather API returned status " +
                               std::to_string(response.status));
    }
    return ParseWeather(response.body);
}

std::expected<WeatherInfo, std::string> WeatherClient::ParseWeather(
    const std::string& body) {
    try {
        json data = json::parse(body);
        const auto& main = data.at("main");
        const auto& wind = data.at("wind");
        const auto& condition = data.at("weather").at(0);
        const auto icon = condition.at("icon").get<std::string>();
        if (icon.empty() || (icon.back() != 'd' && icon.back() != 'n')) {
            throw std::invalid_argument("Invalid OpenWeather day/night icon");
        }
        // OpenWeather metric wind is m/s; the application uses km/h.
        // Snow is water equivalent in mm. Estimate snow depth in cm using
        // the 7:1 snow/water ratio used by our previous weather provider.
        WeatherInfo weather{
            .temperature = main.at("temp").get<double>(),
            .apparent_temperature = main.at("feels_like").get<double>(),
            .relative_humidity = main.at("humidity").get<double>(),
            .rain =
                data.contains("rain") ? data.at("rain").value("1h", 0.0) : 0.0,
            .snowfall = data.contains("snow")
                            ? data.at("snow").value("1h", 0.0) * 0.7
                            : 0.0,
            .wind_speed = wind.at("speed").get<double>() * 3.6,
            .wind_gusts =
                wind.value("gust", wind.at("speed").get<double>()) * 3.6,
            .wind_direction = wind.value("deg", 0),
            .weather_code = condition.at("id").get<int>(),
            .cloud_cover = data.at("clouds").at("all").get<int>(),
            .is_day = icon.back() == 'd',
        };
        weather.description = condition.at("description").get<std::string>();
        SetMood(weather);

        return weather;
    } catch (const std::exception& e) {
        return std::unexpected("Failed to parse weather response: " +
                               std::string(e.what()));
    }
}

}  // namespace app
