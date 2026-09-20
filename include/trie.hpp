#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lexicore {

/// Trie node for prefix-based retrieval.
/// Memory ownership: each parent uniquely owns its children via unique_ptr.
/// Destruction is automatic and top-down.
struct TrieNode {
    std::unordered_map<char, std::unique_ptr<TrieNode>> children;
    bool isEndOfWord = false;
};

/// Trie data structure for O(k) prefix search and autocomplete.
/// k = length of the query/prefix, independent of dictionary size.
class Trie {
public:
    Trie();

    /// Insert a word into the trie.
    void insert(const std::string& word);

    /// Exact match: is this word in the trie?
    bool search(const std::string& word) const;

    /// Does any word in the trie start with this prefix?
    bool startsWith(const std::string& prefix) const;

    /// Returns up to `limit` words matching the given prefix,
    /// sorted lexicographically. Cap prevents terminal flood
    /// on broad queries like "prefix a".
    std::vector<std::string> autocomplete(const std::string& prefix,
                                          size_t limit = 20) const;

    /// Total number of nodes in the trie (for memory profiling).
    size_t getNodeCount() const;

private:
    /// Traverse to the node representing the given prefix.
    /// Returns nullptr if the prefix doesn't exist in the trie.
    const TrieNode* findNode(const std::string& prefix) const;

    /// DFS from the given node, collecting completed words up to limit.
    void collectWords(const TrieNode* node, const std::string& prefix,
                      std::vector<std::string>& results, size_t limit) const;

    /// Count nodes recursively.
    size_t countNodes(const TrieNode* node) const;

    std::unique_ptr<TrieNode> root_;
};

} // namespace lexicore
