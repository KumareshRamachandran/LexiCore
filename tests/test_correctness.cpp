#include "bktree.hpp"
#include "dictionary.hpp"
#include "edit_distance.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

using namespace lexicore;

/// Linear-scan baseline for fuzzy search (correctness oracle).
/// Computes edit distance against every word — guaranteed correct.
std::vector<std::pair<std::string, int>> linearFuzzySearch(
    const std::vector<std::string>& words,
    const std::string& query,
    int maxDistance) {
    std::vector<std::pair<std::string, int>> results;
    for (const auto& w : words) {
        int d = editDistance(query, w);
        if (d <= maxDistance) {
            results.emplace_back(w, d);
        }
    }
    return results;
}

/// Compare two result sets for equality (order-independent).
bool resultSetsMatch(std::vector<std::pair<std::string, int>> a,
                     std::vector<std::pair<std::string, int>> b) {
    auto cmp = [](const std::pair<std::string, int>& x,
                  const std::pair<std::string, int>& y) {
        return x.first < y.first || (x.first == y.first && x.second < y.second);
    };
    std::sort(a.begin(), a.end(), cmp);
    std::sort(b.begin(), b.end(), cmp);
    return a == b;
}

void testSmallDictionary() {
    std::vector<std::string> words = {
        "book", "cook", "hook", "look", "back", "bake",
        "take", "make", "lake", "cake", "apple", "apply",
        "ample", "simple", "sample", "temple", "example"
    };

    // Build BK-tree with shuffled insertion.
    std::vector<std::string> shuffled = words;
    std::mt19937 rng(42);
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    BKTree bktree;
    for (const auto& w : shuffled) {
        bktree.insert(w);
    }

    // Test multiple queries and thresholds.
    std::vector<std::string> queries = {
        "book", "cook", "apple", "aple", "bak", "xyz",
        "mple", "sampl", "", "examplee", "booook"
    };

    int mismatches = 0;
    for (const auto& query : queries) {
        for (int maxDist = 0; maxDist <= 3; ++maxDist) {
            auto linearResults = linearFuzzySearch(words, query, maxDist);
            auto bkResults = bktree.search(query, maxDist);

            if (!resultSetsMatch(linearResults, bkResults)) {
                std::cerr << "  ✗ MISMATCH: query=\"" << query
                          << "\", maxDist=" << maxDist << "\n";
                std::cerr << "    Linear found " << linearResults.size()
                          << " results, BK-tree found " << bkResults.size() << "\n";

                // Show differences.
                std::unordered_set<std::string> linearWords, bkWords;
                for (const auto& [w, d] : linearResults) linearWords.insert(w);
                for (const auto& [w, d] : bkResults) bkWords.insert(w);

                for (const auto& w : linearWords) {
                    if (!bkWords.count(w)) {
                        std::cerr << "    Missing from BK-tree: " << w << "\n";
                    }
                }
                for (const auto& w : bkWords) {
                    if (!linearWords.count(w)) {
                        std::cerr << "    Extra in BK-tree: " << w << "\n";
                    }
                }
                ++mismatches;
            }
        }
    }

    if (mismatches == 0) {
        std::cout << "  ✓ Small dictionary oracle (" << queries.size()
                  << " queries × 4 thresholds)\n";
    } else {
        std::cerr << "  ✗ " << mismatches << " mismatches found!\n";
        std::abort();
    }
}

void testRandomizedOracle() {
    // Build a moderate dictionary from random words.
    std::mt19937 rng(99999);
    std::uniform_int_distribution<int> charDist('a', 'z');
    std::uniform_int_distribution<size_t> lenDist(2, 8);

    std::vector<std::string> words;
    std::unordered_set<std::string> seen;
    while (words.size() < 500) {
        size_t len = lenDist(rng);
        std::string w;
        for (size_t i = 0; i < len; ++i) {
            w += static_cast<char>(charDist(rng));
        }
        if (seen.insert(w).second) {
            words.push_back(w);
        }
    }

    // Build BK-tree (shuffled).
    auto shuffled = words;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    BKTree bktree;
    for (const auto& w : shuffled) {
        bktree.insert(w);
    }

    // Generate random queries (mix of dictionary words and mutations).
    std::uniform_int_distribution<size_t> idxDist(0, words.size() - 1);
    int numQueries = 200;
    int mismatches = 0;

    for (int i = 0; i < numQueries; ++i) {
        std::string query = words[idxDist(rng)];
        // Randomly mutate half the queries.
        if (i % 2 == 0 && !query.empty()) {
            size_t pos = std::uniform_int_distribution<size_t>(0, query.size() - 1)(rng);
            query[pos] = static_cast<char>(charDist(rng));
        }

        for (int maxDist = 0; maxDist <= 3; ++maxDist) {
            auto linearResults = linearFuzzySearch(words, query, maxDist);
            auto bkResults = bktree.search(query, maxDist);

            if (!resultSetsMatch(linearResults, bkResults)) {
                std::cerr << "  ✗ Randomized mismatch: query=\"" << query
                          << "\", maxDist=" << maxDist
                          << " (linear=" << linearResults.size()
                          << ", bk=" << bkResults.size() << ")\n";
                ++mismatches;
            }
        }
    }

    if (mismatches == 0) {
        std::cout << "  ✓ Randomized oracle (500 words, " << numQueries
                  << " queries × 4 thresholds)\n";
    } else {
        std::cerr << "  ✗ " << mismatches << " mismatches in randomized oracle!\n";
        std::abort();
    }
}

void testEdgeCases() {
    BKTree bktree;
    std::vector<std::string> words = {"a", "ab", "abc", "abcd"};

    for (const auto& w : words) {
        bktree.insert(w);
    }

    // Empty query.
    {
        auto linear = linearFuzzySearch(words, "", 1);
        auto bk = bktree.search("", 1);
        assert(resultSetsMatch(linear, bk));
    }

    // Query longer than any word.
    {
        auto linear = linearFuzzySearch(words, "abcdefgh", 2);
        auto bk = bktree.search("abcdefgh", 2);
        assert(resultSetsMatch(linear, bk));
    }

    // maxDist = 0 (exact match).
    {
        auto linear = linearFuzzySearch(words, "abc", 0);
        auto bk = bktree.search("abc", 0);
        assert(resultSetsMatch(linear, bk));
    }

    std::cout << "  ✓ Edge cases (empty query, long query, maxDist=0)\n";
}

int main() {
    std::cout << "=== Correctness Oracle: BK-tree vs Linear Scan ===\n";

    testSmallDictionary();
    testRandomizedOracle();
    testEdgeCases();

    std::cout << "\nAll correctness tests passed. BK-tree matches linear baseline.\n";
    return 0;
}
