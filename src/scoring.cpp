#include "app/scoring.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>

namespace app {

namespace {

constexpr double kEarthRadiusMeters = 6371000.0;

double ToRadians(double degrees) { return degrees * std::numbers::pi / 180.0; }

bool IsIndoor(PlaceEnvironment env) { return env == PlaceEnvironment::kIndoor; }

bool IsOutdoor(PlaceEnvironment env) {
    return env == PlaceEnvironment::kOutdoor;
}

double BaseKindScore(PlaceKind kind) {
    switch (kind) {
        case PlaceKind::kMuseum:
            return 70.0;
        case PlaceKind::kGallery:
            return 65.0;
        case PlaceKind::kAttraction:
            return 68.0;
        case PlaceKind::kViewpoint:
            return 62.0;
        case PlaceKind::kCafe:
            return 58.0;
        case PlaceKind::kRestaurant:
            return 56.0;
        case PlaceKind::kFastFood:
            return 42.0;
        case PlaceKind::kBar:
            return 48.0;
        case PlaceKind::kPub:
            return 50.0;
        case PlaceKind::kNightclub:
            return 44.0;
        case PlaceKind::kPark:
            return 64.0;
        case PlaceKind::kGarden:
            return 62.0;
        case PlaceKind::kBakery:
            return 54.0;
        case PlaceKind::kHistoric:
            return 66.0;
        case PlaceKind::kTheatre:
            return 63.0;
        case PlaceKind::kCinema:
            return 55.0;
        default:
            return 40.0;
    }
}

double WeatherBonus(const Place& place, const WeatherInfo& weather) {
    if (weather.mood == WeatherMood::kTerrible) {
        if (IsIndoor(place.environment)) {
            return 25.0;
        }
        if (IsOutdoor(place.environment)) {
            return -40.0;
        }
        return -5.0;
    }

    if (weather.mood == WeatherMood::kBad) {
        if (IsIndoor(place.environment)) {
            return 18.0;
        }
        if (IsOutdoor(place.environment)) {
            return -25.0;
        }
        return 0.0;
    }

    if (weather.mood == WeatherMood::kGreatOutdoor) {
        if (IsOutdoor(place.environment)) {
            return 22.0;
        }
        if (place.kind == PlaceKind::kCafe ||
            place.kind == PlaceKind::kBakery) {
            return 6.0;
        }
        return 0.0;
    }

    if (weather.mood == WeatherMood::kOkay) {
        if (IsOutdoor(place.environment)) {
            return 4.0;
        }
        return 0.0;
    }

    return 0.0;
}

double NightBonus(const Place& place, const WeatherInfo& weather) {
    if (weather.is_day) {
        return 0.0;
    }

    if (HasNightBonus(place.kind)) {
        return 14.0;
    }

    return 0.0;
}

double DistancePenalty(double distance_m) {
    if (distance_m < 300.0) {
        return 0.0;
    }
    if (distance_m > 2000.0) {
        return 35.0;
    }
    return (distance_m - 300.0) / 1700.0 * 35.0;
}

std::string BuildReason(const Place& place, const WeatherInfo& weather,
                        double distance_m) {
    if (weather.mood == WeatherMood::kBad ||
        weather.mood == WeatherMood::kTerrible) {
        if (IsIndoor(place.environment)) {
            return "Good indoor option for bad weather";
        }
        if (IsOutdoor(place.environment)) {
            return "Outdoor place, but weather makes it less comfortable";
        }
        return "Mixed option near you";
    }

    if (weather.mood == WeatherMood::kGreatOutdoor &&
        IsOutdoor(place.environment)) {
        return "Great outdoor option for current weather";
    }

    if (!weather.is_day && HasNightBonus(place.kind)) {
        return "Great fit for evening plans";
    }

    if (distance_m < 300.0) {
        return "Very close to your location";
    }

    return "Good match by category and distance";
}

double CloudBonus(const Place& place, const WeatherInfo& weather) {
    if (weather.is_day) {
        return 0;
    }

    if (weather.cloud_cover < 40) {
        if (place.kind == PlaceKind::kViewpoint ||
            place.kind == PlaceKind::kPark ||
            place.kind == PlaceKind::kGarden) {
            return 4.0;
        }
    }

    if (weather.cloud_cover > 85) {
        if (place.kind == PlaceKind::kViewpoint) {
            return -8.0;
        }
        if (place.kind == PlaceKind::kPark ||
            place.kind == PlaceKind::kGarden) {
            return -3.0;
        }
    }

    return 0.0;
}

}  // namespace

double HaversineDistanceMeters(const Coordinates& a, const Coordinates& b) {
    const double lat1 = ToRadians(a.lat);
    const double lat2 = ToRadians(b.lat);
    const double dlat = ToRadians(b.lat - a.lat);
    const double dlon = ToRadians(b.lon - a.lon);

    const double sin_dlat = std::sin(dlat / 2.0);
    const double sin_dlon = std::sin(dlon / 2.0);

    const double h = sin_dlat * sin_dlat +
                     std::cos(lat1) * std::cos(lat2) * sin_dlon * sin_dlon;

    const double c = 2.0 * std::atan2(std::sqrt(h), std::sqrt(1.0 - h));
    return kEarthRadiusMeters * c;
}

std::vector<ScoredPlace> ScorePlaces(const std::vector<Place>& places,
                                     const WeatherInfo& weather,
                                     const Coordinates& origin) {
    std::vector<ScoredPlace> result;
    result.reserve(places.size());

    for (const Place& place : places) {
        const double distance = HaversineDistanceMeters(origin, place.coords);

        ScoredPlace scored;
        scored.place = place;
        scored.straight_distance_m = distance;
        scored.route_distance_m = 0.0;
        scored.walk_time_min = 0.0;

        scored.score = BaseKindScore(place.kind) +
                       WeatherBonus(place, weather) +
                       NightBonus(place, weather) + CloudBonus(place, weather) -
                       DistancePenalty(distance);

        scored.reason = BuildReason(place, weather, distance);

        result.push_back(std::move(scored));
    }

    return result;
}

std::vector<ScoredPlace> SelectTopCandidates(std::vector<ScoredPlace> places,
                                             std::size_t max_count) {
    std::sort(places.begin(), places.end(),
              [](const ScoredPlace& lhs, const ScoredPlace& rhs) {
                  return lhs.score > rhs.score;
              });

    if (places.size() > max_count) {
        places.resize(max_count);
    }
    return places;
}

std::vector<ScoredPlace> DiversifyPlaces(std::vector<ScoredPlace> places,
                                         int max_results) {
    std::sort(places.begin(), places.end(),
              [](const ScoredPlace& lhs, const ScoredPlace& rhs) {
                  return lhs.score > rhs.score;
              });

    std::vector<ScoredPlace> result;
    std::map<PlaceKind, int> count_by_kind;

    for (ScoredPlace& place : places) {
        if (result.size() >= static_cast<std::size_t>(max_results)) {
            break;
        }

        int& count = count_by_kind[place.place.kind];

        if (count >= 3) {
            continue;
        }

        result.push_back(std::move(place));
        ++count;
    }

    return result;
}

}  // namespace app
