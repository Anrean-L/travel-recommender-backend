#include <gtest/gtest.h>

#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>

#include "api/geolocation_client.hpp"
#include "api/http_client.hpp"
#include "api/places_client.hpp"
#include "api/routing_client.hpp"
#include "api/weather_client.hpp"
#include "app/domain.hpp"
#include "app/scoring.hpp"

namespace {

class FakeHttpClient final : public app::IHttpClient {
   public:
    app::HttpResponse Get(const std::string& path,
                          const app::QueryParams& params = {}) override {
        last_get_path = path;
        last_get_params = params;
        return get_response;
    }

    app::HttpResponse Post(const std::string& path,
                           const app::QueryParams& params = {}) override {
        last_post_path = path;
        last_post_params = params;
        return post_response;
    }

    app::HttpResponse Post(const std::string& path,
                           const app::HttpRequestBody& body,
                           const app::QueryParams& params = {}) override {
        last_post_json_path = path;
        last_post_json_body = body;
        last_post_json_params = params;
        return post_json_response;
    }

    app::HttpResponse get_response{200, ""};
    app::HttpResponse post_response{200, ""};
    app::HttpResponse post_json_response{200, ""};
    std::string last_get_path;
    std::string last_post_path;
    std::string last_post_json_path;
    app::QueryParams last_get_params;
    app::QueryParams last_post_params;
    app::QueryParams last_post_json_params;
    app::HttpRequestBody last_post_json_body;
};

const app::QueryParam* FindParam(const app::QueryParams& params,
                                 const std::string& key) {
    const auto it = std::find_if(
        params.begin(), params.end(),
        [&key](const app::QueryParam& param) { return param.key == key; });
    if (it == params.end()) {
        return nullptr;
    }
    return &*it;
}

app::ScoredPlace MakeScoredPlace(double score, double straight_distance_m,
                                 app::Coordinates coords) {
    app::ScoredPlace place;
    place.place.name = "Test place";
    place.place.kind = app::PlaceKind::kCafe;
    place.place.environment = app::PlaceEnvironment::kIndoor;
    place.place.coords = coords;
    place.score = score;
    place.straight_distance_m = straight_distance_m;
    return place;
}
}  // namespace

TEST(GeolocationClient, FetchCoordinatesBuildsRequestAndParsesResponse) {
    FakeHttpClient http_client;
    http_client.get_response = {200, R"({"lat":"59.9386","lon":"30.3141"})"};

    app::GeolocationClient client(http_client, "secret-token");
    const auto result = client.FetchCoordinates("8.8.8.8");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(http_client.last_get_path, "/8.8.8.8");
    ASSERT_NE(FindParam(http_client.last_get_params, "token"), nullptr);
    EXPECT_EQ(FindParam(http_client.last_get_params, "token")->value,
              "secret-token");
    EXPECT_DOUBLE_EQ(result->lat, 59.9386);
    EXPECT_DOUBLE_EQ(result->lon, 30.3141);
}

TEST(GeolocationClient, FetchCoordinatesRejectsCoordinatesOutsideWorldBounds) {
    FakeHttpClient http_client;
    http_client.get_response = {200, R"({"lat":"91.0","lon":"30.3141"})"};

    app::GeolocationClient client(http_client, "secret-token");
    const auto result = client.FetchCoordinates("8.8.8.8");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Invalid coordinates are received");
}

TEST(WeatherClient, FetchWeatherBuildsForecastRequestAndMapsWeatherFields) {
    FakeHttpClient http_client;
    http_client.get_response = {200,
                                R"({
            "current": {
                "temperature_2m": 21.5,
                "apparent_temperature": 22.0,
                "relative_humidity_2m": 48.0,
                "rain": 0.0,
                "snowfall": 0.0,
                "wind_speed_10m": 12.0,
                "wind_gusts_10m": 18.0,
                "wind_direction_10m": 240,
                "weather_code": 0,
                "is_day": 0,
                "cloud_cover": 15
            }
        })"};

    app::WeatherClient client(http_client);
    const auto result = client.FetchWeather({59.9386, 30.3141});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(http_client.last_get_path, "/v1/forecast");
    ASSERT_NE(FindParam(http_client.last_get_params, "latitude"), nullptr);
    ASSERT_NE(FindParam(http_client.last_get_params, "longitude"), nullptr);
    ASSERT_NE(FindParam(http_client.last_get_params, "current"), nullptr);
    ASSERT_NE(FindParam(http_client.last_get_params, "timezone"), nullptr);
    EXPECT_EQ(FindParam(http_client.last_get_params, "timezone")->value,
              "auto");
    EXPECT_DOUBLE_EQ(result->temperature, 21.5);
    EXPECT_EQ(result->weather_code, 0);
    EXPECT_FALSE(result->is_day);
    EXPECT_EQ(result->description, "Clear sky");
    EXPECT_EQ(result->mood, app::WeatherMood::kGreatOutdoor);
}

TEST(WeatherClient, FetchWeatherReturnsParseErrorForMalformedJson) {
    FakeHttpClient http_client;
    http_client.get_response = {200, R"({"current":42})"};

    app::WeatherClient client(http_client);
    const auto result = client.FetchWeather({59.9386, 30.3141});

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Failed to parse weather response: "),
              std::string::npos);
}

TEST(PlacesClient, FetchPlacesBuildsOverpassQueryAndParsesPlaceDetails) {
    FakeHttpClient http_client;
    http_client.post_response = {200,
                                 R"({
            "elements": [
                {
                    "type": "node",
                    "id": 101,
                    "lat": 59.9387,
                    "lon": 30.3142,
                    "tags": {
                        "name": "Cinema Mirage",
                        "amenity": "cinema",
                        "addr:street": "Nevsky Prospect",
                        "addr:housenumber": "24",
                        "website": "https://cinema.example",
                        "opening_hours": "Mo-Su 10:00-23:00"
                    }
                }
            ]
        })"};

    app::PlacesClient client(http_client);
    const auto result = client.FetchPlaces({59.9386, 30.3141}, 750);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(http_client.last_post_path, "/api/interpreter");
    ASSERT_NE(FindParam(http_client.last_post_params, "data"), nullptr);
    const std::string& query =
        FindParam(http_client.last_post_params, "data")->value;
    EXPECT_NE(query.find("around:750,59.938600,30.314100"), std::string::npos);
    ASSERT_EQ(result->size(), 1U);
    EXPECT_EQ(result->front().name, "Cinema Mirage");
    EXPECT_EQ(result->front().kind, app::PlaceKind::kCinema);
    EXPECT_EQ(result->front().environment, app::PlaceEnvironment::kIndoor);
    EXPECT_EQ(result->front().address, "Nevsky Prospect, 24");
    EXPECT_EQ(result->front().website, "https://cinema.example");
    EXPECT_EQ(result->front().opening_hours, "Mo-Su 10:00-23:00");
}

TEST(PlacesClient, FetchPlacesSkipsDuplicatesAndUnnamedEntries) {
    FakeHttpClient http_client;
    http_client.post_response = {200,
                                 R"({
            "elements": [
                {
                    "type": "node",
                    "id": 42,
                    "lat": 59.9387,
                    "lon": 30.3142,
                    "tags": {
                        "name": "Summer Garden",
                        "leisure": "garden"
                    }
                },
                {
                    "type": "node",
                    "id": 42,
                    "lat": 59.9387,
                    "lon": 30.3142,
                    "tags": {
                        "name": "Summer Garden",
                        "leisure": "garden"
                    }
                },
                {
                    "type": "node",
                    "id": 43,
                    "lat": 59.9388,
                    "lon": 30.3143,
                    "tags": {
                        "leisure": "park"
                    }
                }
            ]
        })"};

    app::PlacesClient client(http_client);
    const auto result = client.FetchPlaces({59.9386, 30.3141}, 500);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 1U);
    EXPECT_EQ(result->front().name, "Summer Garden");
    EXPECT_EQ(result->front().kind, app::PlaceKind::kGarden);
}

TEST(RoutingClient, AddRoutesUsesApiResponseAndStoresRouteMetrics) {
    FakeHttpClient http_client;
    http_client.post_json_response = {
        200, R"({"distances":[[900.0]],"durations":[[600.0]]})"};

    app::RoutingClient client(http_client, "ors-key");
    const app::Coordinates origin{59.9386, 30.3141};
    std::vector<app::ScoredPlace> places{
        MakeScoredPlace(80.0, 700.0, {59.939, 30.32})};

    const auto result = client.AddRoutes(origin, std::move(places));

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(http_client.last_post_json_path, "/v2/matrix/foot-walking");
    ASSERT_NE(FindParam(http_client.last_post_json_params, "api_key"), nullptr);
    EXPECT_EQ(FindParam(http_client.last_post_json_params, "api_key")->value,
              "ors-key");
    EXPECT_EQ(http_client.last_post_json_body.content_type, "application/json");
    const auto body =
        nlohmann::json::parse(http_client.last_post_json_body.content);
    EXPECT_EQ(body.at("locations"),
              nlohmann::json::array({nlohmann::json::array({30.3141, 59.9386}),
                                     nlohmann::json::array({30.32, 59.939})}));
    EXPECT_EQ(body.at("sources"), nlohmann::json::array({0}));
    EXPECT_EQ(body.at("destinations"), nlohmann::json::array({1}));
    EXPECT_EQ(body.at("metrics"),
              nlohmann::json::array({"distance", "duration"}));
    EXPECT_DOUBLE_EQ(result.front().route_distance_m, 900.0);
    EXPECT_DOUBLE_EQ(result.front().walk_time_min, 10.0);
    EXPECT_DOUBLE_EQ(result.front().score, 80.0);
}

TEST(RoutingClient, AddRoutesFallsBackToStraightDistanceWhenRouteFails) {
    FakeHttpClient http_client;
    http_client.post_json_response = {200, R"({"distances":[],"durations":[]})"};

    app::RoutingClient client(http_client, "ors-key");
    const app::Coordinates origin{59.9386, 30.3141};
    std::vector<app::ScoredPlace> places{
        MakeScoredPlace(80.0, 900.0, {59.939, 30.32})};

    const auto result = client.AddRoutes(origin, std::move(places));

    ASSERT_EQ(result.size(), 1U);
    EXPECT_DOUBLE_EQ(result.front().route_distance_m, 900.0);
    EXPECT_DOUBLE_EQ(result.front().walk_time_min, 15.0);
    EXPECT_DOUBLE_EQ(result.front().score, 80.0);
}

TEST(Scoring, RerankWithRoutesAppliesRoutePenaltyForOkayWeather) {
    app::WeatherInfo weather;
    weather.mood = app::WeatherMood::kOkay;

    std::vector<app::ScoredPlace> places{
        MakeScoredPlace(80.0, 700.0, {59.939, 30.32})};
    places.front().walk_time_min = 10.0;

    const auto result = app::RerankWithRoutes(std::move(places), weather);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_DOUBLE_EQ(result.front().score, 75.0);
}

TEST(Scoring, RerankWithRoutesAppliesWeatherAdjustedPenalty) {
    app::WeatherInfo weather;
    weather.mood = app::WeatherMood::kTerrible;

    std::vector<app::ScoredPlace> places{
        MakeScoredPlace(80.0, 900.0, {59.939, 30.32})};
    places.front().walk_time_min = 15.0;

    const auto result = app::RerankWithRoutes(std::move(places), weather);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_DOUBLE_EQ(result.front().score, 64.0);
}
