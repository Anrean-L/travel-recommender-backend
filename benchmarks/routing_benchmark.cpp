// Live comparison with the GET-per-place implementation from commit d1cb19b.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include "api/places_client.hpp"
#include "api/routing_client.hpp"
#include "app/scoring.hpp"

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

std::string UtcNow() {
    const auto now = std::time(nullptr);
    std::ostringstream out;
    out << std::put_time(std::gmtime(&now), "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

// Both approaches use the production HTTPS transport with its cache disabled.
// Rate-limit pacing is recorded separately and excluded from measured latency.
class MeasuredHttp final : public app::IHttpClient {
   public:
    explicit MeasuredHttp(int interval_ms)
        : transport_("api.openrouteservice.org", 0, std::chrono::seconds(0)),
          interval_(interval_ms) {}

    app::HttpResponse Get(const std::string& path,
                          const app::QueryParams& params = {}) override {
        return Measure([&] { return transport_.Get(path, params); });
    }
    app::HttpResponse Post(const std::string& path,
                           const app::QueryParams& params = {}) override {
        return Measure([&] { return transport_.Post(path, params); });
    }
    app::HttpResponse Post(const std::string& path,
                           const app::HttpRequestBody& body,
                           const app::QueryParams& params = {}) override {
        // Verify that the actual production client requests a 1 x N matrix.
        auto request = json::parse(body.content);
        if (request.at("sources") != json::array({0}) ||
            request.at("destinations").size() + 1 !=
                request.at("locations").size()) {
            throw std::runtime_error("Unexpected production matrix shape");
        }
        auto response =
            Measure([&] { return transport_.Post(path, body, params); });
        const auto data = json::parse(response.body);
        const auto n = request.at("destinations").size();
        for (const auto* metric : {"distances", "durations"}) {
            const auto& rows = data.at(metric);
            if (rows.size() != 1 || rows.at(0).size() != n)
                throw std::runtime_error("Matrix is not 1 x N");
            for (const auto& cell : rows.at(0)) {
                if (!cell.is_number())
                    throw std::runtime_error("Unreachable route in matrix");
            }
        }
        return response;
    }
    void Reset() {
        requests = bytes = 0;
        pacing_ms = 0;
    }
    std::size_t requests = 0, bytes = 0;
    double pacing_ms = 0;

   private:
    template <class F>
    app::HttpResponse Measure(F&& call) {
        const auto before = Clock::now();
        if (last_start_ != Clock::time_point{})
            std::this_thread::sleep_until(last_start_ + interval_);
        pacing_ms +=
            std::chrono::duration<double, std::milli>(Clock::now() - before)
                .count();
        last_start_ = Clock::now();
        ++requests;
        auto response = call();
        bytes += response.body.size();
        if (response.status != 200)
            throw std::runtime_error("openrouteservice HTTP status " +
                                     std::to_string(response.status));
        return response;
    }
    app::HttplibHttpClient transport_;
    std::chrono::milliseconds interval_;
    Clock::time_point last_start_{};
};

std::string Key(const char* name) {
    const auto* value = std::getenv(name);
    if (!value || !*value)
        throw std::runtime_error(std::string("Missing ") + name);
    return value;
}

void Write(const std::string& path, const json& data) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Cannot write output");
    json recorded = data;
    recorded["recorded_at_utc"] = UtcNow();
    out << recorded.dump(2) << '\n';
}

void Capture(const std::string& path) {
    app::HttplibHttpClient http("api.geoapify.com", 0, std::chrono::seconds(0));
    app::PlacesClient client(http, Key("GEOAPIFY_KEY"));
    const app::Coordinates origin{59.93, 30.3351};
    auto places = client.FetchPlaces(origin, 1000);
    if (!places) throw std::runtime_error(places.error());
    app::WeatherInfo weather{};
    weather.temperature = weather.apparent_temperature = 20;
    weather.is_day = true;
    weather.mood = app::WeatherMood::kGreatOutdoor;
    auto scored = app::ScorePlaces(*places, weather, origin);
    std::stable_sort(
        scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.score > b.score; });
    json selected = json::array();
    std::map<app::PlaceKind, int> counts;
    for (const auto& candidate : scored) {
        const auto& place = candidate.place;
        if (counts[place.kind] >= 8) continue;
        ++counts[place.kind];
        selected.push_back({{"name", place.name},
                            {"lat", place.coords.lat},
                            {"lon", place.coords.lon},
                            {"osm_type", place.osm_type},
                            {"osm_id", place.osm_id}});
        if (selected.size() == 40) break;
    }
    Write(path,
          {{"source", "Geoapify Places, OpenStreetMap data"},
           {"origin", {{"lat", origin.lat}, {"lon", origin.lon}}},
           {"radius_m", 1000},
           {"fetched_places", places->size()},
           {"selection",
            "Production ScorePlaces, sunny 20 C; top 40, at most 8 per kind"},
           {"places", selected}});
    std::cout << "Captured " << selected.size() << " candidates\n";
}

std::vector<app::ScoredPlace> Input(const json& fixture, std::size_t n) {
    std::vector<app::ScoredPlace> places;
    for (std::size_t i = 0; i < n; ++i) {
        const auto& p = fixture.at("places").at(i);
        app::ScoredPlace place{};
        place.place.name = p.at("name");
        place.place.coords = {p.at("lat"), p.at("lon")};
        places.push_back(place);
    }
    return places;
}

// Reproduces d1cb19b: one synchronous directions GET for each destination.
std::vector<app::RouteInfo> Sequential(
    MeasuredHttp& http, const std::string& key, app::Coordinates origin,
    const std::vector<app::ScoredPlace>& places) {
    const auto point = [](app::Coordinates p) {
        return std::to_string(p.lon) + "," + std::to_string(p.lat);
    };
    std::vector<app::RouteInfo> routes;
    for (const auto& place : places) {
        auto response = http.Get("/v2/directions/foot-walking",
                                 {{"api_key", key},
                                  {"start", point(origin)},
                                  {"end", point(place.place.coords)}});
        const auto data = json::parse(response.body);
        const auto& summary =
            data.at("features").at(0).at("properties").at("summary");
        routes.push_back({summary.at("distance"), summary.at("duration")});
    }
    return routes;
}

double Median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    auto n = values.size();
    return n % 2 ? values[n / 2] : (values[n / 2 - 1] + values[n / 2]) / 2;
}

int main(int argc, char** argv) {
    json report;
    std::string output = "benchmarks/routing_results.json";
    try {
        std::string fixture_path = "benchmarks/places_spb.json";
        int repeats = 3, interval_ms = 1600;
        std::vector<std::size_t> sizes{10, 20, 40};
        for (int i = 1; i < argc; ++i) {
            const std::string flag = argv[i];
            if (i + 1 >= argc)
                throw std::runtime_error("Expected option value");
            const std::string value = argv[++i];
            if (flag == "--capture") {
                Capture(value);
                return 0;
            }
            if (flag == "--fixture")
                fixture_path = value;
            else if (flag == "--output")
                output = value;
            else if (flag == "--repeats")
                repeats = std::stoi(value);
            else if (flag == "--interval-ms")
                interval_ms = std::stoi(value);
            else if (flag == "--sizes") {
                sizes.clear();
                std::stringstream list(value);
                std::string item;
                while (std::getline(list, item, ',')) {
                    const int size = std::stoi(item);
                    if (size < 1)
                        throw std::runtime_error("Invalid workload size");
                    sizes.push_back(size);
                }
            } else
                throw std::runtime_error("Unknown option");
        }
        if (repeats < 1 || interval_ms < 0)
            throw std::runtime_error("Invalid option value");
        std::ifstream input(fixture_path);
        const auto fixture = json::parse(input);
        if (sizes.empty()) throw std::runtime_error("No workload sizes");
        for (auto n : sizes) {
            if (n > fixture.at("places").size())
                throw std::runtime_error("Not enough fixture places");
        }
        const auto key = Key("ORS_API_KEY");
        const app::Coordinates origin{fixture.at("origin").at("lat"),
                                      fixture.at("origin").at("lon")};
        MeasuredHttp http(interval_ms);
        app::RoutingClient matrix_client(http, key);
        report =
            json{{"fixture", fixture_path},   {"baseline_commit", "d1cb19b"},
                 {"cache_enabled", false},    {"interval_ms", interval_ms},
                 {"repeats", repeats},        {"run_status", "running"},
                 {"scenarios", json::array()}};
        Write(output, report);
        for (std::size_t n : sizes) {
            auto places = Input(fixture, n);
            report["scenarios"].push_back({{"places", n},
                                           {"complete", false},
                                           {"trials", json::array()}});
            auto& scenario = report["scenarios"].back();
            std::vector<double> serial_times, matrix_times;
            for (int trial = 0; trial < repeats; ++trial) {
                json sample;
                std::vector<app::RouteInfo> sequential;
                std::vector<app::ScoredPlace> matrix;
                // Alternate order to reduce systematic first-request bias.
                for (int step = 0; step < 2; ++step) {
                    const bool is_matrix = (step + trial) % 2 != 0;
                    report["active"] = {
                        {"places", n},
                        {"trial", trial + 1},
                        {"method", is_matrix ? "matrix" : "sequential"}};
                    http.Reset();
                    const auto start = Clock::now();
                    if (is_matrix)
                        matrix = matrix_client.AddRoutes(origin, places);
                    else
                        sequential = Sequential(http, key, origin, places);
                    const double wall =
                        std::chrono::duration<double, std::milli>(Clock::now() -
                                                                  start)
                            .count();
                    const double active = wall - http.pacing_ms;
                    const std::size_t expected = is_matrix ? 1 : n;
                    if (http.requests != expected)
                        throw std::runtime_error("Unexpected HTTP call count");
                    sample[is_matrix ? "matrix" : "sequential"] = {
                        {"requests", http.requests},
                        {"response_bytes", http.bytes},
                        {"latency_ms", active},
                        {"pacing_ms", http.pacing_ms},
                        {"wall_ms", wall}};
                    (is_matrix ? matrix_times : serial_times).push_back(active);
                }
                double max_distance_delta = 0, max_duration_delta = 0;
                for (std::size_t i = 0; i < n; ++i) {
                    max_distance_delta =
                        std::max(max_distance_delta,
                                 std::abs(sequential[i].distance_m -
                                          matrix[i].route_distance_m));
                    max_duration_delta =
                        std::max(max_duration_delta,
                                 std::abs(sequential[i].duration_sec -
                                          matrix[i].walk_time_min * 60));
                }
                sample["max_distance_delta_m"] = max_distance_delta;
                sample["max_duration_delta_s"] = max_duration_delta;
                scenario["trials"].push_back(sample);
                Write(output, report);
                std::cout << "Completed N=" << n << " trial=" << trial + 1
                          << std::endl;
            }
            const double serial = Median(serial_times),
                         matrix = Median(matrix_times);
            scenario["complete"] = true;
            scenario["sequential_median_ms"] = serial;
            scenario["matrix_median_ms"] = matrix;
            scenario["speedup"] = serial / matrix;
            Write(output, report);
        }
        report["run_status"] = "complete";
        report.erase("active");
        Write(output, report);
        std::cout << report.dump(2) << '\n';
    } catch (const std::exception&) {
        // Never print transport exception details: query strings contain keys.
        if (report.is_object()) {
            report["run_status"] = "failed";
            try {
                Write(output, report);
            } catch (...) {
            }
        }
        std::cerr
            << "Benchmark failed; no valid timing claim for incomplete "
               "scenarios. "
               "Check credentials, quota, connectivity and fixture size.\n";
        return 1;
    }
}
