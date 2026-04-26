#include <gtest/gtest.h>

#include <vector>

#include "app/domain.hpp"
#include "app/scoring.hpp"

namespace {

app::Place MakePlace(app::PlaceKind kind, std::string name) {
    return app::Place{
        .name = std::move(name),
        .kind = kind,
        .environment = app::PlaceEnvironment::kIndoor,
        .coords = {59.9386, 30.3141},
    };
}

app::WeatherInfo MakeWeather(bool is_day) {
    app::WeatherInfo weather;
    weather.is_day = is_day;
    weather.mood = app::WeatherMood::kOkay;
    return weather;
}

}  // namespace

TEST(Scoring, NightBonusRaisesScoreForNightPlaces) {
    const app::Coordinates origin{59.9386, 30.3141};
    const std::vector<app::Place> places{MakePlace(app::PlaceKind::kCinema,
                                                   "Cinema")};

    const auto day_scores = app::ScorePlaces(places, MakeWeather(true), origin);
    const auto night_scores =
        app::ScorePlaces(places, MakeWeather(false), origin);

    ASSERT_EQ(day_scores.size(), 1U);
    ASSERT_EQ(night_scores.size(), 1U);
    EXPECT_GT(night_scores.front().score, day_scores.front().score);
    EXPECT_EQ(night_scores.front().reason, "Great fit for evening plans");
}

TEST(Scoring, NightBonusDoesNotAffectRegularPlaces) {
    const app::Coordinates origin{59.9386, 30.3141};
    const std::vector<app::Place> places{MakePlace(app::PlaceKind::kMuseum,
                                                   "Museum")};

    const auto day_scores = app::ScorePlaces(places, MakeWeather(true), origin);
    const auto night_scores =
        app::ScorePlaces(places, MakeWeather(false), origin);

    ASSERT_EQ(day_scores.size(), 1U);
    ASSERT_EQ(night_scores.size(), 1U);
    EXPECT_DOUBLE_EQ(night_scores.front().score, day_scores.front().score);
}
