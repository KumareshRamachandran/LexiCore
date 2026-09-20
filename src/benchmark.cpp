#include "benchmark.hpp"
#include "bktree.hpp"
#include "edit_distance.hpp"
#include "trie.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace lexicore {

namespace {

/// Generate a misspelled version of a word by applying a random mutation.
std::string mutateWord(const std::string& word, std::mt19937& rng) {
    if (word.empty()) return word;

    std::uniform_int_distribution<int> opDist(0, 2);
    std::uniform_int_distribution<int> charDist('a', 'z');

    std::string result = word;
    int op = opDist(rng);
    std::uniform_int_distribution<size_t> posDist(0, result.size() - 1);
    size_t pos = posDist(rng);

    switch (op) {
        case 0: // substitution
            result[pos] = static_cast<char>(charDist(rng));
            break;
        case 1: // insertion
            result.insert(result.begin() + static_cast<long>(pos),
                          static_cast<char>(charDist(rng)));
            break;
        case 2: // deletion
            if (result.size() > 1) {
                result.erase(pos, 1);
            }
            break;
    }
    return result;
}

/// Generate a set of queries by sampling from a word list and optionally mutating.
std::vector<std::string> generateQueries(const std::vector<std::string>& words,
                                          size_t count, unsigned seed,
                                          bool mutate = false) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<size_t> dist(0, words.size() - 1);

    std::vector<std::string> queries;
    queries.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        std::string q = words[dist(rng)];
        if (mutate) {
            q = mutateWord(q, rng);
        }
        queries.push_back(q);
    }
    return queries;
}

/// Generate prefix queries by taking prefixes of random words.
std::vector<std::string> generatePrefixQueries(const std::vector<std::string>& words,
                                                size_t count, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<size_t> wordDist(0, words.size() - 1);

    std::vector<std::string> queries;
    queries.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const std::string& word = words[wordDist(rng)];
        if (word.size() <= 1) {
            queries.push_back(word);
        } else {
            std::uniform_int_distribution<size_t> lenDist(1, std::min(word.size(), size_t(5)));
            queries.push_back(word.substr(0, lenDist(rng)));
        }
    }
    return queries;
}

struct TimingResult {
    double totalMs;
    double avgUs;
    double queriesPerSec;
    double p50Us;
    double p95Us;
    double p99Us;
};

/// Time a batch of operations, collecting per-query latencies.
template <typename Func>
TimingResult timeQueries(const std::vector<std::string>& queries, Func&& func) {
    std::vector<double> latencies;
    latencies.reserve(queries.size());

    auto totalStart = std::chrono::high_resolution_clock::now();

    for (const auto& q : queries) {
        auto start = std::chrono::high_resolution_clock::now();
        func(q);
        auto end = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(us);
    }

    auto totalEnd = std::chrono::high_resolution_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(totalEnd - totalStart).count();

    std::sort(latencies.begin(), latencies.end());

    size_t n = latencies.size();
    auto percentile = [&](double p) -> double {
        size_t idx = static_cast<size_t>(p * static_cast<double>(n - 1));
        return latencies[std::min(idx, n - 1)];
    };

    double avgUs = totalMs * 1000.0 / static_cast<double>(n);
    double qps = static_cast<double>(n) / (totalMs / 1000.0);

    return {totalMs, avgUs, qps, percentile(0.50), percentile(0.95), percentile(0.99)};
}

/// Time index construction.
template <typename Func>
double timeBuildMs(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

void printHeader(const std::string& title) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(70, '=') << "\n";
}

void printTimingRow(const std::string& label, const TimingResult& t) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  " << std::left << std::setw(25) << label
              << " | " << std::setw(10) << t.totalMs << " ms"
              << " | avg " << std::setw(8) << t.avgUs << " µs"
              << " | " << std::setw(10) << t.queriesPerSec << " q/s"
              << "\n";
    std::cout << "  " << std::setw(25) << ""
              << " | p50: " << std::setw(8) << t.p50Us << " µs"
              << " | p95: " << std::setw(8) << t.p95Us << " µs"
              << " | p99: " << std::setw(8) << t.p99Us << " µs"
              << "\n";
}

} // anonymous namespace

void runBenchmark(const Dictionary& dict, const BenchmarkConfig& config) {
    const auto& allWords = dict.getWords();
    const auto& wordSet = dict.getWordSet();

    std::cout << "\n╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  LexiCore Benchmark Suite                                     ║\n";
    std::cout << "║  Trials: " << config.numTrials
              << " (median) | Queries/trial: " << config.numQueries
              << std::setw(20) << "║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";

    for (size_t dictSize : config.dictSizes) {
        // Get subset or full dictionary.
        std::vector<std::string> words;
        if (dictSize == 0 || dictSize >= allWords.size()) {
            words = allWords;
            dictSize = allWords.size();
        } else {
            words = dict.getSubset(dictSize, config.seed);
        }

        printHeader("Dictionary size: " + std::to_string(dictSize) + " words");

        // Generate query sets (same queries for all strategies).
        auto exactQueries = generateQueries(words, config.numQueries, config.seed + 1);
        auto prefixQueries = generatePrefixQueries(words, config.numQueries, config.seed + 2);
        auto fuzzyQueries = generateQueries(words, config.numQueries, config.seed + 3, true);

        // Build structures.
        std::unordered_set<std::string> hashSet(words.begin(), words.end());

        Trie trie;
        double trieBuildMs = timeBuildMs([&]() {
            for (const auto& w : words) trie.insert(w);
        });

        auto shuffled = dict.getShuffledWords(config.seed);
        if (dictSize < allWords.size()) {
            // Use subset for BK-tree too, but shuffled.
            std::unordered_set<std::string> subset(words.begin(), words.end());
            shuffled.erase(
                std::remove_if(shuffled.begin(), shuffled.end(),
                               [&](const std::string& w) { return subset.find(w) == subset.end(); }),
                shuffled.end());
        }

        BKTree bktree;
        double bkBuildMs = timeBuildMs([&]() {
            for (const auto& w : shuffled) bktree.insert(w);
        });

        // Structure diagnostics.
        std::cout << "\n  Build times:\n";
        std::cout << "    Trie:    " << std::fixed << std::setprecision(2) << trieBuildMs << " ms"
                  << "  (" << trie.getNodeCount() << " nodes)\n";
        std::cout << "    BK-tree: " << bkBuildMs << " ms"
                  << "  (" << bktree.getNodeCount() << " nodes"
                  << ", max depth: " << bktree.getMaxDepth() << ")\n";

        // --- Exact: Linear scan vs Hash lookup ---
        std::cout << "\n  [Exact Match] Linear scan vs Hash lookup\n";
        {
            // Warm-up.
            for (const auto& q : exactQueries) {
                std::find(words.begin(), words.end(), q);
                hashSet.count(q);
            }

            auto linearResult = timeQueries(exactQueries, [&](const std::string& q) {
                volatile bool found = std::find(words.begin(), words.end(), q) != words.end();
                (void)found;
            });
            printTimingRow("Linear scan", linearResult);

            auto hashResult = timeQueries(exactQueries, [&](const std::string& q) {
                volatile bool found = hashSet.count(q) > 0;
                (void)found;
            });
            printTimingRow("Hash lookup", hashResult);
        }

        // --- Prefix: Linear prefix scan vs Trie ---
        std::cout << "\n  [Prefix Search] Linear prefix scan vs Trie\n";
        {
            // Warm-up.
            for (const auto& q : prefixQueries) {
                for (const auto& w : words) {
                    if (w.substr(0, q.size()) == q) break;
                }
                trie.autocomplete(q, 20);
            }

            auto linearResult = timeQueries(prefixQueries, [&](const std::string& q) {
                std::vector<std::string> matches;
                size_t count = 0;
                for (const auto& w : words) {
                    if (w.size() >= q.size() && w.compare(0, q.size(), q) == 0) {
                        matches.push_back(w);
                        if (++count >= 20) break;
                    }
                }
            });
            printTimingRow("Linear prefix scan", linearResult);

            auto trieResult = timeQueries(prefixQueries, [&](const std::string& q) {
                auto results = trie.autocomplete(q, 20);
                (void)results;
            });
            printTimingRow("Trie autocomplete", trieResult);
        }

        // --- Fuzzy: Linear edit-distance scan vs BK-tree ---
        std::cout << "\n  [Fuzzy Search] Linear scan vs BK-tree (maxDist="
                  << config.fuzzyMaxDistance << ")\n";
        {
            // Warm-up.
            for (size_t i = 0; i < std::min(size_t(10), fuzzyQueries.size()); ++i) {
                for (const auto& w : words) {
                    editDistance(fuzzyQueries[i], w);
                }
                bktree.search(fuzzyQueries[i], config.fuzzyMaxDistance);
            }

            auto linearResult = timeQueries(fuzzyQueries, [&](const std::string& q) {
                std::vector<std::pair<std::string, int>> matches;
                for (const auto& w : words) {
                    int d = editDistance(q, w);
                    if (d <= config.fuzzyMaxDistance) {
                        matches.emplace_back(w, d);
                    }
                }
            });
            printTimingRow("Linear edit-dist scan", linearResult);

            auto bkResult = timeQueries(fuzzyQueries, [&](const std::string& q) {
                auto results = bktree.search(q, config.fuzzyMaxDistance);
                (void)results;
            });
            printTimingRow("BK-tree search", bkResult);
        }
    }

    std::cout << "\n" << std::string(70, '-') << "\n";
    std::cout << "  Benchmark complete.\n\n";
}

} // namespace lexicore
