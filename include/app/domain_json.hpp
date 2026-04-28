#pragma once

#include <nlohmann/json.hpp>

#include "app/domain.hpp"
#include "domain.hpp"

namespace app {

NLOHMANN_JSON_SERIALIZE_ENUM(WeatherMood,
                             {
                                 {WeatherMood::kTerrible, "terrible"},
                                 {WeatherMood::kBad, "bad"},
                                 {WeatherMood::kOkay, "okey"},
                                 {WeatherMood::kGreatOutdoor, "great"},
                             })

NLOHMANN_JSON_SERIALIZE_ENUM(PlaceKind,
                             {
                                 {PlaceKind::kUnknown, "unknown"},
                                 {PlaceKind::kMuseum, "museum"},
                                 {PlaceKind::kGallery, "gallery"},
                                 {PlaceKind::kViewpoint, "viewpoint"},
                                 {PlaceKind::kCafe, "cafe"},
                                 {PlaceKind::kRestaurant, "restaurant"},
                                 {PlaceKind::kFastFood, "fast_food"},
                                 {PlaceKind::kBar, "bar"},
                                 {PlaceKind::kPub, "pub"},
                                 {PlaceKind::kNightclub, "nightclub"},
                                 {PlaceKind::kPark, "park"},
                                 {PlaceKind::kGarden, "garden"},
                                 {PlaceKind::kBakery, "bakery"},
                                 {PlaceKind::kHistoric, "historic"},
                                 {PlaceKind::kTheatre, "theatre"},
                                 {PlaceKind::kCinema, "cinema"},
                             })

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Coordinates, lat, lon)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Place, name, kind, environment, coords,
                                   osm_type, osm_id, address, website,
                                   opening_hours)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ScoredPlace, place, score,
                                   straight_distance_m, route_distance_m,
                                   walk_time_min, reason)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WeatherInfo, temperature,
                                   apparent_temperature, relative_humidity,
                                   rain, snowfall, wind_speed, wind_direction,
                                   description)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RecommendationResponse, location, weather,
                                   places)

}  // namespace app