#pragma once

#include <expected>

#include "app/domain.hpp"
#include "http_client.hpp"

namespace app {

class PlacesClient {
   public:
    explicit PlacesClient(IHttpClient& client) : client_(client) {}

    std::expected<std::vector<Place>, std::string> FetchPlaces(
        const Coordinates& coords, int radius_m);

   private:
    IHttpClient& client_;

    std::string BuildQuery(const Coordinates& coords, int radius_m);
    std::expected<std::vector<Place>, std::string> ParsePlaces(
        const std::string& body);
};

}  // namespace app
