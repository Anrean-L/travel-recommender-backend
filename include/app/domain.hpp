#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace app {

struct Coordinates {
    double lat{};
    double lon{};
};

enum class WeatherMood {
    kGreatOutdoor,
    kOkay,
    kBad,
    kTerrible,
};

struct WeatherInfo {
    double temperature{};
    double apparent_temperature{};
    double relative_humidity{};
    double rain{};
    double snowfall{};
    double wind_speed{};
    double wind_gusts{};
    int wind_direction{};
    int weather_code{};
    int cloud_cover{};
    bool is_day{};

    WeatherMood mood;
    std::string description;
};

enum class PlaceKind {
    kMuseum,
    kGallery,
    kViewpoint,
    kCafe,
    kRestaurant,
    kFastFood,
    kBar,
    kPub,
    kNightclub,
    kPark,
    kGarden,
    kBakery,
    kHistoric,
    kTheatre,
    kCinema,
    kUnknown,
};

constexpr bool HasNightBonus(PlaceKind kind) {
    switch (kind) {
        case PlaceKind::kBar:
        case PlaceKind::kPub:
        case PlaceKind::kNightclub:
        case PlaceKind::kTheatre:
        case PlaceKind::kCinema:
            return true;
        default:
            return false;
    }
}

enum class PlaceEnvironment {
    kIndoor,
    kOutdoor,
    kMixed,
};
struct Place {
    std::string name;
    PlaceKind kind;
    PlaceEnvironment environment;
    Coordinates coords;

    std::string osm_type;
    uint64_t osm_id{};

    std::string address;
    std::string website;
    std::string opening_hours;
};

struct ScoredPlace {
    Place place;
    double score{};
    double straight_distance_m{};
    double route_distance_m{};
    double walk_time_min{};
    std::string reason;
};

struct RouteInfo {
    double distance_m{};
    double duration_sec{};
};

struct RecommendationResponse {
    Coordinates location{};
    WeatherInfo weather{};
    std::vector<ScoredPlace> places{};
};

struct RecommendationRequest {
    bool has_coordinates = false;
    Coordinates coords{};

    std::string client_ip;

    int radius_m = 1000;
    int max_results = 10;
};

}  // namespace app
