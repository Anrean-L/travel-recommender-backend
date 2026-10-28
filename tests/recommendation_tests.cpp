#include <gtest/gtest.h>

#include <algorithm>

#include "api/geolocation_client.hpp"
#include "api/places_client.hpp"
#include "api/routing_client.hpp"
#include "api/weather_client.hpp"
#include "app/domain.hpp"
#include "app/recommendation.hpp"
#include "test_support.hpp"

TEST(Recommendation, BuildRecommendationsSkipsGeolocationWhenCoordsProvided) {
    test_support::ScenarioHttpClient http_client;
    http_client.geolocation_response =
        test_support::MakeGeolocationResponse(10.0, 20.0);
    http_client.weather_response = test_support::MakeWeatherResponse(0.0, true);
    http_client.routing_response = {
        200, R"({"distances":[[480.0,480.0]],"durations":[[360.0,360.0]]})"};
    http_client.places_response =
        test_support::MakePlacesResponse(test_support::json::array(
            {test_support::MakePlaceElement(1, "Cafe One", "cafe", 59.9390,
                                            30.3150),
             test_support::MakePlaceElement(2, "Museum One", "restaurant",
                                            59.9400, 30.3160)}));

    app::WeatherClient weather_client(http_client);
    app::PlacesClient places_client(http_client, "places-key");
    app::RoutingClient routing_client(http_client, "ors-key");
    app::GeolocationClient geolocation_client(http_client, "geo-key");

    app::RecommendationRequest request;
    request.has_coordinates = true;
    request.coords = {59.9386, 30.3141};
    request.client_ip = "8.8.8.8";
    request.max_results = 5;

    const auto result =
        app::BuildRecommendations(request, weather_client, places_client,
                                  routing_client, geolocation_client);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(http_client.geolocation_calls, 0);
    EXPECT_EQ(http_client.weather_calls, 1);
    EXPECT_EQ(http_client.places_calls, 1);
    EXPECT_EQ(http_client.routing_calls, 1);
    EXPECT_DOUBLE_EQ(result->location.lat, 59.9386);
    EXPECT_DOUBLE_EQ(result->location.lon, 30.3141);
    ASSERT_EQ(result->places.size(), 2U);
    EXPECT_DOUBLE_EQ(result->places.front().route_distance_m, 480.0);
    EXPECT_DOUBLE_EQ(result->places.front().walk_time_min, 6.0);
}

TEST(Recommendation, BuildRecommendationsUsesGeolocationAndDiversifiesKinds) {
    test_support::ScenarioHttpClient http_client;
    http_client.geolocation_response =
        test_support::MakeGeolocationResponse(59.9386, 30.3141);
    http_client.weather_response =
        test_support::MakeWeatherResponse(5.0, false);
    http_client.routing_response =
        test_support::MakeRouteResponse(300.0, 240.0);
    http_client.places_response =
        test_support::MakePlacesResponse(test_support::json::array(
            {test_support::MakePlaceElement(1, "Restaurant 1", "restaurant",
                                            59.9387, 30.3142),
             test_support::MakePlaceElement(2, "Restaurant 2", "restaurant",
                                            59.9388, 30.3143),
             test_support::MakePlaceElement(3, "Restaurant 3", "restaurant",
                                            59.9389, 30.3144),
             test_support::MakePlaceElement(4, "Restaurant 4", "restaurant",
                                            59.9390, 30.3145),
             test_support::MakePlaceElement(5, "Restaurant 5", "restaurant",
                                            59.9391, 30.3146),
             test_support::MakePlaceElement(6, "Cafe 1", "cafe", 59.9392,
                                            30.3147),
             test_support::MakePlaceElement(7, "Cafe 2", "cafe", 59.9393,
                                            30.3148)}));

    app::WeatherClient weather_client(http_client);
    app::PlacesClient places_client(http_client, "places-key");
    app::RoutingClient routing_client(http_client, "ors-key");
    app::GeolocationClient geolocation_client(http_client, "geo-key");

    app::RecommendationRequest request;
    request.client_ip = "8.8.8.8";
    request.max_results = 4;
    request.radius_m = 750;

    const auto result =
        app::BuildRecommendations(request, weather_client, places_client,
                                  routing_client, geolocation_client);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(http_client.geolocation_calls, 1);
    ASSERT_FALSE(http_client.get_requests.empty());
    EXPECT_EQ(http_client.get_requests.front().path, "/8.8.8.8");
    ASSERT_EQ(result->places.size(), 4U);
    EXPECT_EQ(std::count_if(result->places.begin(), result->places.end(),
                            [](const app::ScoredPlace& place) {
                                return place.place.kind ==
                                       app::PlaceKind::kRestaurant;
                            }),
              3);
    EXPECT_EQ(std::count_if(result->places.begin(), result->places.end(),
                            [](const app::ScoredPlace& place) {
                                return place.place.kind ==
                                       app::PlaceKind::kCafe;
                            }),
              1);
}

TEST(Recommendation, BuildRecommendationsReturnsClientFailureMessage) {
    test_support::ScenarioHttpClient http_client;
    http_client.geolocation_response =
        test_support::MakeGeolocationResponse(59.9386, 30.3141);
    http_client.weather_response = {200, R"({"current":42})"};
    http_client.routing_response =
        test_support::MakeRouteResponse(300.0, 240.0);
    http_client.places_response =
        test_support::MakePlacesResponse(test_support::json::array());

    app::WeatherClient weather_client(http_client);
    app::PlacesClient places_client(http_client, "places-key");
    app::RoutingClient routing_client(http_client, "ors-key");
    app::GeolocationClient geolocation_client(http_client, "geo-key");

    app::RecommendationRequest request;
    request.client_ip = "8.8.8.8";

    const auto result =
        app::BuildRecommendations(request, weather_client, places_client,
                                  routing_client, geolocation_client);

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Failed to parse weather response: "),
              std::string::npos);
}
