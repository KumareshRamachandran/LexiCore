#pragma once

#include <string>
#include <utility>
#include <vector>

namespace lexicore {

/// A ranked search result.
struct RankedResult {
    std::string word;
    int distance;
};

/// Rank fuzzy search results deterministically.
/// Sort order:
///   1. Primary: ascending edit distance (closer matches first)
///   2. Secondary: lexicographic order (deterministic across runs)
std::vector<RankedResult> rankResults(
    const std::vector<std::pair<std::string, int>>& results);

} // namespace lexicore
