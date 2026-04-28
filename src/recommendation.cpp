#include "app/recommendation.hpp"

#include <algorithm>
#include <exception>
#include <expected>
#include <map>
#include <vector>

#include "app/domain.hpp"
#include "app/scoring.hpp"
#include "scheduler/scheduler.hpp"

namespace app {

namespace {

class RecommendationError : public std::exception {
   public:
    explicit RecommendationError(std::string message)
        : message_(std::move(message)) {}

    const char* what() const noexcept override { return message_.c_str(); }

   private:
    std::string message_;
};

std::vector<ScoredPlace> SortCandidates(std::vector<ScoredPlace> places) {
    std::sort(places.begin(), places.end(),
              [](const ScoredPlace& lhs, const ScoredPlace& rhs) {
                  return lhs.score > rhs.score;
              });
    return places;
}

std::vector<ScoredPlace> DiversifyPlaces(std::vector<ScoredPlace> places,
                                         std::size_t max_count,
                                         std::size_t max_per_kind) {
    std::vector<ScoredPlace> result;
    std::map<PlaceKind, int> count_by_kind;

    for (ScoredPlace& place : places) {
        if (result.size() >= static_cast<std::size_t>(max_count)) {
            break;
        }
        int& count = count_by_kind[place.place.kind];
        if (count >= max_per_kind) {
            continue;
        }
        result.push_back(std::move(place));
        ++count;
    }

    return result;
}

std::vector<ScoredPlace> SelectDiverseTopCandidates(
    std::vector<ScoredPlace> places, std::size_t max_count, int max_per_kind) {
    auto sorted_places = SortCandidates(std::move(places));

    return DiversifyPlaces(sorted_places, max_count, max_per_kind);
}

template <typename T>
T Unwrap(std::expected<T, std::string> result) {
    if (!result) {
        throw RecommendationError(result.error());
    }
    return std::move(*result);
}

}  // namespace

std::expected<RecommendationResponse, std::string> BuildRecommendations(
    const RecommendationRequest& request, WeatherClient& weather_client,
    PlacesClient& places_client, RoutingClient& routing_client,
    GeolocationClient& geolocation_client) {
    try {
        scheduler::TTaskScheduler scheduler;

        auto location = scheduler.Add([&request, &geolocation_client]() {
            if (request.has_coordinates) {
                return request.coords;
            }
            return Unwrap(
                geolocation_client.FetchCoordinates(request.client_ip));
        });

        auto weather = scheduler.Add(
            [&weather_client](const Coordinates& location) {
                return Unwrap(weather_client.FetchWeather(location));
            },
            location.GetFutureResult<const Coordinates&>());

        auto places = scheduler.Add(
            [&places_client, &request](const Coordinates& location) {
                return Unwrap(
                    places_client.FetchPlaces(location, request.radius_m));
            },
            location.GetFutureResult<const Coordinates&>());

        auto scored_places = scheduler.Add(
            [](std::vector<Place> places, const WeatherInfo& weather,
               const Coordinates& location) {
                return ScorePlaces(places, weather, location);
            },
            places.GetFutureResult<std::vector<Place>>(),
            weather.GetFutureResult<const WeatherInfo&>(),
            location.GetFutureResult<const Coordinates&>());

        auto candidates = scheduler.Add(
            [](std::vector<ScoredPlace> places) {
                return SelectDiverseTopCandidates(std::move(places), 30, 8);
            },
            scored_places.GetFutureResult<std::vector<ScoredPlace>>());

        auto routed_places = scheduler.Add(
            [&routing_client](const Coordinates& location,
                              std::vector<ScoredPlace> places) {
                return routing_client.AddRoutes(location, std::move(places));
            },
            location.GetFutureResult<const Coordinates&>(),
            candidates.GetFutureResult<std::vector<ScoredPlace>>());

        auto reranked = scheduler.Add(
            [](std::vector<ScoredPlace> places, const WeatherInfo& weather) {
                return RerankWithRoutes(std::move(places), weather);
            },
            routed_places.GetFutureResult<std::vector<ScoredPlace>>(),
            weather.GetFutureResult<const WeatherInfo&>());

        auto final_places = scheduler.Add(
            [&request](std::vector<ScoredPlace> places) {
                return SelectDiverseTopCandidates(std::move(places),
                                                  request.max_results, 3);
            },
            reranked.GetFutureResult<std::vector<ScoredPlace>>());

        auto response = scheduler.Add(
            [](const Coordinates& location, const WeatherInfo& weather,
               std::vector<ScoredPlace> places) {
                return RecommendationResponse{
                    location,
                    weather,
                    std::move(places),
                };
            },
            location.GetFutureResult<const Coordinates&>(),
            weather.GetFutureResult<const WeatherInfo&>(),
            final_places.GetFutureResult<std::vector<ScoredPlace>>());

        return response.GetResultSync<RecommendationResponse>();
    } catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

}  // namespace app
