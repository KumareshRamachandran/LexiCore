#pragma once

#include <string>

namespace lexicore {

/// Standard Levenshtein edit distance between two strings.
/// Operations: insertion, deletion, substitution (each cost 1).
/// Complexity: O(n·m) time and space where n, m are string lengths.
int editDistance(const std::string& a, const std::string& b);

/// Threshold-aware Levenshtein edit distance.
/// Only computes within a diagonal band of width 2*maxDist+1.
/// Returns the true distance if distance <= maxDist,
/// otherwise returns maxDist+1 as a sentinel (NOT the true distance).
///
/// IMPORTANT: The return value when distance > maxDist is a sentinel,
/// not the actual distance. Never use this sentinel for BK-tree pruning
/// interval calculation — use editDistance() for that.
int editDistanceBounded(const std::string& a, const std::string& b, int maxDist);

} // namespace lexicore
