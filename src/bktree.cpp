#include "bktree.hpp"
#include "edit_distance.hpp"

#include <algorithm>

namespace lexicore {

void BKTree::insert(const std::string& word) {
    if (!root_) {
        root_ = std::make_unique<BKNode>(word);
        return;
    }
    insertImpl(root_.get(), word);
}

void BKTree::insertImpl(BKNode* node, const std::string& word) {
    int dist = editDistance(node->word, word);
    if (dist == 0) {
        return; // Duplicate word, skip.
    }
    auto it = node->children.find(dist);
    if (it == node->children.end()) {
        node->children[dist] = std::make_unique<BKNode>(word);
    } else {
        insertImpl(it->second.get(), word);
    }
}

std::vector<std::pair<std::string, int>> BKTree::search(
    const std::string& query, int maxDistance) const {
    std::vector<std::pair<std::string, int>> results;
    if (root_) {
        searchImpl(root_.get(), query, maxDistance, results);
    }
    return results;
}

void BKTree::searchImpl(const BKNode* node, const std::string& query,
                         int maxDistance,
                         std::vector<std::pair<std::string, int>>& results) const {
    // CRITICAL: Use true editDistance() here, not editDistanceBounded().
    // We need the actual distance value to compute the correct pruning
    // interval [d - maxDistance, d + maxDistance]. editDistanceBounded()
    // returns a sentinel (maxDist+1) on bail-out, which would produce
    // an incorrect pruning range and silently skip valid subtrees.
    int d = editDistance(query, node->word);

    if (d <= maxDistance) {
        results.emplace_back(node->word, d);
    }

    // Triangle inequality pruning: only recurse into children whose
    // keyed distance falls in [d - maxDistance, d + maxDistance].
    int low = d - maxDistance;
    int high = d + maxDistance;

    for (const auto& [childDist, childNode] : node->children) {
        if (childDist >= low && childDist <= high) {
            searchImpl(childNode.get(), query, maxDistance, results);
        }
    }
}

size_t BKTree::getNodeCount() const {
    return countNodes(root_.get());
}

size_t BKTree::countNodes(const BKNode* node) const {
    if (!node) return 0;
    size_t count = 1;
    for (const auto& [dist, child] : node->children) {
        count += countNodes(child.get());
    }
    return count;
}

size_t BKTree::getMaxDepth() const {
    return maxDepthImpl(root_.get());
}

size_t BKTree::maxDepthImpl(const BKNode* node) const {
    if (!node) return 0;
    size_t maxChildDepth = 0;
    for (const auto& [dist, child] : node->children) {
        maxChildDepth = std::max(maxChildDepth, maxDepthImpl(child.get()));
    }
    return 1 + maxChildDepth;
}

} // namespace lexicore
