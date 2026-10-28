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

TEST(GeolocationClient, FetchCoordinatesAcceptsNumericAndMixedFormats) {
    FakeHttpClient http_client;
    app::GeolocationClient client(http_client, "secret-token");
    for (const auto* body : {R"({"lat":59.9386,"lon":30.3141})",
                             R"({"lat":"59.9386","lon":30.3141})",
                             R"({"lat":59.9386,"lon":"30.3141"})"}) {
        http_client.get_response = {200, body};
        const auto result = client.FetchCoordinates("8.8.8.8");
        ASSERT_TRUE(result.has_value()) << result.error();
        EXPECT_DOUBLE_EQ(result->lat, 59.9386);
        EXPECT_DOUBLE_EQ(result->lon, 30.3141);
    }
}

TEST(GeolocationClient, FetchCoordinatesRejectsMalformedAndNonFiniteValues) {
    FakeHttpClient http_client;
    app::GeolocationClient client(http_client, "secret-token");
    for (const auto* body :
         {R"({"lat":"nan","lon":30})", R"({"lat":59,"lon":"inf"})",
          R"({"lat":"59junk","lon":30})", R"({"lat":null,"lon":30})",
          R"({"lat":91,"lon":30})"}) {
        http_client.get_response = {200, body};
        EXPECT_FALSE(client.FetchCoordinates("8.8.8.8").has_value());
    }
}

TEST(GeolocationClient, FetchCoordinatesRejectsCoordinatesOutsideWorldBounds) {
    FakeHttpClient http_client;
    http_client.get_response = {200, R"({"lat":"91.0","lon":"30.3141"})"};

    app::GeolocationClient client(http_client, "secret-token");
    const auto result = client.FetchCoordinates("8.8.8.8");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Invalid coordinates are received");
}

TEST(WeatherClient, BuildsOpenWeatherRequestAndConvertsUnits) {
    FakeHttpClient http;
    http.get_response = {200, R"({
        "main":{"temp":21.5,"feels_like":22.0,"humidity":48},
        "wind":{"speed":3.0,"gust":5.0,"deg":240},
        "rain":{"1h":1.2}, "snow":{"1h":0.5},
        "clouds":{"all":15},
        "weather":[{"id":601,"icon":"13n","description":"snow"}]
    })"};
    app::WeatherClient client(http, "weather-key");
    auto result = client.FetchWeather({59.9386, 30.3141});
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(http.last_get_path, "/data/2.5/weather");
    for (const auto* key : {"lat", "lon", "appid", "units"})
        ASSERT_NE(FindParam(http.last_get_params, key), nullptr);
    EXPECT_EQ(FindParam(http.last_get_params, "appid")->value, "weather-key");
    EXPECT_EQ(FindParam(http.last_get_params, "units")->value, "metric");
    EXPECT_EQ(FindParam(http.last_get_params, "lat")->value, "59.938600");
    EXPECT_EQ(FindParam(http.last_get_params, "lon")->value, "30.314100");
    EXPECT_DOUBLE_EQ(result->temperature, 21.5);
    EXPECT_DOUBLE_EQ(result->apparent_temperature, 22.0);
    EXPECT_DOUBLE_EQ(result->relative_humidity, 48.0);
    EXPECT_DOUBLE_EQ(result->rain, 1.2);
    EXPECT_DOUBLE_EQ(result->snowfall, 0.35);
    EXPECT_DOUBLE_EQ(result->wind_speed, 10.8);
    EXPECT_DOUBLE_EQ(result->wind_gusts, 18.0);
    EXPECT_EQ(result->wind_direction, 240);
    EXPECT_EQ(result->cloud_cover, 15);
    EXPECT_EQ(result->weather_code, 601);
    EXPECT_FALSE(result->is_day);
    EXPECT_EQ(result->description, "snow");
    EXPECT_EQ(result->mood, app::WeatherMood::kBad);
}

TEST(WeatherClient, HandlesMissingPrecipitationAndGusts) {
    FakeHttpClient http;
    http.get_response = {200, R"({
        "main":{"temp":21,"feels_like":22,"humidity":48},
        "wind":{"speed":2}, "clouds":{"all":0},
        "weather":[{"id":800,"icon":"01d","description":"clear sky"}]
    })"};
    app::WeatherClient client(http, "weather-key");
    auto result = client.FetchWeather({59.9, 30.3});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_day);
    EXPECT_DOUBLE_EQ(result->rain, 0);
    EXPECT_DOUBLE_EQ(result->snowfall, 0);
    EXPECT_DOUBLE_EQ(result->wind_gusts, result->wind_speed);
    EXPECT_EQ(result->mood, app::WeatherMood::kGreatOutdoor);
}

TEST(WeatherClient, FetchWeatherReturnsParseErrorForMalformedJson) {
    FakeHttpClient http;
    app::WeatherClient client(http, "weather-key");
    for (const auto* body : {"not json", R"({"main":42})",
                             R"({"main":{},"wind":{},"weather":[]})"}) {
        http.get_response = {200, body};
        auto result = client.FetchWeather({59.9, 30.3});
        ASSERT_FALSE(result.has_value());
        EXPECT_NE(result.error().find("Failed to parse weather response: "),
                  std::string::npos);
    }
}

TEST(WeatherClient, ReportsProviderStatusWithoutExposingKey) {
    FakeHttpClient http;
    http.get_response = {429, "secret-token"};
    app::WeatherClient client(http, "secret-token");
    auto result = client.FetchWeather({59.9, 30.3});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "OpenWeather API returned status 429");
}

TEST(PlacesClient, FetchPlacesBuildsGeoapifyRequestAndParsesDetails) {
    FakeHttpClient http_client;
    http_client.get_response = {200, R"({"features":[{
        "properties":{
            "name":"Cinema Mirage", "lat":59.9387, "lon":30.3142,
            "categories":["entertainment.cinema"], "place_id":"cinema-1",
            "address_line2":"Nevsky Prospect, 24", "website":"https://cinema.example",
            "opening_hours":"Mo-Su 10:00-23:00",
            "datasource":{"raw":{"osm_type":"n","osm_id":101}}
        }}]})"};
    app::PlacesClient client(http_client, "places-key");
    const auto result = client.FetchPlaces({59.9386, 30.3141}, 750);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(http_client.last_get_path, "/v2/places");
    for (const auto* key :
         {"apiKey", "filter", "bias", "categories", "limit"}) {
        ASSERT_NE(FindParam(http_client.last_get_params, key), nullptr);
    }
    EXPECT_EQ(FindParam(http_client.last_get_params, "apiKey")->value,
              "places-key");
    EXPECT_EQ(FindParam(http_client.last_get_params, "filter")->value,
              "circle:30.314100,59.938600,750");
    EXPECT_EQ(FindParam(http_client.last_get_params, "bias")->value,
              "proximity:30.314100,59.938600");
    EXPECT_EQ(FindParam(http_client.last_get_params, "limit")->value, "100");
    ASSERT_EQ(result->size(), 1U);
    EXPECT_EQ(result->front().name, "Cinema Mirage");
    EXPECT_EQ(result->front().kind, app::PlaceKind::kCinema);
    EXPECT_EQ(result->front().environment, app::PlaceEnvironment::kIndoor);
    EXPECT_EQ(result->front().osm_type, "node");
    EXPECT_EQ(result->front().osm_id, 101U);
    EXPECT_DOUBLE_EQ(result->front().coords.lat, 59.9387);
    EXPECT_EQ(result->front().address, "Nevsky Prospect, 24");
    EXPECT_EQ(result->front().website, "https://cinema.example");
    EXPECT_EQ(result->front().opening_hours, "Mo-Su 10:00-23:00");
}

TEST(PlacesClient, SkipsDuplicatesUnnamedAndInvalidCoordinates) {
    FakeHttpClient http_client;
    http_client.get_response = {200, R"({"features":[
        {"properties":{"name":"Garden","place_id":"garden","categories":["leisure.park","leisure.park.garden"]},
         "geometry":{"type":"Point","coordinates":[30.3142,59.9387]}},
        {"properties":{"name":"Duplicate","place_id":"garden","categories":["leisure.park.garden"],"lat":59.9387,"lon":30.3142}},
        {"properties":{"place_id":"unnamed","categories":["leisure.park"],"lat":59.9,"lon":30.3}},
        {"properties":{"name":"Invalid","categories":["leisure.park"],"lat":91,"lon":30}},
        {"properties":{"name":"Unknown","categories":["unknown"],"lat":59,"lon":30}}
    ]})"};
    app::PlacesClient client(http_client, "places-key");
    auto result = client.FetchPlaces({59.9386, 30.3141}, 500);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 1U);
    EXPECT_EQ(result->front().kind, app::PlaceKind::kGarden);
    EXPECT_EQ(result->front().environment, app::PlaceEnvironment::kOutdoor);
}

TEST(PlacesClient, ReportsUpstreamErrorWithoutExposingKey) {
    FakeHttpClient http_client;
    http_client.get_response = {401, "secret-key"};
    app::PlacesClient client(http_client, "secret-key");
    auto result = client.FetchPlaces({59.9, 30.3}, 500);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Geoapify Places API returned status 401");
}

TEST(PlacesClient, RejectsMalformedResponseAndAcceptsEmptyResults) {
    FakeHttpClient http_client;
    app::PlacesClient client(http_client, "places-key");
    http_client.get_response = {200, R"({"features":42})"};
    EXPECT_FALSE(client.FetchPlaces({59.9, 30.3}, 500).has_value());
    http_client.get_response = {200, R"({"features":[]})"};
    auto result = client.FetchPlaces({59.9, 30.3}, 500);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
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
    http_client.post_json_response = {200,
                                      R"({"distances":[],"durations":[]})"};

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
