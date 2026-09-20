# LexiCore

**High-Performance Fuzzy Search & Autocomplete Engine** — C++20, STL-only

A multi-strategy retrieval engine that answers three query types, each backed by the data structure suited to it, and empirically proves their performance trade-offs through benchmarking.

```text
                LexiCore
                   |
       +-----------+-----------+
       |           |           |
   Exact Search Prefix Search Fuzzy Search
       |           |           |
   Hash Table      Trie      BK-Tree
       |           |           |
       +-----------+-----------+
                   |
                Ranking
                   |
              CLI Output
```

---

## Features

| Query Type | Data Structure | Complexity | Command |
|---|---|---|---|
| **Exact match** | `unordered_set` | O(1) avg | `exact <word>` |
| **Prefix / autocomplete** | Trie | O(k) traversal | `prefix <prefix> [limit]` |
| **Fuzzy / typo-tolerant** | BK-tree | Practical, metric-pruned | `fuzzy <word> [maxDist]` |
| **Baseline** | Linear scan | O(n·m) | Used in benchmarks |

- **Benchmark suite** comparing all strategies across dictionary sizes (1K / 10K / 88K words)
- **Correctness oracle** — BK-tree results validated against linear-scan baseline
- **Triangle-inequality pruning** in BK-tree search (the core algorithmic insight)

---

## Quick Start

```bash
# Build (Release)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run
./build/lexicore

# Run tests
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"
cmake --build build-debug
cd build-debug && ctest --output-on-failure
```

### Usage

```
LexiCore> exact apple
✓ Found: apple

LexiCore> prefix app
  apple
  application
  apply
  ...and 12 more

LexiCore> fuzzy aple 1
  able   (dist: 1)
  ale    (dist: 1)
  ample  (dist: 1)
  apple  (dist: 1)

LexiCore> benchmark
Running benchmark suite...
```

---

## Architecture

```text
                    CLI
                     |
              Query Parser
                     |
               Normalization
                     |
              +------+------+
              |             |
          Exact Query     Search Query
              |             |
        Hash-based        +-------------+
        lookup            |             |
                         Prefix        Fuzzy
                           |             |
                          Trie         BK-Tree
                           |             |
                           +------+------+
                                  |
                               Ranking
                          (distance → lex)
                                  |
                                Output
```

---

## Design Decisions

### Data Structure Selection

Each query type is served by the structure naturally suited to it:

- **`unordered_set`** — O(1) average exact membership test. Excellent for "is this word in the dictionary?" but cannot do prefix or fuzzy queries (you'd have to scan every key).
- **Trie** — O(k) traversal where k = prefix length, independent of dictionary size. The key advantage: retrieval cost doesn't grow with the dictionary.
- **BK-tree** — metric-space tree using edit distance. Triangle-inequality pruning (`d - maxDist ≤ child ≤ d + maxDist`) lets us skip entire subtrees without computing full edit distance against every word.
- **Linear scan** — the baseline everything else is measured against.

### Memory Ownership

All tree nodes (Trie and BK-tree) use `std::unique_ptr`. Every parent uniquely owns its children; destruction is automatic and top-down. No manual `delete`, no dangling pointers, no leaks.

An alternative for Trie nodes is `std::array<Node*, 26>` (faster lookup, more memory per node). We chose `unordered_map<char, unique_ptr<TrieNode>>` for flexible alphabet and memory efficiency on sparse nodes.

### BK-Tree Insertion Order

Dictionary is **shuffled** (`std::shuffle` with fixed seed 42) before BK-tree insertion. Alphabetically-sorted input creates degenerate chain-shaped trees because adjacent words tend to have similar edit distances from any root. Shuffling produces a well-branched tree (max depth 19 for 88K words vs. potentially 100+ without shuffling).

### Edit Distance in BK-Tree

The BK-tree search uses the **true** `editDistance()` (not the bounded variant) to compute the value `d` needed for the pruning interval `[d - maxDistance, d + maxDistance]`. The bounded variant `editDistanceBounded()` returns a sentinel (`maxDist+1`) on early bail-out, which is **not the true distance** — using it for pruning would silently skip valid subtrees.

### Normalization Policy

All words are lowercased, non-alphabetic characters are stripped, and blank lines are skipped during loading. Duplicates are removed (first occurrence wins). This is a deliberate, simple choice — documented here rather than left ambiguous.

### Ranking

Results are sorted by ascending edit distance, then lexicographic order for deterministic output across runs. No synthetic frequency data — ranking is scientifically clean.

### Benchmark Subsetting

Dictionary subsets for 1K/10K benchmark sweeps use **seeded random sampling**, not "first N words." Most word lists are alphabetically sorted, so a prefix slice would benchmark only words starting with 'a'/'b', skewing similarity distribution.

---

## Complexity Summary

| Structure | Search | Insert | Space | Notes |
|---|---|---|---|---|
| Linear scan | O(n·m) | O(1) | O(n) | n = dict size, m = word length |
| Hash table | O(1) avg | O(1) avg | O(n) | Exact match only |
| Trie | O(k) | O(k) | O(alphabet × nodes) | k = query length, independent of n |
| BK-tree | Practical | O(depth) | O(n) | Performance depends on distribution/radius |

---

## Known Trade-offs

| Issue | Cause | Discussion |
|---|---|---|
| Trie memory usage | `unordered_map` per node | Acceptable at this scale; compressed/radix trie is the production fix |
| BK-tree may underperform | Small dictionary, large radius, similar words | Not universal — diagnosing this is itself a learning outcome |
| Small dicts make fancy structures look worse | Overhead isn't worth it at small scale | Good result to report — structure choice depends on scale |
| Edit distance is O(n·m) | Classic DP | Fine for word-length strings; bounded variant helps in practice |

---

## Testing

All tests pass with AddressSanitizer + UndefinedBehaviorSanitizer enabled.

- **Unit tests** — edit distance, trie, BK-tree in isolation
- **Property-based tests** — edit distance symmetry and triangle inequality over 1000 random pairs/triples
- **Correctness oracle** — BK-tree results vs. linear-scan baseline (500 words × 200 queries × 4 thresholds)

---

## Tech Stack

- **Language:** C++20
- **Structures:** STL only — no external libraries
- **Build:** CMake 3.20+
- **Dictionary:** 88,344 unique words (from `/usr/share/dict/words`, 104,334 raw lines)

---

## Project Structure

```text
LexiCore/
├── CMakeLists.txt
├── README.md
├── RESULTS.md
├── include/
│   ├── dictionary.hpp
│   ├── edit_distance.hpp
│   ├── trie.hpp
│   ├── bktree.hpp
│   ├── ranking.hpp
│   └── benchmark.hpp
├── src/
│   ├── dictionary.cpp
│   ├── edit_distance.cpp
│   ├── trie.cpp
│   ├── bktree.cpp
│   ├── ranking.cpp
│   └── benchmark.cpp
├── app/
│   └── main.cpp
├── data/
│   └── words.txt
├── tests/
│   ├── test_edit_distance.cpp
│   ├── test_trie.cpp
│   ├── test_bktree.cpp
│   └── test_correctness.cpp
└── docs/
    └── ...
```
