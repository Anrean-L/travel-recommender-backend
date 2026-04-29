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

TEST(Config, LoadConfigReadsBothApiKeysFromEnvironment) {
    test_support::ScopedEnvVar two_ip_api_key("TWO_IP_API_KEY");
    test_support::ScopedEnvVar ors_api_key("ORS_API_KEY");
    two_ip_api_key.Set("two-ip-secret");
    ors_api_key.Set("ors-secret");

    const auto result = app::LoadConfig();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->two_ip_api_key, "two-ip-secret");
    EXPECT_EQ(result->ors_api_key, "ors-secret");
}
