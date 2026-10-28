#pragma once

#include <cstdlib>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "api/http_client.hpp"

namespace test_support {

using json = nlohmann::json;

class ScopedEnvVar final {
   public:
    explicit ScopedEnvVar(std::string key) : key_(std::move(key)) {
        if (const char* value = std::getenv(key_.c_str())) {
            had_value_ = true;
            value_ = value;
        }
    }

    ~ScopedEnvVar() {
        if (had_value_) {
            Set(value_);
        } else {
            Unset();
        }
    }

    void Set(std::string_view value) const {
#ifdef _WIN32
        _putenv_s(key_.c_str(), std::string(value).c_str());
#else
        setenv(key_.c_str(), std::string(value).c_str(), 1);
#endif
    }

    void Unset() const {
#ifdef _WIN32
        _putenv_s(key_.c_str(), "");
#else
        unsetenv(key_.c_str());
#endif
    }

   private:
    std::string key_;
    bool had_value_ = false;
    std::string value_;
};

class ScenarioHttpClient final : public app::IHttpClient {
   public:
    app::HttpResponse Get(const std::string& path,
                          const app::QueryParams& params = {}) override {
        get_requests.push_back({path, params});

        if (path == "/data/2.5/weather") {
            ++weather_calls;
            return weather_response;
        }

        if (path == "/v2/places") {
            ++places_calls;
            return places_response;
        }

        ++geolocation_calls;
        return geolocation_response;
    }

    app::HttpResponse Post(const std::string& path,
                           const app::QueryParams& params = {}) override {
        post_requests.push_back({path, params});
        ++places_calls;
        return places_response;
    }

    app::HttpResponse Post(const std::string& path,
                           const app::HttpRequestBody& body,
                           const app::QueryParams& params = {}) override {
        post_json_requests.push_back({path, params, body});

        if (path == "/v2/matrix/foot-walking") {
            ++routing_calls;
            return routing_response;
        }

        return {0, ""};
    }

    struct RequestRecord {
        std::string path;
        app::QueryParams params;
    };

    struct JsonRequestRecord {
        std::string path;
        app::QueryParams params;
        app::HttpRequestBody body;
    };

    app::HttpResponse geolocation_response{200, ""};
    app::HttpResponse weather_response{200, ""};
    app::HttpResponse routing_response{200, ""};
    app::HttpResponse places_response{200, ""};

    int geolocation_calls = 0;
    int weather_calls = 0;
    int routing_calls = 0;
    int places_calls = 0;

    std::vector<RequestRecord> get_requests;
    std::vector<RequestRecord> post_requests;
    std::vector<JsonRequestRecord> post_json_requests;
};

inline app::HttpResponse MakeGeolocationResponse(double lat, double lon) {
    return {200,
            json{{"lat", std::to_string(lat)}, {"lon", std::to_string(lon)}}
                .dump()};
}

inline app::HttpResponse MakeWeatherResponse(
    double rain, bool is_day, double temperature = 20.0,
    double apparent_temperature = 20.0) {
    return {200,
            json{{"main",
                  {{"temp", temperature},
                   {"feels_like", apparent_temperature},
                   {"humidity", 55}}},
                 {"wind",
                  {{"speed", 8.0 / 3.6}, {"gust", 12.0 / 3.6}, {"deg", 180}}},
                 {"rain", {{"1h", rain}}},
                 {"clouds", {{"all", 20}}},
                 {"weather", json::array({{{"id", 800},
                                           {"icon", is_day ? "01d" : "01n"},
                                           {"description", "clear sky"}}})}}
                .dump()};
}

inline app::HttpResponse MakeRouteResponse(double distance_m,
                                           double duration_sec) {
    return {200, json{{"distances", json::array({json::array({distance_m})})},
                      {"durations", json::array({json::array({duration_sec})})}}
                     .dump()};
}

inline app::HttpResponse MakePlacesResponse(const json& elements) {
    return {200,
            json{{"type", "FeatureCollection"}, {"features", elements}}.dump()};
}

inline json MakePlaceElement(std::uint64_t id, std::string name,
                             std::string amenity, double lat, double lon) {
    return json{{"type", "Feature"},
                {"properties",
                 {{"place_id", std::to_string(id)},
                  {"name", std::move(name)},
                  {"lat", lat},
                  {"lon", lon},
                  {"categories", json::array({"catering." + amenity})},
                  {"datasource",
                   {{"raw",
                     {{"osm_type", "n"},
                      {"osm_id", id},
                      {"amenity", std::move(amenity)}}}}}}}};
}

}  // namespace test_support
