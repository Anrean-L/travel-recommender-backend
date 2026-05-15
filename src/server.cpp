#include "app/server.hpp"

#include <httplib.h>

#include <expected>
#include <nlohmann/json.hpp>
#include <string>

#include "app/domain_json.hpp"
#include "app/logger.hpp"
#include "app/recommendation.hpp"

namespace app {

namespace {

using json = nlohmann::json;

void SetCorsHeaders(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

std::expected<double, std::string> ParseDoubleParam(const httplib::Request& req,
                                                    const std::string& name) {
    if (!req.has_param(name)) {
        return std::unexpected("Missing query parameter: " + name);
    }

    try {
        return std::stod(req.get_param_value(name));
    } catch (const std::exception&) {
        return std::unexpected("Invalid query parameter: " + name);
    }
}

int ParseIntParamOrDefault(const httplib::Request& req, const std::string& name,
                           int default_value) {
    if (!req.has_param(name)) {
        return default_value;
    }

    try {
        return std::stoi(req.get_param_value(name));
    } catch (const std::exception&) {
        return default_value;
    }
}

std::string GetClientIp(const httplib::Request& req) {
    std::string forwarded = req.get_header_value("X-Forwarded-For");
    if (!forwarded.empty()) {
        const std::size_t comma_pos = forwarded.find(',');
        if (comma_pos != std::string::npos) {
            return forwarded.substr(0, comma_pos);
        }
        return forwarded;
    }

    return req.remote_addr;
}

void SendJson(httplib::Response& res, const json& body, int status = 200) {
    SetCorsHeaders(res);
    res.status = status;
    res.set_content(body.dump(2), "application/json; charset=utf-8");
}

void SendError(httplib::Response& res, const std::string& message,
               int status = 500) {
    SendJson(res, json{{"error", message}}, status);
}

}  // namespace

void RunServer(int port, WeatherClient& weather_client,
               PlacesClient& places_client, RoutingClient& routing_client,
               GeolocationClient& geolocation_client) {
    httplib::Server server;

    server.Options(R"(.*)",
                   [](const httplib::Request&, httplib::Response& res) {
                       SetCorsHeaders(res);
                       res.status = 204;
                   });

    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        SetCorsHeaders(res);
        res.set_content("OK", "text/plain; charset=utf-8");
    });

    server.Get("/recommend", [&](const httplib::Request& req,
                                 httplib::Response& res) {
        LogInfo("GET /recommend from " + GetClientIp(req));
        RecommendationRequest request;

        request.radius_m = ParseIntParamOrDefault(req, "radius_m", 1000);
        request.max_results = ParseIntParamOrDefault(req, "max_results", 10);

        if (req.has_param("lat") && req.has_param("lon")) {
            auto lat = ParseDoubleParam(req, "lat");
            auto lon = ParseDoubleParam(req, "lon");
            if (!lat) {
                SendError(res, lat.error(), 400);
                return;
            }
            if (!lon) {
                SendError(res, lon.error(), 400);
                return;
            }
            request.has_coordinates = true;
            request.coords = Coordinates{.lat = *lat, .lon = *lon};
            request.client_ip = GetClientIp(req);
        } else {
            request.has_coordinates = false;
            request.client_ip = GetClientIp(req);
        }

        auto result =
            BuildRecommendations(request, weather_client, places_client,
                                 routing_client, geolocation_client);

        if (!result) {
            SendError(res, result.error(), 500);
            return;
        }

        SendJson(res, json(*result));
    });

    LogInfo("Server is starting on http://0.0.0.0:" + std::to_string(port));
    if (!server.listen("0.0.0.0", port)) {
        LogError("Failed to start server on port " + std::to_string(port));
    }
}

}  // namespace app
