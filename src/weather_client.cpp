#include "api/weather_client.hpp"

#include <expected>
#include <string>

#include "api/http_client.hpp"
#include "app/config.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace app {

namespace {

std::string GetCodeDescription(int code) {
    switch (code) {
        case 0:
            return "Clear sky";
        case 1:
            return "Mainly clear";
        case 2:
            return "Partly cloudy";
        case 3:
            return "Overcast";
        case 45:
            return "Fog";
        case 48:
            return "Depositing rime fog";
        case 51:
            return "Light drizzle";
        case 53:
            return "Moderate drizzle";
        case 55:
            return "Dense drizzle";
        case 56:
            return "Light freezing drizzle";
        case 57:
            return "Dense freezing drizzle";
        case 61:
            return "Slight rain";
        case 63:
            return "Moderate rain";
        case 65:
            return "Heavy rain";
        case 66:
            return "Light freezing rain";
        case 67:
            return "Heavy freezing rain";
        case 71:
            return "Slight snowfall";
        case 73:
            return "Moderate snowfall";
        case 75:
            return "Heavy snowfall";
        case 77:
            return "Snow grains";
        case 80:
            return "Slight rain showers";
        case 81:
            return "Moderate rain showers";
        case 82:
            return "Violent rain showers";
        case 85:
            return "Slight snow showers";
        case 86:
            return "Moderate snow showers";
        case 95:
            return "Thunderstorm";
        case 96:
            return "Thunderstorm with slight hail";
        case 99:
            return "Thunderstorm with heavy hail";
        default:
            return "Unknown weather";
    }
}

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

void SetDescription(WeatherInfo& weather) {
    weather.description = GetCodeDescription(weather.weather_code);
}

}  // namespace

std::expected<WeatherInfo, std::string> WeatherClient::FetchWeather(
    const Coordinates& coords) {
    QueryParams params{
        {"latitude", std::to_string(coords.lat)},
        {"longitude", std::to_string(coords.lon)},
        {"current",
         "temperature_2m,relative_humidity_2m,wind_speed_10m,wind_direction_"
         "10m,rain,snowfall,wind_gusts_10m,apparent_temperature,weather_code,"
         "is_day,cloud_cover"},
        {"past_days", "0"},
        {"forecast_days", "1"},
        {"timezone", "auto"}};
    HttpResponse response = client_.Get("/v1/forecast", params);
    if (IsErrorCode(response.status)) {
        return std::unexpected("Received an error status from API: " +
                               std::to_string(response.status));
    }
    return ParseWeather(response.body);
}

std::expected<WeatherInfo, std::string> WeatherClient::ParseWeather(
    const std::string& body) {
    try {
        json data = json::parse(body);
        const json& current = data.at("current");

        WeatherInfo weather{
            .temperature = current.at("temperature_2m").get<double>(),
            .apparent_temperature =
                current.at("apparent_temperature").get<double>(),
            .relative_humidity =
                current.at("relative_humidity_2m").get<double>(),
            .rain = current.at("rain").get<double>(),
            .snowfall = current.at("snowfall").get<double>(),
            .wind_speed = current.at("wind_speed_10m").get<double>(),
            .wind_gusts = current.at("wind_gusts_10m").get<double>(),
            .wind_direction = current.at("wind_direction_10m").get<int>(),
            .weather_code = current.at("weather_code").get<int>(),
            .cloud_cover = current.at("cloud_cover").get<int>(),
            .is_day = static_cast<bool>(current.at("is_day").get<int>()),
        };

        SetMood(weather);
        SetDescription(weather);

        return weather;
    } catch (const std::exception& e) {
        return std::unexpected("Failed to parse weather response: " +
                               std::string(e.what()));
    }
}

}  // namespace app
