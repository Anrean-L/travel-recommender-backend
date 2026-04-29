#include "api/routing_client.hpp"

#include <exception>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "api/http_client.hpp"
#include "app/config.hpp"
#include "app/domain.hpp"

namespace app {

using json = nlohmann::json;

constexpr double kFallbackWalkingSpeed = 75.0;
constexpr double kDirectRouteModifier = 1.25;
constexpr double kSecondsInMinute = 60.0;

namespace {
json CoordinatesToMatrixPoint(const Coordinates& coords) {
    return json::array({coords.lon, coords.lat});
}

void ApplyFallbackRoute(ScoredPlace& place) {
    place.route_distance_m = place.straight_distance_m;
    place.walk_time_min =
        place.straight_distance_m * kDirectRouteModifier / kFallbackWalkingSpeed;
}

}  // namespace

std::vector<ScoredPlace> RoutingClient::AddRoutes(
    const Coordinates& origin, std::vector<ScoredPlace> places) {
    if (places.empty()) {
        return places;
    }

    auto routes = FetchRoutes(origin, places);
    if (!routes.has_value() || routes->size() != places.size()) {
        for (ScoredPlace& place : places) {
            ApplyFallbackRoute(place);
        }
        return places;
    }

    for (std::size_t i = 0; i < places.size(); ++i) {
        if (routes->at(i).has_value()) {
            places[i].route_distance_m = routes->at(i)->distance_m;
            places[i].walk_time_min =
                routes->at(i)->duration_sec / kSecondsInMinute;
        } else {
            ApplyFallbackRoute(places[i]);
        }
    }
    return places;
}

std::expected<std::vector<std::optional<RouteInfo>>, std::string>
RoutingClient::FetchRoutes(const Coordinates& origin,
                           const std::vector<ScoredPlace>& places) {
    json locations = json::array();
    locations.push_back(CoordinatesToMatrixPoint(origin));

    json destinations = json::array();
    for (std::size_t i = 0; i < places.size(); ++i) {
        locations.push_back(CoordinatesToMatrixPoint(places[i].place.coords));
        destinations.push_back(i + 1);
    }

    const json request_body = {{"locations", std::move(locations)},
                               {"sources", json::array({0})},
                               {"destinations", std::move(destinations)},
                               {"metrics", json::array({"distance", "duration"})}};
    auto response = client_.Post("/v2/matrix/foot-walking",
                                 HttpRequestBody{.content = request_body.dump(),
                                                 .content_type = "application/json"},
                                 {{"api_key", api_key_}});
    if (IsErrorCode(response.status)) {
        return std::unexpected("Received an error status from API: " +
                               std::to_string(response.status));
    }
    try {
        json data = json::parse(response.body);
        const auto& distances = data.at("distances").at(0);
        const auto& durations = data.at("durations").at(0);
        if (distances.size() != places.size() || durations.size() != places.size()) {
            return std::unexpected("Routing matrix response size mismatch");
        }

        std::vector<std::optional<RouteInfo>> routes;
        routes.reserve(places.size());
        for (std::size_t i = 0; i < places.size(); ++i) {
            if (distances.at(i).is_null() || durations.at(i).is_null()) {
                routes.push_back(std::nullopt);
                continue;
            }
            routes.push_back(RouteInfo{
                .distance_m = distances.at(i).get<double>(),
                .duration_sec = durations.at(i).get<double>()});
        }
        return routes;
    } catch (const std::exception& e) {
        return std::unexpected("Failed to parse routing response: " +
                               std::string(e.what()));
    }
}

}  // namespace app
