#include "ranking.hpp"

#include <algorithm>

namespace lexicore {

std::vector<RankedResult> rankResults(
    const std::vector<std::pair<std::string, int>>& results) {
    std::vector<RankedResult> ranked;
    ranked.reserve(results.size());

    for (const auto& [word, dist] : results) {
        ranked.push_back({word, dist});
    }

    std::sort(ranked.begin(), ranked.end(),
              [](const RankedResult& a, const RankedResult& b) {
                  if (a.distance != b.distance) {
                      return a.distance < b.distance; // closer matches first
                  }
                  return a.word < b.word; // lexicographic tie-break
              });

    return ranked;
}

} // namespace lexicore
