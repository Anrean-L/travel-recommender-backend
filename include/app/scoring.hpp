#pragma once

#include <vector>

#include "app/domain.hpp"

namespace app {

double HaversineDistanceMeters(const Coordinates& a, const Coordinates& b);

std::vector<ScoredPlace> ScorePlaces(const std::vector<Place>& places,
                                     const WeatherInfo& weather,
                                     const Coordinates& origin);

std::vector<ScoredPlace> RerankWithRoutes(std::vector<ScoredPlace> places,
                                          const WeatherInfo& weather);

}  // namespace app