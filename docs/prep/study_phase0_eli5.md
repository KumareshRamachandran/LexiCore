# LexiCore Study Guide — Phase 0 ELI5: Big Picture + Concept Map

> **Who this is for:** Competitive programmer (CF Specialist) who wants to understand *what* LexiCore is,
> *why* each part exists, and *how* everything fits together — before reading a single line of code.
> You need this "30,000 ft view" so that no line of code surprises you.

---

## 1. What Is LexiCore? (Explain Like I'm 5)

### 1.1 The Core Problem

Imagine you work at a library with **88,344 books**, each labeled with one word on the spine.
A visitor walks up and asks one of three things:

1. **"Is the word 'apple' here?"** → YES/NO
2. **"Show me every book starting with 'app'."** → Could be 50+ books
3. **"I wrote 'aple' — what did I mean?"** → Find similar words (typo-tolerant)

**The dumb solution:** walk the entire shelf from start to end for every question.
For 88,344 books, this is painfully slow.

**LexiCore's solution:** organize the library differently for each question type.

| Question Type | "Library Organization" | Technical Name |
|---|---|---|
| Exact lookup | Alphabetical index card (hash) | `unordered_set` |
| Prefix search | Tree of letter-tabs | Trie |
| Typo-tolerant | "Phonebook of similar distances" | BK-tree |

That's it. LexiCore is a **C++20 command-line program** that:
- Builds all three indexes from a 88,344-word dictionary
- Accepts user queries at a prompt
- Routes each query to the right structure
- Benchmarks all three against brute-force to **prove** the speedup

---

### 1.2 The Three Query Types — Concrete Examples

```
LexiCore> exact apple
✓ Found: apple                    ← hash lookup, O(1)

LexiCore> prefix app
  apple
  application
  apply
  ...and 12 more                  ← trie, O(k) where k=3 (len of "app")

LexiCore> fuzzy aple 1
  able   (dist: 1)
  ale    (dist: 1)
  apple  (dist: 1)                ← BK-tree, metric pruning
```

**Why three separate structures instead of one?**

No single structure answers all three efficiently:
- `unordered_set` → exact in O(1), but cannot answer prefix/fuzzy without scanning every key
- Trie → exact + prefix in O(k), but fuzzy requires visiting all nodes (same as brute force)
- BK-tree → fuzzy with pruning, but cannot do O(k) prefix — no path by prefix in a BK-tree

This is the fundamental design insight: **different problems need different structures**.

---

### 1.3 Edit Distance — The Concept That Everything Else Depends On

**What it is:** The minimum number of single-character operations (insert / delete / substitute)
to transform one word into another.

**Why it matters:** This number is what defines "closeness" in fuzzy search.

```
"aple" → "apple"
  Insert 'p' between 'a' and 'l'  → 1 operation
  editDistance("aple", "apple") = 1

"kitten" → "sitting"
  k→s (substitute), e→i (substitute), insert g  → 3 operations
  editDistance("kitten", "sitting") = 3
```

**The 3 operations in full:**

| Operation | Example | Cost |
|---|---|---|
| Substitution | "cat" → "bat" (c→b) | 1 |
| Insertion | "cat" → "cart" (insert r) | 1 |
| Deletion | "cart" → "cat" (delete r) | 1 |

**Minimum = Levenshtein edit distance.** This is the metric at the heart of the BK-tree.

---

### 1.4 The BK-Tree's Magic Trick — Triangle Inequality Pruning

Suppose you want all words within edit distance 2 of "aple".
Naive approach: compute editDistance("aple", word) for every word = 88,344 calls.

BK-tree approach: organize words so you can SKIP most of them.

**How?** Build a tree where each edge label = edit distance from parent's word to child's word.

```
       "book"
      /       \
   d=1         d=2
   /               \
"cook"           "back"
```

Now to search for "booc" with maxDist=1:
1. Visit "book": d = editDistance("booc","book") = 1 ✓ (match)
2. Only visit children where edge label k ∈ [d-maxDist, d+maxDist] = [0, 2]
3. Skip ALL other children — they cannot possibly contain a match (triangle inequality proves it)

**The triangle inequality:** If A is distance d from you, and B is distance k from A,
then B is at least |d-k| from you. If |d-k| > maxDist, B can't be a match → skip its subtree.

That's the entire cleverness. It sounds simple but it's a rigorous mathematical proof —
and it only works because edit distance satisfies the triangle inequality (it's a **metric**).

---

## 2. Project Architecture — Every File and Why It Exists

### 2.1 The Full Data Flow

```
words.txt (88,344 raw lines)
       |
       v
Dictionary::loadFromFile()   ← normalize + deduplicate
       |
       +----------+----------+
       |          |          |
unordered_set   Trie      BKTree
(wordSet_)     (words_)  (shuffled words_)
       |          |          |
       v          v          v
  exact query  prefix     fuzzy
  (main.cpp)   query      query
               (main.cpp) (main.cpp)
                     |
                   Ranking
                     |
                  CLI output
```

### 2.2 File-by-File Purpose Map

**`include/` — Contracts (Headers)**

| File | What it declares | Key design choice |
|---|---|---|
| `edit_distance.hpp` | `editDistance()`, `editDistanceBounded()` | Two functions with different contracts — one returns sentinel |
| `trie.hpp` | `Trie` class + `TrieNode` struct | `unordered_map<char, unique_ptr<TrieNode>>` children |
| `bktree.hpp` | `BKTree` class + `BKNode` struct | `unordered_map<int, unique_ptr<BKNode>>` children keyed by distance |
| `dictionary.hpp` | `Dictionary` class | Holds both `vector` (order) and `unordered_set` (lookup) |
| `ranking.hpp` | `RankedResult`, `rankResults()` | Separate concern — sort by distance then lexicographic |
| `benchmark.hpp` | `runBenchmark()`, `BenchmarkConfig` | Templated timing, percentile calculations |

**`src/` — Implementations**

| File | Core logic | Key technique |
|---|---|---|
| `edit_distance.cpp` | Levenshtein DP (2 variants) | 2-row rolling optimization, diagonal band restriction |
| `trie.cpp` | Insert, search, autocomplete | DFS `collectWords`, `findNode` traversal |
| `bktree.cpp` | Insert, search with pruning | Triangle inequality [d-r, d+r] interval check |
| `dictionary.cpp` | File I/O, normalize, shuffle | `mt19937` seeded shuffle, `isalpha`/`tolower` normalization |
| `ranking.cpp` | Sort results | Lambda comparator: distance first, then lexicographic |
| `benchmark.cpp` | Timing harness | `high_resolution_clock`, percentile calculation, template `timeQueries` |

**`app/main.cpp` — The Glue**

Parses user commands (`exact`, `prefix`, `fuzzy`, `benchmark`) and routes to the right structure.
Contains no algorithmic logic — pure dispatch and I/O. Single responsibility.

**`tests/` — Correctness Guarantees**

| File | Strategy | What it proves |
|---|---|---|
| `test_edit_distance.cpp` | Unit tests + property tests | Symmetry, triangle inequality, known pairs |
| `test_trie.cpp` | Unit tests | Insert/search/prefix/autocomplete correctness |
| `test_bktree.cpp` | Unit tests | Insert, search at various thresholds |
| `test_correctness.cpp` | Differential testing oracle | BK-tree ≡ linear scan for all queries/thresholds |

---

## 3. The Three Data Structures — Concept Deep Dive

### 3.1 Hash Table (`unordered_set`) — Exact Lookup

**Mental model:** A phonebook with alphabetical tabs. You jump directly to the right page.

**How it works:**
1. Compute `hash("apple")` → some integer like `394782`
2. `394782 % tableSize` → bucket index, say `47`
3. Look in bucket `47` — if "apple" is there, found. If not, not found.

**Why O(1)?** Because steps 1–3 are constant time regardless of dictionary size.

**The catch:** Two different strings can hash to the same bucket — that's a **collision**.
Handled by storing a linked list (chaining) or probing. Worst case (all collisions) = O(n).
Average case = O(1) with a good hash function.

**What it cannot do:** Prefix queries. To find all words starting with "app", you'd have to
rehash every word and check — O(n) scan. The trie does this in O(k).

**In LexiCore:**
```cpp
const std::unordered_set<std::string>& wordSet = dict.getWordSet();
if (wordSet.count(word)) { /* exact match */ }
```

**Benchmark result:** 271× faster than linear scan at 88K words. 0.45µs per query.

---

### 3.2 Trie (Prefix Tree) — Prefix + Autocomplete

**Mental model:** A tree of letter-tabs. Each level = one character. Shared prefixes = shared branches.

**Visual for ["apple", "apply", "apt", "banana"]:**
```
        root
       /    \
      a      b
      |      |
      p      a
     / \     |
    p   t    n
    |   |    |
    l  (*)   a
   / \        |
  e   y       n
  |   |       |
 (*) (*)      a
             (*)
```

(*) = isEndOfWord = true

**Why O(k) lookup?** Follow one edge per character. k = word/prefix length.
Dictionary size n is irrelevant — you never compare against other words.

**Autocomplete algorithm:**
1. `findNode(prefix)` → traverse to the prefix's terminal node in O(k)
2. `collectWords(node, prefix, results, limit)` → DFS from there, collect all isEndOfWord nodes
3. `sort(results)` → lexicographic order for deterministic output

**`unordered_map<char, unique_ptr<TrieNode>>` vs `array<Node*,26>`:**

| | `unordered_map` | `array<26>` |
|---|---|---|
| Lookup | O(1) avg (hash) | O(1) (direct index) |
| Memory per node | Only actual children | Always 26 pointers = 208 bytes |
| Total for 185K nodes | Much less (sparse) | 185,264 × 208 = ~38 MB |
| Alphabet | Any characters | Only lowercase a-z |

LexiCore chose `unordered_map` — flexible alphabet, lower memory for sparse nodes.

**Benchmark result:** 49× faster than linear scan. Performance is nearly **constant** from 10K to 88K
words (~4.4µs at both scales). This empirically proves O(k) — independent of n.

---

### 3.3 BK-Tree — Metric-Space Fuzzy Search

**Mental model:** A tree where words that are "close" (small edit distance) live near each other.
The tree structure encodes distances, enabling intelligent skipping.

**Insertion rule:** Each child edge is labeled with `editDistance(parent.word, child.word)`.
If a slot at that distance already exists, recurse into that child.

**Why shuffle before inserting?**

Alphabetical order → adjacent words have nearly identical edit distances from the root → chain tree.

```
Alphabetical: a → aa → aaa → aaaa (each at dist=1 from previous → deep chain, depth=n)
Shuffled:     "cat" root with children at distances 1,2,3,4,5 → wide and branchy, depth≈19
```

LexiCore uses `std::shuffle` with fixed seed 42 before BK-tree insertion.
Result: max depth 19 for 88K words (would be 1000+ without shuffle).

**Search with triangle inequality pruning:**

```cpp
void searchImpl(const BKNode* node, query, maxDist, results) {
    int d = editDistance(query, node->word);  // TRUE distance (not bounded!)
    if (d <= maxDist) results.emplace_back(node->word, d);

    int low  = d - maxDist;
    int high = d + maxDist;
    for (auto& [k, child] : node->children) {
        if (k >= low && k <= high) {        // Triangle inequality gate
            searchImpl(child.get(), ...);
        }
        // Children outside [low,high] → entire subtree skipped
    }
}
```

**The sentinel trap — most important correctness issue in the whole project:**

`editDistanceBounded(q, w, r)` returns `r+1` as a sentinel when true distance > r.
If you mistakenly use this sentinel as `d` in the pruning interval [d-r, d+r],
the interval is wrong → you **silently miss valid matches**.

LexiCore explicitly uses the true `editDistance()` in BK-tree search.

**Benchmark result:** 2.4× faster than linear scan at 88K words.
Not O(log n) — performance depends on tree balance and search radius.

---

## 4. C++ Concepts — From Zero to Reading LexiCore

### 4.1 `unique_ptr` — Why Every Node Uses It

**Problem with raw pointers:**
```cpp
TrieNode* node = new TrieNode();
// ... 200 lines of code later ...
delete node;  // easy to forget → memory leak
              // double-delete → crash
              // exception before delete → leak
```

**`unique_ptr` solution:**
```cpp
std::unique_ptr<TrieNode> node = std::make_unique<TrieNode>();
// Destructor runs automatically when node goes out of scope
// No manual delete. No leak. Exception-safe.
```

**The "unique" part:** Only ONE `unique_ptr` can own an object at a time.

```cpp
auto a = std::make_unique<int>(42);
auto b = a;             // COMPILE ERROR — copy constructor deleted
auto b = std::move(a);  // OK — ownership transferred, a becomes nullptr
```

**In LexiCore's Trie:**
```cpp
struct TrieNode {
    std::unordered_map<char, std::unique_ptr<TrieNode>> children;
    bool isEndOfWord = false;
};
```

Each parent uniquely owns all its children. When root is destroyed, the destructor chain
fires top-down automatically — every node freed, no manual `delete` anywhere.

**`.get()` for traversal without ownership:**
```cpp
TrieNode* current = root_.get(); // borrow raw pointer, unique_ptr still owns it
for (char c : word) {
    current = current->children[c].get(); // traverse, no ownership transfer
}
```

Rule: use `.get()` only for short-lived traversal borrows.
Never store `.get()` results beyond the `unique_ptr`'s lifetime.

---

### 4.2 RAII — The Philosophy Behind `unique_ptr`

**Resource Acquisition Is Initialization.**
Every resource (heap memory, file handle, lock) is tied to an object's lifetime.
Acquire in constructor. Release in destructor.

```cpp
// RAII: file automatically closed when scope exits
{
    std::ifstream file("words.txt"); // constructor opens file
    // ... read lines ...
}  // destructor closes file — even if exception is thrown
```

This is why LexiCore has zero explicit `delete` calls, zero `fclose()` calls.
All resources are managed through RAII objects (`unique_ptr`, `ifstream`).

---

### 4.3 Move Semantics — Why `std::move` Appears Everywhere

Strings contain heap-allocated character buffers. Copying a string = allocating new buffer + copying bytes = O(k).
**Moving** a string just swaps the internal pointer. O(1).

```cpp
// BKNode constructor — moves string instead of copying
explicit BKNode(std::string w) : word(std::move(w)) {}
//                                     ^ O(1) pointer swap, not O(k) copy
```

After `std::move(w)`, `w` is in a valid but unspecified state (usually empty string).
The moved-from object must not be used except to assign to it or destroy it.

**Why this matters for performance:**
Insertion of 88,344 words into BK-tree — without move semantics, each word is copied on
every recursion level. With move, each string is constructed once and handed off in O(1).

---

### 4.4 `const` Correctness — Reading the Function Signatures

```cpp
// From trie.hpp:
bool search(const std::string& word) const;
//          ^--- no copy         ^--- method doesn't modify Trie object
```

Three distinct `const` usages:
1. `const std::string& word` — parameter: read-only, no copy
2. `const TrieNode* findNode(...)` — return type: caller cannot modify the returned node
3. `bool search(...) const` — method: cannot mutate `this` — safe to call on `const Trie&`

**Why it matters:**
Passing `const Trie& trie` to a function only lets you call `const`-marked methods.
Non-const methods would be a compile error. This prevents accidental mutation.

---

### 4.5 Structured Bindings — C++17 Syntax You'll See Everywhere

```cpp
// Old style (C++11):
for (const auto& entry : node->children) {
    char ch = entry.first;
    const auto& child = entry.second;
}

// Structured bindings (C++17):
for (const auto& [ch, child] : node->children) {
    // ch and child directly available
}
```

LexiCore uses this throughout. The `[ch, child]` destructures each
`pair<char, unique_ptr<TrieNode>>` automatically.

---

### 4.6 `std::mt19937` — Seeded Randomness

**Mersenne Twister** — a fast, high-quality pseudo-random number generator.
(Same one competitive programmers use in solutions!)

```cpp
std::mt19937 rng(42);                          // seed 42 → reproducible
std::shuffle(words.begin(), words.end(), rng);  // deterministic shuffle
```

**Why fixed seed?**
- Same seed → same shuffle → same BK-tree shape → same benchmark numbers every run
- Reproducible results are verifiable. Non-reproducible results are meaningless.
- Bug reproducibility: if a test fails, re-running with same seed reproduces it exactly.

**Why seed 42 specifically?** Arbitrary. Any fixed value works — just document it.

---

### 4.7 Namespaces — Why `lexicore::` Prefixes Everything

```cpp
namespace lexicore {
    class Trie { ... };
    int editDistance(const std::string& a, const std::string& b);
}
```

**Purpose:** Prevent name collisions. If LexiCore is used in a larger project that also defines
a `Trie` class, `lexicore::Trie` and `other_lib::Trie` don't conflict.

Library headers never use `using namespace` — that would force the namespace on all includers.
`main.cpp` (top-level application) uses `using namespace lexicore` for brevity.

---

## 5. Complexity Analysis — What You Must State in Interviews

### 5.1 The Master Complexity Table

| Structure | Build Time | Query Time | Space | Caveat |
|---|---|---|---|---|
| Linear scan | O(n) | O(n·k) | O(n) | Baseline, always correct, always slowest |
| `unordered_set` | O(n·k) | O(k) avg | O(n) | Exact match only; O(n) worst-case |
| Trie | O(n·k) | O(k) | O(Σ × nodes) | Σ = alphabet size; k = key length |
| BK-tree | O(n·k²) | Practical | O(n) | Distribution-dependent, not O(log n) |
| Edit distance | — | O(n·m) time | O(m) space | 2-row optimization; O(n·maxDist) bounded |

### 5.2 What O(k) vs O(n) Means Practically

For this project: k ≈ 5–15 chars (average word length), n = 88,344.

```
O(n·k) for linear scan → ~88,344 × 10 = 883,440 operations per query
O(k)   for trie        → ~10 operations per query
Theoretical speedup    → ~88,000×
Actual measured speedup → 49× (overhead, cache misses, DFS + sort costs)
```

The gap between theoretical and measured speedup is explained by:
- Cache effects (trie nodes scattered in heap, not sequential like an array)
- Hash function overhead in `unordered_map`
- Output collection (DFS and sorting results takes time)

---

### 5.3 Amortized O(1) for `vector::push_back`

`results.push_back(word)` in trie's `collectWords` is **amortized O(1)**.

Individual pushbacks are O(1) when the vector has capacity.
Occasional resize doubles capacity → copies all elements → O(n) for that one call.
Over n total pushbacks: total work is O(n) → O(1) per operation on average.

This is why `results.reserve(count)` is used in `rankResults` — pre-allocates to avoid resizes.

---

## 6. Benchmark Methodology — Reading RESULTS.md Intelligently

### 6.1 Why Benchmark Numbers Are Meaningless Without Context

Every number in RESULTS.md is tied to:
```
CPU: x86_64 | OS: Ubuntu 24.04 | Compiler: g++ 13.3.0
Build: Release (-O2) | Dictionary: 88,344 words
Queries: 1,000/trial | Trials: 5, median | Warm-up: 1 pass
```

If any of these change, the numbers change. A "BK-tree is 2.4× faster" claim is only meaningful
relative to the specific baseline on the specific hardware with the specific query set.

### 6.2 The Three Comparisons and Why Each Is Paired Correctly

| Pair | Why this pairing |
|---|---|
| Linear scan vs Hash lookup (exact) | Both answer exact match — only data structure differs |
| Linear prefix scan vs Trie (prefix) | Both return words starting with prefix — only structure differs |
| Linear edit-dist scan vs BK-tree (fuzzy) | Both return words within edit distance r — only structure differs |

You don't compare trie's fuzzy performance vs BK-tree (trie doesn't do fuzzy),
or hash's prefix performance vs trie (hash can't do prefix).
The pairings are scientifically clean — **only one variable changes**.

### 6.3 Median vs Mean vs Percentiles

```
5 benchmark trials (ms): [10.1, 10.3, 47.5, 10.2, 10.4]
                                         ^ OS interrupt

Mean   = (10.1+10.3+47.5+10.2+10.4)/5 = 17.7 ms  ← inflated by outlier
Median = 10.3 ms                                   ← stable, representative
```

**p50 = median:** 50% of queries complete within this time.
**p95:** 95% of queries complete within this time.
**p99:** 99% of queries complete within this time.

p99 matters for user experience: "even your slowest 1% of queries are within X ms."
Average can look great while p99 is terrible — hidden tail latency.

---

## 7. SDE Interview Q&A — Phase 0 Concept Questions

These are questions about the big picture — architecture, design decisions, trade-offs.
Answer these before reading code. The code will confirm your mental model.

### Architecture & Design

**Q: Why does LexiCore maintain three separate data structures instead of one?**

**Q: What's the difference between exact, prefix, and fuzzy search? Can one structure handle all three?**

**Q: Why is the dictionary shuffled before BK-tree insertion but not before Trie insertion?**

**Q: What would break if you inserted words into the BK-tree in alphabetical order?**

**Q: Why does the BKNode store children as `unordered_map<int, unique_ptr<BKNode>>` specifically?**

**Q: Why does Dictionary store both a `vector<string>` and an `unordered_set<string>`?**

### Edit Distance

**Q: What are the three edit operations? What is the Levenshtein distance between "kitten" and "sitting"?**

**Q: Why is edit distance a "metric"? Name all four metric properties.**

**Q: Why does BK-tree need edit distance to be a metric (not just any distance function)?**

**Q: Two people propose: one uses Hamming distance (substitution only), one uses Levenshtein. Which is better for a spell-checker, and why?**

### Data Structures

**Q: Explain the trie autocomplete algorithm — what is its time complexity and what determines it?**

**Q: What's the memory trade-off between `unordered_map<char>` children and `array<Node*,26>` in a trie? When would you choose each?**

**Q: Explain BK-tree pruning from the triangle inequality — from first principles.**

**Q: Is BK-tree search O(log n)? What determines practical performance?**

### C++ Ownership

**Q: What is RAII? How does LexiCore apply it to tree memory management?**

**Q: Why does LexiCore use `unique_ptr` instead of raw pointers for tree nodes?**

**Q: What happens to all 185,000+ trie nodes when the `Trie` object is destroyed?**

**Q: When should you use `unique_ptr` vs `shared_ptr` vs `weak_ptr`?**

### Benchmarking

**Q: Why would running the benchmark in Debug mode give wrong results?**

**Q: Why does the benchmark use seeded random queries? What would go wrong with different seeds?**

**Q: What is p99 latency and why is it sometimes more important than average latency?**

**Q: The trie shows nearly identical latency at 10K and 88K words. What does this empirically prove?**

---

## 8. Reading Order Checklist — Before Phase 1

Before reading any code, verify you can do all of the following on paper:

### Must-Do Exercises

- [ ] **Draw a trie** for: `["apple", "apply", "apt", "ape", "banana"]`
  Mark `isEndOfWord = true` at right nodes. Count total nodes.

- [ ] **Trace BK-tree insertion** for (in order): `"book"`, `"cook"`, `"back"`, `"hook"`
  Draw the tree, label every edge with the edit distance. Show work at each step.

- [ ] **Trace BK-tree search** for query `"booc"` with `maxDist=1` on the tree above
  At each node: compute d, check if d≤1, compute [low,high], which children are skipped?

- [ ] **Fill the DP table** for `editDistance("cat", "bat")`
  Draw the full (4×4) table. What is dp[3][3]?

- [ ] **State the triangle inequality** from memory and explain why BK-tree needs it

- [ ] **Explain what `unique_ptr` prevents** — name two specific bugs it eliminates

### Concept Checks

- [ ] Can you explain why the Trie shows O(k) latency independent of n?
- [ ] Can you explain why BK-tree performance is NOT O(log n)?
- [ ] Can you state the sentinel trap: what goes wrong if you use `editDistanceBounded` for pruning?
- [ ] Can you explain why the dictionary is shuffled with a fixed seed before BK-tree insertion?
- [ ] Can you explain RAII in one sentence and give one example from LexiCore?

Once you can do all of the above without looking anything up, proceed to Phase 1.

---

> **Continue to:** `study_phase1.md` — Line-by-line reading of `edit_distance.cpp` and `bktree.cpp`
