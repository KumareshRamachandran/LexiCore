#include "trie.hpp"

#include <algorithm>

namespace lexicore {

Trie::Trie() : root_(std::make_unique<TrieNode>()) {}

void Trie::insert(const std::string& word) {
    TrieNode* current = root_.get();
    for (char c : word) {
        auto& child = current->children[c];
        if (!child) {
            child = std::make_unique<TrieNode>();
        }
        current = child.get();
    }
    current->isEndOfWord = true;
}

bool Trie::search(const std::string& word) const {
    const TrieNode* node = findNode(word);
    return node != nullptr && node->isEndOfWord;
}

bool Trie::startsWith(const std::string& prefix) const {
    return findNode(prefix) != nullptr;
}

std::vector<std::string> Trie::autocomplete(const std::string& prefix,
                                             size_t limit) const {
    std::vector<std::string> results;
    const TrieNode* node = findNode(prefix);
    if (node) {
        collectWords(node, prefix, results, limit);
    }
    // Results are collected in DFS order which naturally gives
    // a consistent (though not strictly alphabetical) ordering.
    // Sort lexicographically for deterministic output.
    std::sort(results.begin(), results.end());
    return results;
}

size_t Trie::getNodeCount() const {
    return countNodes(root_.get());
}

const TrieNode* Trie::findNode(const std::string& prefix) const {
    const TrieNode* current = root_.get();
    for (char c : prefix) {
        auto it = current->children.find(c);
        if (it == current->children.end()) {
            return nullptr;
        }
        current = it->second.get();
    }
    return current;
}

void Trie::collectWords(const TrieNode* node, const std::string& prefix,
                         std::vector<std::string>& results, size_t limit) const {
    if (results.size() >= limit) {
        return;
    }
    if (node->isEndOfWord) {
        results.push_back(prefix);
    }
    for (const auto& [ch, child] : node->children) {
        if (results.size() >= limit) {
            return;
        }
        collectWords(child.get(), prefix + ch, results, limit);
    }
}

size_t Trie::countNodes(const TrieNode* node) const {
    if (!node) return 0;
    size_t count = 1;
    for (const auto& [ch, child] : node->children) {
        count += countNodes(child.get());
    }
    return count;
}

} // namespace lexicore
