#include "edit_distance.hpp"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace lexicore {

int editDistance(const std::string& a, const std::string& b) {
    const size_t n = a.size();
    const size_t m = b.size();

    // Use a single row + previous row to save memory (still O(m) space).
    std::vector<int> prev(m + 1);
    std::vector<int> curr(m + 1);

    // Base case: transforming empty prefix of a into first j chars of b.
    for (size_t j = 0; j <= m; ++j) {
        prev[j] = static_cast<int>(j);
    }

    for (size_t i = 1; i <= n; ++i) {
        curr[0] = static_cast<int>(i);
        for (size_t j = 1; j <= m; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] = std::min({
                prev[j] + 1,         // deletion
                curr[j - 1] + 1,     // insertion
                prev[j - 1] + cost   // substitution
            });
        }
        std::swap(prev, curr);
    }

    return prev[m];
}

int editDistanceBounded(const std::string& a, const std::string& b, int maxDist) {
    const int n = static_cast<int>(a.size());
    const int m = static_cast<int>(b.size());

    // If the length difference alone exceeds maxDist, bail out early.
    if (std::abs(n - m) > maxDist) {
        return maxDist + 1;
    }

    // Use two rows, only computing within the diagonal band [j-maxDist, j+maxDist].
    std::vector<int> prev(m + 1, maxDist + 1);
    std::vector<int> curr(m + 1, maxDist + 1);

    // Base case row.
    for (int j = 0; j <= std::min(m, maxDist); ++j) {
        prev[j] = j;
    }

    for (int i = 1; i <= n; ++i) {
        curr.assign(m + 1, maxDist + 1);

        // Column bounds for the diagonal band.
        int jMin = std::max(1, i - maxDist);
        int jMax = std::min(m, i + maxDist);

        if (jMin == 1) {
            curr[0] = i;
        }

        bool anyValid = false;

        for (int j = jMin; j <= jMax; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;

            int del = prev[j] + 1;
            int ins = curr[j - 1] + 1;
            int sub = prev[j - 1] + cost;

            curr[j] = std::min({del, ins, sub});

            if (curr[j] <= maxDist) {
                anyValid = true;
            }
        }

        // Early termination: if no cell in this row can produce a valid
        // result, the final distance must exceed maxDist.
        if (!anyValid) {
            return maxDist + 1;
        }

        std::swap(prev, curr);
    }

    return (prev[m] <= maxDist) ? prev[m] : maxDist + 1;
}

} // namespace lexicore
