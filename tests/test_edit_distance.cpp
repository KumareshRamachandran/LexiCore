#include "edit_distance.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>

using namespace lexicore;

// --- Standard unit tests ---

void testIdenticalStrings() {
    assert(editDistance("hello", "hello") == 0);
    assert(editDistance("", "") == 0);
    assert(editDistance("a", "a") == 0);
    std::cout << "  ✓ Identical strings\n";
}

void testEmptyStrings() {
    assert(editDistance("", "hello") == 5);
    assert(editDistance("hello", "") == 5);
    assert(editDistance("", "") == 0);
    assert(editDistance("", "a") == 1);
    assert(editDistance("a", "") == 1);
    std::cout << "  ✓ Empty strings\n";
}

void testSingleOperations() {
    // Single substitution.
    assert(editDistance("cat", "bat") == 1);
    assert(editDistance("cat", "car") == 1);

    // Single insertion.
    assert(editDistance("cat", "cats") == 1);
    assert(editDistance("at", "cat") == 1);

    // Single deletion.
    assert(editDistance("cats", "cat") == 1);
    assert(editDistance("cat", "at") == 1);

    std::cout << "  ✓ Single operations\n";
}

void testMultipleOperations() {
    assert(editDistance("kitten", "sitting") == 3);
    assert(editDistance("saturday", "sunday") == 3);
    assert(editDistance("intention", "execution") == 5);
    std::cout << "  ✓ Multiple operations\n";
}

// --- Bounded edit distance tests ---

void testBoundedExact() {
    // Within bound — should return true distance.
    assert(editDistanceBounded("cat", "bat", 1) == 1);
    assert(editDistanceBounded("cat", "cat", 0) == 0);
    assert(editDistanceBounded("kitten", "sitting", 3) == 3);
    assert(editDistanceBounded("kitten", "sitting", 5) == 3);
    std::cout << "  ✓ Bounded: within threshold\n";
}

void testBoundedBailout() {
    // Beyond bound — should return maxDist+1 (sentinel).
    assert(editDistanceBounded("cat", "dog", 1) == 2); // true dist is 3, sentinel is 2
    assert(editDistanceBounded("hello", "world", 2) == 3); // true dist is 4
    assert(editDistanceBounded("abc", "xyz", 1) == 2); // true dist is 3
    std::cout << "  ✓ Bounded: bail-out returns sentinel\n";
}

void testBoundedLengthDifference() {
    // Length difference alone exceeds maxDist.
    assert(editDistanceBounded("a", "abcde", 2) == 3);
    assert(editDistanceBounded("", "abc", 1) == 2);
    std::cout << "  ✓ Bounded: length difference early exit\n";
}

// --- Property-based tests (randomized) ---

/// Generate a random string of given length.
std::string randomString(std::mt19937& rng, size_t maxLen) {
    std::uniform_int_distribution<size_t> lenDist(0, maxLen);
    std::uniform_int_distribution<int> charDist('a', 'z');
    size_t len = lenDist(rng);
    std::string s;
    s.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        s += static_cast<char>(charDist(rng));
    }
    return s;
}

void testSymmetry() {
    // editDistance(a, b) == editDistance(b, a) for all strings a, b.
    std::mt19937 rng(12345);
    const int N = 1000;

    for (int i = 0; i < N; ++i) {
        std::string a = randomString(rng, 15);
        std::string b = randomString(rng, 15);
        int dAB = editDistance(a, b);
        int dBA = editDistance(b, a);
        if (dAB != dBA) {
            std::cerr << "  ✗ Symmetry violated: d(\"" << a << "\", \"" << b
                      << "\") = " << dAB << " but d(\"" << b << "\", \"" << a
                      << "\") = " << dBA << "\n";
            std::abort();
        }
    }
    std::cout << "  ✓ Symmetry property (" << N << " random pairs)\n";
}

void testTriangleInequality() {
    // editDistance(a, c) <= editDistance(a, b) + editDistance(b, c)
    std::mt19937 rng(67890);
    const int N = 1000;

    for (int i = 0; i < N; ++i) {
        std::string a = randomString(rng, 10);
        std::string b = randomString(rng, 10);
        std::string c = randomString(rng, 10);

        int dAC = editDistance(a, c);
        int dAB = editDistance(a, b);
        int dBC = editDistance(b, c);

        if (dAC > dAB + dBC) {
            std::cerr << "  ✗ Triangle inequality violated:\n"
                      << "    d(\"" << a << "\", \"" << c << "\") = " << dAC << "\n"
                      << "    d(\"" << a << "\", \"" << b << "\") + d(\"" << b
                      << "\", \"" << c << "\") = " << dAB << " + " << dBC
                      << " = " << (dAB + dBC) << "\n";
            std::abort();
        }
    }
    std::cout << "  ✓ Triangle inequality (" << N << " random triples)\n";
}

int main() {
    std::cout << "=== Edit Distance Tests ===\n";

    testIdenticalStrings();
    testEmptyStrings();
    testSingleOperations();
    testMultipleOperations();

    std::cout << "\n=== Bounded Edit Distance Tests ===\n";

    testBoundedExact();
    testBoundedBailout();
    testBoundedLengthDifference();

    std::cout << "\n=== Property-Based Tests ===\n";

    testSymmetry();
    testTriangleInequality();

    std::cout << "\nAll edit distance tests passed.\n";
    return 0;
}
