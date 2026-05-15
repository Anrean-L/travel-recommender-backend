#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "app/domain.hpp"
#include "app/domain_json.hpp"

namespace {

using json = nlohmann::json;

}  // namespace

TEST(DomainJson, WeatherInfoSerializesOnlyPublicResponseFields) {
    const app::WeatherInfo weather{
        .temperature = 19.5,
        .apparent_temperature = 17.0,
        .relative_humidity = 63.0,
        .rain = 0.7,
        .snowfall = 0.0,
        .wind_speed = 11.5,
        .wind_gusts = 18.2,
        .wind_direction = 230,
        .weather_code = 3,
        .cloud_cover = 87,
        .is_day = false,
        .mood = app::WeatherMood::kBad,
        .description = "Overcast",
    };

    const json encoded = weather;
    const auto decoded = encoded.get<app::WeatherInfo>();

    EXPECT_FALSE(encoded.contains("wind_gusts"));
    EXPECT_FALSE(encoded.contains("weather_code"));
    EXPECT_FALSE(encoded.contains("cloud_cover"));
    EXPECT_FALSE(encoded.contains("is_day"));
    EXPECT_FALSE(encoded.contains("mood"));
    EXPECT_DOUBLE_EQ(decoded.temperature, 19.5);
    EXPECT_DOUBLE_EQ(decoded.apparent_temperature, 17.0);
    EXPECT_DOUBLE_EQ(decoded.relative_humidity, 63.0);
    EXPECT_DOUBLE_EQ(decoded.rain, 0.7);
    EXPECT_DOUBLE_EQ(decoded.snowfall, 0.0);
    EXPECT_DOUBLE_EQ(decoded.wind_speed, 11.5);
    EXPECT_EQ(decoded.wind_direction, 230);
    EXPECT_EQ(decoded.description, "Overcast");
}
