#include "api/routing_client.hpp"

#include <exception>
#include <nlohmann/json.hpp>
#include <string>

#include "api/http_client.hpp"
#include "app/config.hpp"
#include "app/domain.hpp"

namespace app {

using json = nlohmann::json;

constexpr double kFallbackWalkingSpeed = 75.0;
constexpr double kDirectRouteModifier = 1.25;
constexpr double kSecondsInMinute = 60.0;

namespace {

std::string CoordinatesToString(Coordinates coords) {
    return std::to_string(coords.lon) + "," + std::to_string(coords.lat);
}

}  // namespace

std::vector<ScoredPlace> RoutingClient::AddRoutes(
    const Coordinates& origin, std::vector<ScoredPlace> places) {
    for (ScoredPlace& place : places) {
        auto route = FetchRoute(origin, place.place.coords);
        if (route.has_value()) {
            place.route_distance_m = route->distance_m;
            place.walk_time_min = route->duration_sec / kSecondsInMinute;
        } else {
            place.route_distance_m = place.straight_distance_m;
            place.walk_time_min = place.straight_distance_m *
                                  kDirectRouteModifier / kFallbackWalkingSpeed;
        }
    }
    return places;
}

std::expected<RouteInfo, std::string> RoutingClient::FetchRoute(
    const Coordinates& origin, const Coordinates& destination) {
    QueryParams params{{"api_key", api_key_},
                       {"start", CoordinatesToString(origin)},
                       {"end", CoordinatesToString(destination)}};
    auto response = client_.Get("/v2/directions/foot-walking", params);
    if (IsErrorCode(response.status)) {
        return std::unexpected("Received an error status from API: " +
                               std::to_string(response.status));
    }
    try {
        json data = json::parse(response.body);
        const auto& summary =
            data.at("features").at(0).at("properties").at("summary");
        return RouteInfo{.distance_m = summary.at("distance").get<double>(),
                         .duration_sec = summary.at("duration").get<double>()};
    } catch (const std::exception& e) {
        return std::unexpected("Failed to parse routing response: " +
                               std::string(e.what()));
    }
}

}  // namespace app
