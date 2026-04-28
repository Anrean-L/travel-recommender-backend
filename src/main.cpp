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
    app::HttplibHttpClient geolocation_http_client("api.2ip.io");
    app::HttplibHttpClient weather_http_client("api.open-meteo.com");
    app::HttplibHttpClient places_http_client("overpass-api.de");
    app::HttplibHttpClient routing_http_client("api.openrouteservice.org");

    app::GeolocationClient geolocation_client(geolocation_http_client,
                                              config->ip2_api_key);
    app::WeatherClient weather_client(weather_http_client);
    app::PlacesClient places_client(places_http_client);
    app::RoutingClient routing_client(routing_http_client, config->ors_api_key);

    app::RunServer(8080, weather_client, places_client, routing_client,
                   geolocation_client);

    return 0;
}
