#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <cstddef>

namespace lexicore {

/// Loads, normalizes, and deduplicates a plain-text word list.
///
/// Normalization policy: lowercase everything, strip non-alpha characters,
/// skip blank lines, deduplicate (first occurrence wins).
class Dictionary {
public:
    /// Loads words from the given file path.
    /// Returns false if the file cannot be opened.
    bool loadFromFile(const std::string& filepath);

    /// Returns the full word list (load order, deduplicated).
    const std::vector<std::string>& getWords() const { return words_; }

    /// Returns the exact-lookup set (O(1) membership test).
    const std::unordered_set<std::string>& getWordSet() const { return wordSet_; }

    /// Returns total number of unique words loaded.
    size_t size() const { return words_.size(); }

    /// Returns a seeded random sample of N words from the full dictionary.
    /// Uses std::shuffle with the given seed for reproducibility.
    /// If n >= size(), returns a shuffled copy of all words.
    std::vector<std::string> getSubset(size_t n, unsigned seed) const;

    /// Returns all words in shuffled order (for BK-tree insertion).
    /// Uses std::shuffle with the given seed for reproducibility.
    std::vector<std::string> getShuffledWords(unsigned seed) const;

private:
    /// Normalize a raw line: lowercase, strip non-alpha, trim.
    /// Returns empty string if the result is invalid.
    static std::string normalize(const std::string& raw);

    std::vector<std::string> words_;
    std::unordered_set<std::string> wordSet_;
};

} // namespace lexicore
