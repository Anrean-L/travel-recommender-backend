#include "app/recommendation.hpp"

#include <algorithm>
#include <map>
#include <vector>

#include "app/domain.hpp"

namespace app {

namespace {

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

}  // namespace

std::vector<ScoredPlace> SelectDiverseTopCandidates(
    std::vector<ScoredPlace> places, std::size_t max_count, int max_per_kind) {
    auto sorted_places = SortCandidates(std::move(places));

    return DiversifyPlaces(sorted_places, max_count, max_per_kind);
}

}  // namespace app
