#include "api/geolocation_client.hpp"

#include <expected>
#include <nlohmann/json.hpp>

#include "api/http_client.hpp"
#include "app/config.hpp"

using json = nlohmann::json;

namespace app {

std::expected<Coordinates, std::string> GeolocationClient::FetchCoordinates(
    const std::string& ip) {
    QueryParams params = {{"token", api_key_}};
    auto response = client_.Get("/" + ip, params);
    if (IsErrorCode(response.status)) {
        return std::unexpected("Received an error status from API: " +
                               std::to_string(response.status));
    }
    try {
        json data = json::parse(response.body);
        Coordinates coords{.lat = std::stod(data.at("lat").get<std::string>()),
                           .lon = std::stod(data.at("lon").get<std::string>())};
        if (coords.lat < -90.0 || coords.lat > 90.0 || coords.lon < -180.0 ||
            coords.lon > 180.0) {
            return std::unexpected("Invalid coordinates are received");
        }
        return coords;
    } catch (const std::exception& e) {
        return std::unexpected("Failed to parse geolocation response: " +
                               std::string(e.what()));
    }
}

}  // namespace app
