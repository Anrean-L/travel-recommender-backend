#pragma once
#include <expected>

#include "api/http_client.hpp"
#include "app/domain.hpp"

namespace app {

class GeolocationClient {
   public:
    GeolocationClient(IHttpClient& client, std::string api_key)
        : client_(client), api_key_(std::move(api_key)) {}

    std::expected<Coordinates, std::string> FetchCoordinates(
        const std::string& ip);

   private:
    IHttpClient& client_;
    std::string api_key_;
};

};  // namespace app
