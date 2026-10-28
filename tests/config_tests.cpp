#include <gtest/gtest.h>

#include "app/config.hpp"
#include "test_support.hpp"

TEST(Config, IsErrorCodeTreatsNonSuccessStatusesAsErrors) {
    EXPECT_FALSE(app::IsErrorCode(200));
    EXPECT_FALSE(app::IsErrorCode(299));
    EXPECT_TRUE(app::IsErrorCode(300));
    EXPECT_TRUE(app::IsErrorCode(400));
    EXPECT_TRUE(app::IsErrorCode(401));
    EXPECT_TRUE(app::IsErrorCode(0));
}

TEST(Config, GetEnvReturnsReadableErrorForMissingVariable) {
    test_support::ScopedEnvVar env("LABWORK9_TEST_MISSING_ENV");
    env.Unset();

    const auto result = app::GetEnv("LABWORK9_TEST_MISSING_ENV");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              "LABWORK9_TEST_MISSING_ENV environment variable is not set");
}

TEST(Config, LoadConfigReadsApiKeysFromEnvironment) {
    test_support::ScopedEnvVar two_ip_api_key("TWO_IP_API_KEY");
    test_support::ScopedEnvVar ors_api_key("ORS_API_KEY");
    two_ip_api_key.Set("two-ip-secret");
    ors_api_key.Set("ors-secret");

    test_support::ScopedEnvVar geoapify_key("GEOAPIFY_KEY");
    geoapify_key.Set("places-secret");
    test_support::ScopedEnvVar weather("OPENWEATHER_API_KEY");
    weather.Set("weather-secret");
    test_support::ScopedEnvVar port("PORT");
    port.Unset();
    const auto result = app::LoadConfig();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->two_ip_api_key, "two-ip-secret");
    EXPECT_EQ(result->ors_api_key, "ors-secret");
    EXPECT_EQ(result->geoapify_key, "places-secret");
    EXPECT_EQ(result->port, 8080);
    EXPECT_EQ(result->openweather_api_key, "weather-secret");
}

TEST(Config, LoadConfigRequiresGeoapifyKey) {
    test_support::ScopedEnvVar two_ip("TWO_IP_API_KEY");
    test_support::ScopedEnvVar ors("ORS_API_KEY");
    test_support::ScopedEnvVar geoapify("GEOAPIFY_KEY");
    two_ip.Set("two-ip");
    ors.Set("ors");
    geoapify.Unset();
    auto result = app::LoadConfig();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "GEOAPIFY_KEY environment variable is not set");
}

TEST(Config, LoadConfigRequiresOpenWeatherKey) {
    test_support::ScopedEnvVar two_ip("TWO_IP_API_KEY");
    test_support::ScopedEnvVar ors("ORS_API_KEY");
    test_support::ScopedEnvVar geoapify("GEOAPIFY_KEY");
    test_support::ScopedEnvVar weather("OPENWEATHER_API_KEY");
    test_support::ScopedEnvVar alternate("OPEN_WEATHER_API_KEY");
    alternate.Unset();
    two_ip.Set("two-ip");
    ors.Set("ors");
    geoapify.Set("places");
    weather.Unset();
    auto result = app::LoadConfig();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              "OPENWEATHER_API_KEY environment variable is not set");
}

TEST(Config, LoadConfigAcceptsAlternateOpenWeatherKeyName) {
    test_support::ScopedEnvVar two_ip("TWO_IP_API_KEY");
    test_support::ScopedEnvVar ors("ORS_API_KEY");
    test_support::ScopedEnvVar geoapify("GEOAPIFY_KEY");
    test_support::ScopedEnvVar weather("OPENWEATHER_API_KEY");
    test_support::ScopedEnvVar alternate("OPEN_WEATHER_API_KEY");
    two_ip.Set("two-ip");
    ors.Set("ors");
    geoapify.Set("places");
    weather.Unset();
    alternate.Set("alternate-weather-key");
    auto result = app::LoadConfig();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->openweather_api_key, "alternate-weather-key");
    weather.Set("preferred-weather-key");
    result = app::LoadConfig();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->openweather_api_key, "preferred-weather-key");
}
