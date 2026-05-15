#pragma once

#include "api/geolocation_client.hpp"
#include "api/places_client.hpp"
#include "api/routing_client.hpp"
#include "api/weather_client.hpp"

namespace app {

void RunServer(int port, WeatherClient& weather_client,
               PlacesClient& places_client, RoutingClient& routing_client,
               GeolocationClient& geolocation_client);

}  // namespace app
