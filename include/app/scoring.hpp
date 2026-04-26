#pragma once

#include <string>
#include <vector>

#include "app/domain.hpp"

namespace app {

struct ScoredPlace {
    Place place;
    double score{};
    double straight_distance_m{};
    double route_distance_m{};
    double walk_time_min{};
    std::string reason;
};

double HaversineDistanceMeters(const Coordinates& a, const Coordinates& b);

std::vector<ScoredPlace> ScorePlaces(const std::vector<Place>& places,
                                     const WeatherInfo& weather,
                                     const Coordinates& origin);

std::vector<ScoredPlace> SelectTopCandidates(std::vector<ScoredPlace> places,
                                             int max_count);

std::vector<ScoredPlace> DiversifyPlaces(std::vector<ScoredPlace> places,
                                         int max_results);

}  // namespace app