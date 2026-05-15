#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>

#include "api/geolocation_client.hpp"
#include "api/http_client.hpp"
#include "api/places_client.hpp"
#include "api/routing_client.hpp"
#include "api/weather_client.hpp"
#include "app/config.hpp"
#include "app/server.hpp"

int main() {
    auto config = app::LoadConfig();
    if (!config) {
        std::cerr << config.error();
        return 1;
    }
    app::HttplibHttpClient two_ip_client("api.2ip.io", 32,
                                         std::chrono::hours(12));
    app::HttplibHttpClient open_meteo_client("api.open-meteo.com", 64,
                                             std::chrono::minutes(10));
    app::HttplibHttpClient overpass_client("overpass-api.de", 64,
                                           std::chrono::hours(2));
    app::HttplibHttpClient ors_client("api.openrouteservice.org", 512,
                                      std::chrono::hours(6));

    app::GeolocationClient geolocation_client(two_ip_client,
                                              config->two_ip_api_key);
    app::WeatherClient weather_client(open_meteo_client);
    app::PlacesClient places_client(overpass_client);
    app::RoutingClient routing_client(ors_client, config->ors_api_key);

    app::RunServer(config->port, weather_client, places_client, routing_client,
                   geolocation_client);

    return 0;
}
