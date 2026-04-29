#pragma once

#include <expected>
#include <optional>

#include "api/http_client.hpp"
#include "app/domain.hpp"
#include "http_client.hpp"

namespace app {

class RoutingClient {
   public:
    RoutingClient(IHttpClient& client, std::string api_key)
        : client_(client), api_key_(std::move(api_key)) {}

    std::vector<ScoredPlace> AddRoutes(const Coordinates& origin,
                                       std::vector<ScoredPlace> places);

   private:
    IHttpClient& client_;
    std::string api_key_;

    std::expected<std::vector<std::optional<RouteInfo>>, std::string>
    FetchRoutes(const Coordinates& origin,
                const std::vector<ScoredPlace>& places);
};

}  // namespace app
