#pragma once

#include "dictionary.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace lexicore {

/// Configuration for a benchmark run.
struct BenchmarkConfig {
    std::vector<size_t> dictSizes = {1000, 10000, 0}; // 0 = full dictionary
    size_t numQueries = 1000;
    size_t numTrials = 5;
    int fuzzyMaxDistance = 2;
    unsigned seed = 42;
};

/// Run the full benchmark suite and print results.
/// Fair paired comparisons:
///   Exact:  linear scan vs hash lookup
///   Prefix: linear prefix scan vs trie
///   Fuzzy:  linear edit-distance scan vs BK-tree
void runBenchmark(const Dictionary& dict, const BenchmarkConfig& config = {});

} // namespace lexicore
