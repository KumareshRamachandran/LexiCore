#include "dictionary.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>

namespace lexicore {

std::string Dictionary::normalize(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());
    for (char c : raw) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return result;
}

bool Dictionary::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string word = normalize(line);
        if (word.empty()) {
            continue;
        }
        // Deduplicate: only insert if not already seen
        if (wordSet_.insert(word).second) {
            words_.push_back(word);
        }
    }

    return true;
}

std::vector<std::string> Dictionary::getSubset(size_t n, unsigned seed) const {
    std::vector<std::string> copy = words_;
    std::mt19937 rng(seed);
    std::shuffle(copy.begin(), copy.end(), rng);
    if (n < copy.size()) {
        copy.resize(n);
    }
    return copy;
}

std::vector<std::string> Dictionary::getShuffledWords(unsigned seed) const {
    std::vector<std::string> copy = words_;
    std::mt19937 rng(seed);
    std::shuffle(copy.begin(), copy.end(), rng);
    return copy;
}

} // namespace lexicore
