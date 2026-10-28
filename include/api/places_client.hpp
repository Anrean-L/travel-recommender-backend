#pragma once

#include <expected>
#include <string>
#include <utility>

#include "app/domain.hpp"
#include "http_client.hpp"

namespace app {

class PlacesClient {
   public:
    PlacesClient(IHttpClient& client, std::string api_key)
        : client_(client), api_key_(std::move(api_key)) {}

    std::expected<std::vector<Place>, std::string> FetchPlaces(
        const Coordinates& coords, int radius_m);

   private:
    IHttpClient& client_;

    std::string api_key_;
    std::expected<std::vector<Place>, std::string> ParsePlaces(
        const std::string& body);
};

}  // namespace app
