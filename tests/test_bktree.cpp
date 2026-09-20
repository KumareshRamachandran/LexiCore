#include "bktree.hpp"
#include "edit_distance.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace lexicore;

void testInsertAndSearch() {
    BKTree tree;
    tree.insert("book");
    tree.insert("cook");
    tree.insert("back");
    tree.insert("hook");
    tree.insert("books");

    auto results = tree.search("book", 1);
    // Should find: book (0), cook (1), hook (1), books (1).
    assert(results.size() == 4);

    std::unordered_set<std::string> words;
    for (const auto& [w, d] : results) {
        words.insert(w);
    }
    assert(words.count("book"));
    assert(words.count("cook"));
    assert(words.count("hook"));
    assert(words.count("books"));

    std::cout << "  ✓ Insert and fuzzy search\n";
}

void testExactMatch() {
    // maxDist=0 should behave like exact match.
    BKTree tree;
    tree.insert("apple");
    tree.insert("banana");
    tree.insert("apply");

    auto results = tree.search("apple", 0);
    assert(results.size() == 1);
    assert(results[0].first == "apple");
    assert(results[0].second == 0);

    auto results2 = tree.search("orange", 0);
    assert(results2.empty());

    std::cout << "  ✓ maxDist=0 (exact match)\n";
}

void testEmptyTree() {
    BKTree tree;
    assert(tree.empty());
    assert(tree.getNodeCount() == 0);
    assert(tree.getMaxDepth() == 0);

    auto results = tree.search("anything", 2);
    assert(results.empty());

    std::cout << "  ✓ Empty tree\n";
}

void testSingleWord() {
    BKTree tree;
    tree.insert("hello");

    assert(tree.getNodeCount() == 1);
    assert(tree.getMaxDepth() == 1);

    auto results = tree.search("hello", 0);
    assert(results.size() == 1);

    auto results2 = tree.search("hell", 1);
    assert(results2.size() == 1);
    assert(results2[0].first == "hello");

    std::cout << "  ✓ Single word tree\n";
}

void testLargeRadius() {
    // Large search radius — should find all words.
    BKTree tree;
    tree.insert("cat");
    tree.insert("dog");
    tree.insert("fish");
    tree.insert("bird");

    auto results = tree.search("cat", 10);
    assert(results.size() == 4); // All words within dist 10.

    std::cout << "  ✓ Large search radius\n";
}

void testDuplicateInsertion() {
    BKTree tree;
    tree.insert("hello");
    tree.insert("hello"); // duplicate — should be ignored

    assert(tree.getNodeCount() == 1);

    auto results = tree.search("hello", 0);
    assert(results.size() == 1);

    std::cout << "  ✓ Duplicate insertion ignored\n";
}

void testNodeCountAndDepth() {
    BKTree tree;
    tree.insert("book");
    tree.insert("cook");
    tree.insert("back");
    tree.insert("hook");

    assert(tree.getNodeCount() == 4);
    assert(tree.getMaxDepth() >= 1);
    assert(tree.getMaxDepth() <= 4); // worst case: chain

    std::cout << "  ✓ Node count and max depth\n";
}

void testNoMatchingResult() {
    BKTree tree;
    tree.insert("aaa");
    tree.insert("bbb");
    tree.insert("ccc");

    auto results = tree.search("zzz", 1);
    assert(results.empty());

    std::cout << "  ✓ No matching result\n";
}

void testVeryLongWords() {
    BKTree tree;
    std::string long1(50, 'a');
    std::string long2(50, 'b');
    tree.insert(long1);
    tree.insert(long2);

    auto results = tree.search(long1, 0);
    assert(results.size() == 1);
    assert(results[0].first == long1);

    std::cout << "  ✓ Very long words\n";
}

int main() {
    std::cout << "=== BK-Tree Tests ===\n";

    testInsertAndSearch();
    testExactMatch();
    testEmptyTree();
    testSingleWord();
    testLargeRadius();
    testDuplicateInsertion();
    testNodeCountAndDepth();
    testNoMatchingResult();
    testVeryLongWords();

    std::cout << "\nAll BK-tree tests passed.\n";
    return 0;
}
