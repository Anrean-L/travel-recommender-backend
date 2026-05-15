#pragma once

#include <expected>

#include "api/geolocation_client.hpp"
#include "api/places_client.hpp"
#include "api/routing_client.hpp"
#include "api/weather_client.hpp"
#include "app/domain.hpp"
#include "domain.hpp"

namespace app {

std::expected<RecommendationResponse, std::string> BuildRecommendations(
    const RecommendationRequest& request, WeatherClient& weather_client,
    PlacesClient& places_client, RoutingClient& routing_client,
    GeolocationClient& geolocation_client);

}  // namespace app
