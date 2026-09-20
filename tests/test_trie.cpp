#include "trie.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace lexicore;

void testInsertAndSearch() {
    Trie trie;
    trie.insert("apple");
    trie.insert("application");
    trie.insert("apply");
    trie.insert("banana");

    assert(trie.search("apple") == true);
    assert(trie.search("application") == true);
    assert(trie.search("apply") == true);
    assert(trie.search("banana") == true);
    assert(trie.search("app") == false);    // prefix, not a word
    assert(trie.search("orange") == false);
    assert(trie.search("") == false);
    assert(trie.search("apples") == false);
    std::cout << "  ✓ Insert and search\n";
}

void testStartsWith() {
    Trie trie;
    trie.insert("apple");
    trie.insert("application");
    trie.insert("banana");

    assert(trie.startsWith("app") == true);
    assert(trie.startsWith("apple") == true);
    assert(trie.startsWith("ban") == true);
    assert(trie.startsWith("cat") == false);
    assert(trie.startsWith("") == true);  // empty prefix matches everything
    assert(trie.startsWith("applications") == false); // longer than any word
    std::cout << "  ✓ startsWith\n";
}

void testAutocomplete() {
    Trie trie;
    trie.insert("apple");
    trie.insert("application");
    trie.insert("apply");
    trie.insert("apt");
    trie.insert("banana");

    auto results = trie.autocomplete("app", 20);
    assert(results.size() == 3);
    // Results should be sorted lexicographically.
    assert(results[0] == "apple");
    assert(results[1] == "application");
    assert(results[2] == "apply");

    auto results2 = trie.autocomplete("ap", 20);
    assert(results2.size() == 4); // apple, application, apply, apt

    auto results3 = trie.autocomplete("ban", 20);
    assert(results3.size() == 1);
    assert(results3[0] == "banana");

    std::cout << "  ✓ Autocomplete\n";
}

void testAutocompleteLimit() {
    Trie trie;
    trie.insert("apple");
    trie.insert("application");
    trie.insert("apply");
    trie.insert("apt");

    auto results = trie.autocomplete("ap", 2);
    assert(results.size() == 2); // limited to 2
    std::cout << "  ✓ Autocomplete with limit\n";
}

void testEmptyTrie() {
    Trie trie;
    assert(trie.search("anything") == false);
    assert(trie.startsWith("") == true);
    assert(trie.autocomplete("test").empty());
    assert(trie.getNodeCount() == 1); // root node
    std::cout << "  ✓ Empty trie\n";
}

void testSingleCharPrefix() {
    Trie trie;
    trie.insert("apple");
    trie.insert("avocado");
    trie.insert("banana");

    auto results = trie.autocomplete("a", 20);
    assert(results.size() == 2);
    assert(results[0] == "apple");
    assert(results[1] == "avocado");
    std::cout << "  ✓ Single-char prefix\n";
}

void testNoMatchPrefix() {
    Trie trie;
    trie.insert("apple");
    trie.insert("banana");

    auto results = trie.autocomplete("xyz");
    assert(results.empty());
    std::cout << "  ✓ No-match prefix\n";
}

void testGetNodeCount() {
    Trie trie;
    // Root = 1 node.
    assert(trie.getNodeCount() == 1);

    trie.insert("ab");
    // Root -> a -> b = 3 nodes.
    assert(trie.getNodeCount() == 3);

    trie.insert("ac");
    // Root -> a -> {b, c} = 4 nodes.
    assert(trie.getNodeCount() == 4);

    std::cout << "  ✓ Node count\n";
}

int main() {
    std::cout << "=== Trie Tests ===\n";

    testInsertAndSearch();
    testStartsWith();
    testAutocomplete();
    testAutocompleteLimit();
    testEmptyTrie();
    testSingleCharPrefix();
    testNoMatchPrefix();
    testGetNodeCount();

    std::cout << "\nAll trie tests passed.\n";
    return 0;
}
