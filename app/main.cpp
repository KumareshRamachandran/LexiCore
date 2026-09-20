#include "benchmark.hpp"
#include "bktree.hpp"
#include "dictionary.hpp"
#include "edit_distance.hpp"
#include "ranking.hpp"
#include "trie.hpp"

#include <iostream>
#include <sstream>
#include <string>

using namespace lexicore;

namespace {

void printBanner(size_t wordCount) {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════╗\n";
    std::cout << "║  LexiCore — Fuzzy Search Engine       ║\n";
    std::cout << "║  Dictionary: " << wordCount << " words loaded"
              << std::string(std::max(0, 6 - static_cast<int>(std::to_string(wordCount).size())), ' ')
              << "     ║\n";
    std::cout << "╚═══════════════════════════════════════╝\n\n";
}

void printHelp() {
    std::cout << "Commands:\n"
              << "  exact <word>              — exact dictionary lookup\n"
              << "  prefix <prefix> [limit]   — autocomplete (default limit: 20)\n"
              << "  fuzzy <word> [maxDist]    — fuzzy search (default maxDist: 2)\n"
              << "  benchmark                 — run performance benchmark suite\n"
              << "  help                      — show this message\n"
              << "  quit                      — exit\n\n";
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    // Determine dictionary path.
    std::string dictPath = "data/words.txt";
    if (argc > 1) {
        dictPath = argv[1];
    }

    // Load dictionary.
    Dictionary dict;
    if (!dict.loadFromFile(dictPath)) {
        std::cerr << "Error: Could not open dictionary file: " << dictPath << "\n";
        std::cerr << "Usage: lexicore [dictionary_path]\n";
        return 1;
    }

    if (dict.size() == 0) {
        std::cerr << "Error: Dictionary is empty after loading.\n";
        return 1;
    }

    // Build index structures.
    const auto& wordSet = dict.getWordSet();

    Trie trie;
    for (const auto& w : dict.getWords()) {
        trie.insert(w);
    }

    BKTree bktree;
    auto shuffled = dict.getShuffledWords(42);
    for (const auto& w : shuffled) {
        bktree.insert(w);
    }

    printBanner(dict.size());

    std::cout << "  Trie: " << trie.getNodeCount() << " nodes\n";
    std::cout << "  BK-tree: " << bktree.getNodeCount() << " nodes"
              << ", max depth: " << bktree.getMaxDepth() << "\n\n";

    printHelp();

    // Interactive loop.
    std::string line;
    while (true) {
        std::cout << "LexiCore> ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        std::istringstream iss(line);
        std::string command;
        if (!(iss >> command)) {
            continue; // empty line
        }

        if (command == "quit" || command == "exit") {
            break;
        }

        if (command == "help") {
            printHelp();
            continue;
        }

        if (command == "exact") {
            std::string word;
            if (!(iss >> word)) {
                std::cout << "Usage: exact <word>\n\n";
                continue;
            }
            if (wordSet.count(word)) {
                std::cout << "✓ Found: " << word << "\n\n";
            } else {
                std::cout << "✗ Not found: " << word << "\n\n";
            }
            continue;
        }

        if (command == "prefix") {
            std::string prefix;
            if (!(iss >> prefix)) {
                std::cout << "Usage: prefix <prefix> [limit]\n\n";
                continue;
            }
            size_t limit = 20;
            if (iss >> limit) {
                if (limit == 0) limit = 20;
            }

            // Get more than limit to know the total count.
            auto results = trie.autocomplete(prefix, limit + 1000);
            size_t total = results.size();

            // Display up to limit.
            size_t shown = std::min(total, limit);
            for (size_t i = 0; i < shown; ++i) {
                std::cout << "  " << results[i] << "\n";
            }
            if (total > limit) {
                std::cout << "  ...and " << (total - limit) << " more\n";
            }
            if (total == 0) {
                std::cout << "  No matches for prefix \"" << prefix << "\"\n";
            }
            std::cout << "\n";
            continue;
        }

        if (command == "fuzzy") {
            std::string query;
            if (!(iss >> query)) {
                std::cout << "Usage: fuzzy <word> [maxDist]\n\n";
                continue;
            }
            int maxDist = 2;
            if (iss >> maxDist) {
                if (maxDist < 0) {
                    std::cout << "Error: maxDist must be non-negative.\n\n";
                    continue;
                }
            }

            auto rawResults = bktree.search(query, maxDist);
            auto ranked = rankResults(rawResults);

            size_t limit = 20;
            size_t shown = std::min(ranked.size(), limit);
            for (size_t i = 0; i < shown; ++i) {
                std::cout << "  " << ranked[i].word
                          << "  (dist: " << ranked[i].distance << ")\n";
            }
            if (ranked.size() > limit) {
                std::cout << "  ...and " << (ranked.size() - limit) << " more\n";
            }
            if (ranked.empty()) {
                std::cout << "  No matches within distance " << maxDist
                          << " of \"" << query << "\"\n";
            }
            std::cout << "\n";
            continue;
        }

        if (command == "benchmark") {
            runBenchmark(dict);
            continue;
        }

        std::cout << "Unknown command: " << command << "\n";
        std::cout << "Type 'help' for available commands.\n\n";
    }

    return 0;
}
