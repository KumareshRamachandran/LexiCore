#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lexicore {

/// BK-tree node. Each child edge is keyed by the edit distance
/// from the parent's word to the child's word.
/// Memory ownership: unique_ptr for all children.
struct BKNode {
    std::string word;
    std::unordered_map<int, std::unique_ptr<BKNode>> children;

    explicit BKNode(std::string w) : word(std::move(w)) {}
};

/// BK-tree for fuzzy (edit-distance-based) search.
/// Uses triangle-inequality pruning to avoid checking every word.
///
/// IMPORTANT: Insert words in shuffled order to avoid degenerate
/// chain-shaped trees from alphabetically-sorted input.
class BKTree {
public:
    BKTree() = default;

    /// Insert a word into the BK-tree.
    void insert(const std::string& word);

    /// Find all words within maxDistance edit distance of the query.
    /// Returns vector of (word, distance) pairs.
    ///
    /// Uses true editDistance() for pruning interval calculation,
    /// NOT editDistanceBounded(). See implementation plan for why.
    std::vector<std::pair<std::string, int>> search(
        const std::string& query, int maxDistance) const;

    /// Total number of nodes in the tree.
    size_t getNodeCount() const;

    /// Maximum depth from root to any leaf.
    /// Useful for detecting degenerate trees from unshufffled input.
    size_t getMaxDepth() const;

    /// Whether the tree is empty (no words inserted).
    bool empty() const { return root_ == nullptr; }

private:
    void insertImpl(BKNode* node, const std::string& word);

    void searchImpl(const BKNode* node, const std::string& query,
                    int maxDistance,
                    std::vector<std::pair<std::string, int>>& results) const;

    size_t countNodes(const BKNode* node) const;
    size_t maxDepthImpl(const BKNode* node) const;

    std::unique_ptr<BKNode> root_;
};

} // namespace lexicore
