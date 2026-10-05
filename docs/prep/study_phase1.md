# LexiCore Study Guide — Phase 1: Core Engine

> **Files covered (read in this order):**
> 1. `include/edit_distance.hpp` — declarations + contract documentation
> 2. `src/edit_distance.cpp` — two implementations of Levenshtein DP
> 3. `include/bktree.hpp` — data structure design (node + class)
> 4. `src/bktree.cpp` — insert, search, pruning, diagnostics
> 5. `tests/test_correctness.cpp` — oracle that proves correctness

---

## File 1: `edit_distance.hpp` (23 lines)

### Full file

```cpp
#pragma once

#include <string>

namespace lexicore {

/// Standard Levenshtein edit distance between two strings.
/// Operations: insertion, deletion, substitution (each cost 1).
/// Complexity: O(n·m) time and space where n, m are string lengths.
int editDistance(const std::string& a, const std::string& b);

/// Threshold-aware Levenshtein edit distance.
/// Only computes within a diagonal band of width 2*maxDist+1.
/// Returns the true distance if distance <= maxDist,
/// otherwise returns maxDist+1 as a sentinel (NOT the true distance).
///
/// IMPORTANT: The return value when distance > maxDist is a sentinel,
/// not the actual distance. Never use this sentinel for BK-tree pruning
/// interval calculation — use editDistance() for that.
int editDistanceBounded(const std::string& a, const std::string& b, int maxDist);

} // namespace lexicore
```

### Line-by-line observations

**`#pragma once` (line 1)**
Header guard — tells the compiler to include this file only once per translation unit, even if `#include`d multiple times. Equivalent to `#ifndef EDIT_DISTANCE_HPP / #define ... / #endif` but simpler. Standard in modern C++.

**`namespace lexicore` (line 5)**
All project symbols are wrapped in a namespace. Prevents name collisions if this library is used alongside other code. You'd access it as `lexicore::editDistance(...)` unless you write `using namespace lexicore`.

**`const std::string& a` (lines 10, 20)**
Pass by const reference — no copy, no modification. This is the default for non-trivial types in C++. If you wrote `std::string a`, you'd copy the entire string on every call — expensive.

**The contract comment on `editDistanceBounded` (lines 17–19)**
The most important lines in this header. It explicitly says:
> "The return value when distance > maxDist is a sentinel, not the actual distance. Never use this sentinel for BK-tree pruning."

This is **design-by-contract** — documenting not just what the function does but what you must NOT do with its output. In interviews, pointing to this comment shows you understand API design, not just implementation.

### Conceptual picture this file gives you

Two functions. Same problem. Different contracts:
```
editDistance(a, b)
  → always returns true distance
  → safe to use anywhere

editDistanceBounded(a, b, maxDist)
  → returns true distance if ≤ maxDist
  → returns maxDist+1 (SENTINEL) if > maxDist
  → ONLY safe for yes/no threshold checks
```

---

## File 2: `edit_distance.cpp` (96 lines)

### Function 1: `editDistance` (lines 9–36)

```cpp
int editDistance(const std::string& a, const std::string& b) {
    const size_t n = a.size();
    const size_t m = b.size();

    std::vector<int> prev(m + 1);   // "previous row" of the DP table
    std::vector<int> curr(m + 1);   // "current row" being filled

    // Base case: prev[j] = j (cost to turn "" into b[0..j-1])
    for (size_t j = 0; j <= m; ++j) {
        prev[j] = static_cast<int>(j);
    }

    for (size_t i = 1; i <= n; ++i) {
        curr[0] = static_cast<int>(i); // Base case: cost to turn a[0..i-1] into ""
        for (size_t j = 1; j <= m; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] = std::min({
                prev[j] + 1,         // delete a[i-1]
                curr[j - 1] + 1,     // insert b[j-1]
                prev[j - 1] + cost   // substitute (free if chars match)
            });
        }
        std::swap(prev, curr); // curr becomes prev for next row
    }

    return prev[m]; // answer is at position m of what was last "curr"
}
```

#### The 2-row space optimization — understand this deeply

The standard textbook DP uses a 2D table `dp[n+1][m+1]`. Every cell `dp[i][j]` only depends on:
- `dp[i-1][j]` → deletion (row above, same column)
- `dp[i][j-1]` → insertion (same row, left column)
- `dp[i-1][j-1]` → substitution (row above, left column)

This means when computing row `i`, you only ever look at row `i-1`. Rows 0 through `i-2` are never touched again. So instead of storing the entire (n+1)×(m+1) table, store only 2 rows.

```
Full table (O(nm) space):       2-row optimization (O(m) space):
┌────────────────────┐          prev: [0][1][2][3]  ← row i-1
│ 0  1  2  3  4 ... │          curr: [1][?][?][?]  ← row i being filled
│ 1  .  .  .  . ... │
│ 2  .  .  .  . ... │          After filling curr, swap(prev, curr)
│ 3  .  .  .  . ... │          → prev now holds row i
└────────────────────┘          → curr is garbage, reused next iteration
```

**Why `std::swap` is O(1):** Swapping two `vector`s doesn't copy data — it just swaps the internal pointers. Modern C++ vectors are heap-allocated, so swap is pointer exchange.

#### The `std::min({...})` initializer list form
```cpp
curr[j] = std::min({prev[j]+1, curr[j-1]+1, prev[j-1]+cost});
```
This is `std::min` with an initializer list — picks minimum of 3 values. Equivalent to `min(a, min(b, c))` but cleaner.

#### Why `a[i-1]` and not `a[i]`?
The DP index goes from 1 to n, but the string is 0-indexed. Row `i` corresponds to the `i`-th character, which is `a[i-1]`. This off-by-one is the most common mistake when implementing edit distance.

#### After the loops — why `prev[m]`?
After the last iteration's `std::swap`, what was `curr` (now fully computed row n) becomes `prev`. So the final answer is in `prev[m]`.

---

### Function 2: `editDistanceBounded` (lines 38–93)

```cpp
int editDistanceBounded(const std::string& a, const std::string& b, int maxDist) {
    const int n = static_cast<int>(a.size());
    const int m = static_cast<int>(b.size());

    // Optimization 1: length difference shortcut
    if (std::abs(n - m) > maxDist) {
        return maxDist + 1;
    }

    // Initialize BOTH rows to maxDist+1 (sentinel)
    std::vector<int> prev(m + 1, maxDist + 1);
    std::vector<int> curr(m + 1, maxDist + 1);

    // Base case: only fill within band [0, maxDist]
    for (int j = 0; j <= std::min(m, maxDist); ++j) {
        prev[j] = j;
    }

    for (int i = 1; i <= n; ++i) {
        curr.assign(m + 1, maxDist + 1); // reset entire row to sentinel

        // Diagonal band bounds for this row
        int jMin = std::max(1, i - maxDist);
        int jMax = std::min(m, i + maxDist);

        if (jMin == 1) {
            curr[0] = i; // base case col 0 (only if it's in range)
        }

        bool anyValid = false;

        for (int j = jMin; j <= jMax; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int del = prev[j] + 1;
            int ins = curr[j - 1] + 1;
            int sub = prev[j - 1] + cost;
            curr[j] = std::min({del, ins, sub});

            if (curr[j] <= maxDist) {
                anyValid = true;
            }
        }

        // Optimization 2: early termination
        if (!anyValid) {
            return maxDist + 1;
        }

        std::swap(prev, curr);
    }

    return (prev[m] <= maxDist) ? prev[m] : maxDist + 1;
}
```

#### Three optimization layers — learn each

**Layer 1: Length shortcut (lines 43–45)**
```cpp
if (std::abs(n - m) > maxDist) return maxDist + 1;
```
The minimum edit distance between strings of length n and m is `|n - m|` (you need at least that many insertions or deletions just to match lengths). If this floor already exceeds maxDist, the answer cannot possibly be ≤ maxDist. Return immediately.

**Layer 2: Diagonal band restriction (lines 60–61)**
```cpp
int jMin = std::max(1, i - maxDist);
int jMax = std::min(m, i + maxDist);
```
Only cells within `maxDist` steps of the main diagonal are computable — cells farther away can't produce a distance ≤ maxDist. Restricting j to `[i-maxDist, i+maxDist]` skips those cells.

```
Without restriction: fill entire row (m cells per row → O(nm) total)
With restriction:    fill (2*maxDist+1) cells per row → O(n * maxDist) total
```
For typo correction with maxDist=2: only 5 cells per row instead of potentially 10K.

**Layer 3: Early termination (lines 84–87)**
```cpp
if (!anyValid) return maxDist + 1;
```
If no cell in the current row is ≤ maxDist, the final cell `prev[m]` also can't be ≤ maxDist (distance only increases as you move away from the band). Return immediately instead of computing remaining rows.

#### Why initialize `prev` and `curr` to `maxDist+1`?
Cells outside the band are never filled. By initializing to the sentinel, when cells at band edges try to read from outside-band cells (`prev[j]` for `j = jMin-1`, or `curr[j-1]` for `j = jMin`), they read `maxDist+1` — which correctly forces those transitions to be expensive.

#### `int` vs `size_t` — notice the type change
`editDistance` uses `size_t` (unsigned) for loop indices. `editDistanceBounded` uses `int` (signed). Why? Because `jMin = i - maxDist` can be negative if `i < maxDist`. Unsigned arithmetic wraps around on underflow, causing bugs. Converting to `int` avoids this.

#### Final return (line 92)
```cpp
return (prev[m] <= maxDist) ? prev[m] : maxDist + 1;
```
Even after all optimizations, check if the result exceeds maxDist before returning. The band optimization means `prev[m]` might hold an overestimate if m > i + maxDist in the last row.

---

## File 3: `bktree.hpp` (65 lines)

```cpp
struct BKNode {
    std::string word;
    std::unordered_map<int, std::unique_ptr<BKNode>> children;
    explicit BKNode(std::string w) : word(std::move(w)) {}
};
```

### Every design choice in BKNode explained

**`std::string word`**
Each node stores the word it represents.

**`std::unordered_map<int, std::unique_ptr<BKNode>> children`**
This is the BK-tree's defining data structure. The key is the **edit distance** from this node's word to the child's word. `unique_ptr` gives ownership — each node uniquely owns all its children.

```
        "book"
        /     \
       k=1    k=2
       /         \
    "cook"      "back"
    (dist("book","cook")=1)   (dist("book","back")=2)
```

**`explicit BKNode(std::string w) : word(std::move(w)) {}`**
- `explicit` — prevents implicit conversion. You can't accidentally pass a `string` where a `BKNode` is expected.
- `: word(std::move(w))` — **move constructor** for the string. Transfers ownership of the string's internal buffer instead of copying it. If `w` = "apple" (5 chars), `move` is O(1) — just pointer swap. Copy would be O(k).
- Default constructor for `children` — `unordered_map` default-constructs to empty.

**Why `unordered_map` not `map` for children?**
Both store `int→BKNode*` mappings. `unordered_map` is O(1) average lookup, `map` is O(log n). For BK-tree child lookup by edge key, O(1) is preferable. The children won't be iterated in sorted key order (we iterate ALL children anyway during search), so sorted-order `map` offers nothing.

### BKTree class design

```cpp
class BKTree {
public:
    void insert(const std::string& word);
    std::vector<std::pair<std::string, int>> search(
        const std::string& query, int maxDistance) const;
    size_t getNodeCount() const;
    size_t getMaxDepth() const;
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
```

**Public/private split pattern:**
`insert` / `search` are public but trivial — they just guard the null root and delegate to the `Impl` functions. The real logic is in `insertImpl` / `searchImpl`, which take a raw `BKNode*` and recurse.

This pattern is common in tree code: public entry point handles the empty-tree edge case, private `Impl` handles the recursive logic. Clean separation.

**`search` returns `vector<pair<string,int>>`**
Not sorted, not ranked. Raw matches with their distances. Sorting/ranking is a separate concern (handled by `ranking.cpp`). Single responsibility principle.

**`const` on `search` and `getNodeCount`/`getMaxDepth`:**
These methods don't modify the tree, so they're marked `const`. This means you can call them on a `const BKTree&` reference.

---

## File 4: `bktree.cpp` (91 lines)

### Insert logic (lines 8–27)

```cpp
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
```

#### Trace insertion step by step

Insert: "book", "cook", "back", "hook"

```
Insert "book":
  root_ is null → create root node "book"
  Tree: ["book"]

Insert "cook":
  insertImpl(root="book", "cook")
  dist = editDistance("book", "cook") = 1
  children[1] doesn't exist → create children[1] = "cook"
  Tree: ["book"] → [k=1:"cook"]

Insert "back":
  insertImpl(root="book", "back")
  dist = editDistance("book", "back") = 2
  children[2] doesn't exist → create children[2] = "back"
  Tree: ["book"] → [k=1:"cook", k=2:"back"]

Insert "hook":
  insertImpl(root="book", "hook")
  dist = editDistance("book", "hook") = 1
  children[1] EXISTS ("cook") → recurse
    insertImpl("cook", "hook")
    dist = editDistance("cook", "hook") = 1
    "cook" has no children → create children[1] = "hook"
  Tree: ["book"] → [k=1:"cook"→[k=1:"hook"], k=2:"back"]
```

**Key insight:** When a child slot is occupied, you recurse into that child. The tree fills up by pushing words deeper when slots conflict.

**Duplicate detection (dist == 0):**
If the word already exists in the tree, editDistance = 0. Return immediately — don't create a zero-distance child (that would be wrong structurally and waste memory).

**`it->second.get()`:**
`it` is an iterator into the `unordered_map`, pointing to a `pair<int, unique_ptr<BKNode>>`. `it->second` is the `unique_ptr<BKNode>`. `.get()` borrows the raw pointer for the recursive call without transferring ownership.

---

### Search logic (lines 38–61) — THE most important function

```cpp
void BKTree::searchImpl(const BKNode* node, const std::string& query,
                         int maxDistance,
                         std::vector<std::pair<std::string, int>>& results) const {
    // Step 1: Compute TRUE distance (not bounded)
    int d = editDistance(query, node->word);

    // Step 2: If this node's word is within threshold, add to results
    if (d <= maxDistance) {
        results.emplace_back(node->word, d);
    }

    // Step 3: Triangle inequality pruning
    int low = d - maxDistance;
    int high = d + maxDistance;

    for (const auto& [childDist, childNode] : node->children) {
        if (childDist >= low && childDist <= high) {
            searchImpl(childNode.get(), query, maxDistance, results);
        }
    }
}
```

#### The pruning logic — trace it mentally

```
Query: "booc", maxDistance = 1

At root node "book":
  d = editDistance("booc", "book") = 1   (one substitution: o→o, o→k wait...)
  
  Actually: booc vs book
  b=b, o=o, o=o, c≠k → 1 substitution
  d = 1

  d <= maxDistance (1 <= 1): add "book" to results ✓
  
  low = 1 - 1 = 0
  high = 1 + 1 = 2
  
  Children to check: only those with edge labels in [0, 2]
  
  If "book" has children:
    child k=1 ("cook"): 1 is in [0,2] → recurse
    child k=2 ("back"): 2 is in [0,2] → recurse
    child k=3 ("apple"): 3 is NOT in [0,2] → SKIP (no match possible)
    child k=5 ("xyz"):   5 is NOT in [0,2] → SKIP
```

#### Why the pruning is correct (triangle inequality proof again, applied)

At node with word `w`, query `q`, search radius `r`:
```
d = dist(q, w)

For any word x in a subtree reachable via edge k:
  k = dist(w, x)  [this is what the edge label means]
  
Triangle inequality: dist(q, x) ≤ dist(q, w) + dist(w, x) = d + k
Triangle inequality: dist(q, x) ≥ |dist(q, w) - dist(w, x)| = |d - k|

For x to be a match: dist(q, x) ≤ r
Necessary condition: |d - k| ≤ r  →  d - r ≤ k ≤ d + r

If k is outside [d-r, d+r], then dist(q, x) > r for ALL x in that subtree.
We can SAFELY skip the entire subtree.
```

#### `results` passed by reference — why not return by value?

`searchImpl` is recursive. If it returned by value, each level would create a new vector and concatenate. Passing `results` by reference lets all recursive calls append to the same vector — efficient.

#### C++20 structured bindings (line 57)
```cpp
for (const auto& [childDist, childNode] : node->children) {
```
`[childDist, childNode]` destructures each `pair<int, unique_ptr<BKNode>>` in the map. Alternative (C++11/14 style):
```cpp
for (const auto& entry : node->children) {
    int childDist = entry.first;
    const auto& childNode = entry.second;
    // ...
}
```
Structured bindings are cleaner. They're a C++17 feature (not C++20), but LexiCore is compiled as C++20.

---

### Diagnostic functions (lines 64–88)

```cpp
// Node count via recursive postorder traversal
size_t BKTree::countNodes(const BKNode* node) const {
    if (!node) return 0;
    size_t count = 1;
    for (const auto& [dist, child] : node->children) {
        count += countNodes(child.get());
    }
    return count;
}

// Max depth via recursive DFS
size_t BKTree::maxDepthImpl(const BKNode* node) const {
    if (!node) return 0;
    size_t maxChildDepth = 0;
    for (const auto& [dist, child] : node->children) {
        maxChildDepth = std::max(maxChildDepth, maxDepthImpl(child.get()));
    }
    return 1 + maxChildDepth;
}
```

Both are standard recursive tree traversal — base case (null returns 0), recursive case aggregates children.

**Why max depth matters for BK-trees:**
- Well-shuffled insertion → max depth ≈ 19 (for 88K words) → many branches, good pruning
- Alphabetical insertion → max depth could be 1000+ → chain-like, terrible pruning

LexiCore measures this and reports it in benchmark output. At interview: "We verified the shuffle was effective by checking max depth — 19 for 88K words means excellent branching."

---

## File 5: `test_correctness.cpp` (214 lines)

This is the most important test file. It validates that BK-tree search returns **exactly the same results** as a brute-force linear scan — the only correctness guarantee worth trusting for a pruning algorithm.

### The oracle pattern (lines 16–30)

```cpp
std::vector<std::pair<std::string, int>> linearFuzzySearch(
    const std::vector<std::string>& words,
    const std::string& query,
    int maxDistance) {
    std::vector<std::pair<std::string, int>> results;
    for (const auto& w : words) {
        int d = editDistance(query, w);
        if (d <= maxDistance) {
            results.emplace_back(w, d);
        }
    }
    return results;
}
```

This is the **oracle** — trivially correct because it checks every word with no pruning. No branching logic, no pruning intervals, nothing to get wrong. It's the ground truth.

**The testing strategy:** If BK-tree results ≠ oracle results for any query, the pruning is wrong. This is called **differential testing** — test complex system against a simple known-correct system.

### Order-independent comparison (lines 32–42)

```cpp
bool resultSetsMatch(vector<pair<string,int>> a, vector<pair<string,int>> b) {
    auto cmp = [](const pair<string,int>& x, const pair<string,int>& y) {
        return x.first < y.first || (x.first == y.first && x.second < y.second);
    };
    sort(a.begin(), a.end(), cmp);
    sort(b.begin(), b.end(), cmp);
    return a == b;
}
```

BK-tree returns results in traversal order (DFS, non-deterministic relative to dictionary words). Linear scan returns results in dictionary order. The **sets** should be identical even if the **order** differs.

**Pattern:** Sort both, then compare element-by-element. This makes order-independent equality O(n log n) — simple and correct.

**Why take by value (not reference)?**
```cpp
bool resultSetsMatch(vector<pair<string,int>> a, vector<pair<string,int>> b)
//                   ^ by value, not by const&
```
Because we `sort` them in-place. If taken by const reference, we'd need to copy anyway. Taking by value lets the compiler potentially elide the copy (move semantics).

**The comparator lambda:**
```cpp
return x.first < y.first || (x.first == y.first && x.second < y.second);
```
Sorts lexicographically by word first, then by distance. A word can only appear once in results (dictionary has no duplicates), so this comparator is a strict weak ordering — required by `std::sort`.

### testSmallDictionary (lines 44–106)

17 words × 11 queries × 4 thresholds = 44 comparisons.

**Key design choice:**
```cpp
std::vector<std::string> queries = {
    "book", "cook",      // exact matches in dictionary
    "apple", "aple",     // one with typo
    "bak", "xyz",        // partial/no match
    "mple", "sampl",     // suffix queries
    "",                  // empty string edge case
    "examplee", "booook" // longer than dictionary words
};
```
These cover the interesting cases systematically. Especially: `""` (empty query), `"xyz"` (no match), `"booook"` (longer than any word).

**Error reporting pattern (lines 80–93):**
```cpp
std::unordered_set<std::string> linearWords, bkWords;
// ... populate sets ...
for (const auto& w : linearWords) {
    if (!bkWords.count(w)) cerr << "Missing from BK-tree: " << w;
}
for (const auto& w : bkWords) {
    if (!linearWords.count(w)) cerr << "Extra in BK-tree: " << w;
}
```
On mismatch, shows exactly which words are missing (BK-tree under-counted) or extra (BK-tree over-counted). This is **diagnostic-quality error output** — you can immediately see which pruning rule went wrong.

### testRandomizedOracle (lines 108–170)

500 random words, 200 queries (half mutated), 4 thresholds = 800 comparisons.

**Why random words?**
Handcrafted test cases only cover scenarios the author thought of. Random words can stumble upon edge cases the author didn't anticipate — especially in pruning logic.

**The mutation strategy:**
```cpp
if (i % 2 == 0 && !query.empty()) {
    size_t pos = ...(0, query.size() - 1)(rng);
    query[pos] = static_cast<char>(charDist(rng));
}
```
Half the queries are exact dictionary words (testing exact + near matches), half have a random character substitution (testing fuzzy search). Mix ensures both code paths are exercised.

**Seeded RNG:**
```cpp
std::mt19937 rng(99999);
```
Fixed seed → same random words and queries every run → deterministic test → reproducible failures.

### testEdgeCases (lines 172–202)

```cpp
// Empty query against dictionary {"a", "ab", "abc", "abcd"}
auto linear = linearFuzzySearch(words, "", 1);
auto bk = bktree.search("", 1);
assert(resultSetsMatch(linear, bk));
```

**Empty query:** editDistance("", "a") = 1, editDistance("", "ab") = 2, etc. With maxDist=1, only "a" is within radius 1. BK-tree must handle empty string as a valid query — not crash, not return empty results.

**Query longer than any word:** "abcdefgh" vs dictionary {"a", "ab", "abc", "abcd"}. All dictionary words are shorter. Edit distances are large. With maxDist=2, some might still match (e.g., "abcd" → "abcdefgh" needs 4 deletions, too far; but boundary cases can be tricky).

**maxDist=0:** Equivalent to exact match. BK-tree with maxDist=0 must return exactly the words equal to the query — no more, no less.

---

## Mental Model: How the 3 Pieces Fit Together

```
editDistance()
    ↓ used by
insertImpl()              (build phase: compute edge labels)
    ↓ also used by
searchImpl()              (query phase: compute d for pruning + match check)
    ↑ validated by
linearFuzzySearch()       (test oracle: guaranteed-correct brute force)
```

The correctness chain:
1. `editDistance` is verified correct by unit tests (symmetry + triangle inequality properties)
2. `insertImpl` uses `editDistance` to assign edge labels (structural correctness)
3. `searchImpl` uses `editDistance` for pruning interval (algorithmic correctness)
4. `test_correctness` verifies 3 by comparing against 1 directly

---

## Phase 1 — SDE Interview Questions

### Edit Distance Implementation

- **Q:** Walk me through your `editDistance` implementation. Why two rows instead of the full table?
  > Two rows suffice because cell `dp[i][j]` only depends on row `i-1`. When moving to row `i`, row `i-2` and earlier are never read again. `std::swap` exchanges vector internals in O(1) — no data movement.

- **Q:** Why did you change from `size_t` to `int` in `editDistanceBounded`?
  > `jMin = i - maxDist` can be negative when `i < maxDist`. `size_t` is unsigned — negative values wrap around to huge numbers. Using `int` prevents this undefined behavior.

- **Q:** What's the time complexity of `editDistanceBounded`?
  > O(n × min(m, 2×maxDist+1)) — for each of the n rows, only 2×maxDist+1 cells are computed. With small maxDist (e.g., 2), this is O(5n) rather than O(nm).

- **Q:** When can `editDistanceBounded` return early?
  > Two cases: (1) `|n - m| > maxDist` — length difference alone exceeds threshold; (2) a full row has no cell ≤ maxDist — no subsequent row can improve this.

### BK-Tree Design

- **Q:** Why is `BKNode`'s constructor marked `explicit`?
  > Prevents implicit conversions. Without `explicit`, the compiler might silently convert a `string` to a `BKNode` in an unexpected context, causing hard-to-debug behavior.

- **Q:** Why does `insertImpl` take a raw `BKNode*` rather than a `unique_ptr&`?
  > Because it doesn't need ownership — it just traverses and recurses. Taking `unique_ptr&` would be misleading (implies potential ownership transfer). Raw pointer is correct for a borrow.

- **Q:** What happens when you insert a duplicate word?
  > `editDistance(node->word, duplicate) = 0`. The function checks `if (dist == 0) return;` — silent skip, no duplicate node created.

- **Q:** Why does the BK-tree use `unordered_map` for children rather than `map`?
  > Lookup during search is O(1) average vs O(log k) for `map`, where k is the number of children. Since we iterate all children during search anyway and don't need sorted order, `unordered_map` is preferable.

### BK-Tree Correctness

- **Q:** Prove that BK-tree search is correct (never misses a valid match).
  > By the triangle inequality: if `dist(q, x) ≤ r` and `dist(q, node) = d`, then `|d - dist(node, x)| ≤ r`, so `dist(node, x) ∈ [d-r, d+r]`. Any valid match x reachable through a child with edge k must satisfy k ∈ [d-r, d+r]. Children outside this range cannot contain any valid match — pruning is safe.

- **Q:** What specifically goes wrong if you use `editDistanceBounded` for the pruning interval?
  > `editDistanceBounded("abc", "xyz", 2)` might return 3 (sentinel) when the true distance is 5. Using 3 to compute `[3-2, 3+2] = [1, 5]` instead of `[5-2, 5+2] = [3, 7]` — you include edges 1–2 (wasted work) and miss edges 6–7 (valid matches dropped silently).

- **Q:** How did you verify BK-tree correctness?
  > Differential testing against a linear scan oracle. For every query and threshold, the BK-tree result set must exactly match the linear scan result set. Tested: 17-word handcrafted dictionary (11 queries × 4 thresholds), 500-word random dictionary (200 queries × 4 thresholds), and 3 edge cases. Ran under AddressSanitizer + UBSan.

### Testing Strategy

- **Q:** What is an oracle in testing, and why is it useful here?
  > An oracle is a simpler, provably-correct implementation used to validate a complex one. Here, linear scan is the oracle — too slow for production but guaranteed correct because it has no pruning logic. Any divergence from the oracle indicates a pruning bug.

- **Q:** Why sort both result vectors before comparing them?
  > BK-tree returns results in DFS traversal order, linear scan in dictionary order — both orderings are arbitrary and different. We care about the **set** of results, not the order. Sorting both canonicalizes the order, making equality comparison meaningful.

- **Q:** Why use a seeded RNG in tests?
  > Reproducibility. A failing test must be reproducible to be debuggable. A random test with unseeded RNG could fail on Tuesday and pass on Wednesday — useless. Fixed seed 99999 means the same random words and queries every run.

---

## Reading Checklist — After Phase 1

- [ ] Trace `insertImpl` for ["book", "cook", "back", "hook"] on paper — draw the resulting tree
- [ ] Trace `searchImpl` for query "booc", maxDist=1 on that tree — which children are pruned?
- [ ] Explain why `jMin = max(1, i - maxDist)` in the bounded variant
- [ ] Explain what happens in `editDistanceBounded` when the early termination fires
- [ ] Write the `resultSetsMatch` comparator lambda from scratch without looking
- [ ] Explain the differential testing strategy to someone who hasn't seen this codebase
