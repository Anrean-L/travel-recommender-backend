#pragma once
#include <string>

namespace app {

struct Coordinates {
    double lat{};
    double lon{};
};

enum class WeatherMood {
    kGreatOutdoor,
    kOkay,
    kBad,
    kTerrible,
};

struct WeatherInfo {
    double temperature{};
    double apparent_temperature{};
    double relative_humidity{};
    double rain{};
    double snowfall{};
    double wind_speed{};
    double wind_gusts{};
    int wind_direction{};
    int weather_code{};
    int cloud_cover{};
    bool is_day{};

    WeatherMood mood;
    std::string description;
};

struct Place {};

}  // namespace app
