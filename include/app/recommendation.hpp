#pragma once

#include <vector>

#include "domain.hpp"

namespace app {

std::vector<ScoredPlace> SelectDiverseTopCandidates(
    std::vector<ScoredPlace> places, std::size_t max_count, int max_per_kind);

}  // namespace app
