#include "api/geolocation_client.hpp"

#include <cmath>
#include <expected>
#include <nlohmann/json.hpp>
#include <stdexcept>

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
        const auto read_coordinate = [](const json& value) {
            if (value.is_number()) return value.get<double>();
            const auto text = value.get<std::string>();
            std::size_t parsed = 0;
            const double coordinate = std::stod(text, &parsed);
            if (parsed != text.size()) {
                throw std::invalid_argument("Invalid coordinate string");
            }
            return coordinate;
        };
        Coordinates coords{.lat = read_coordinate(data.at("lat")),
                           .lon = read_coordinate(data.at("lon"))};
        if (!std::isfinite(coords.lat) || !std::isfinite(coords.lon) ||
            coords.lat < -90.0 || coords.lat > 90.0 || coords.lon < -180.0 ||
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
