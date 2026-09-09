# LexiCore — Implementation Plan
### High-Performance Fuzzy Search & Autocomplete Engine (C++20 / STL-Only)

> **Goal:** Build a multi-strategy retrieval engine from scratch — exact (hash), prefix (trie), fuzzy (BK-tree) search — with a benchmark suite that empirically proves each structure's trade-offs against a linear-scan baseline.

---

## Build Environment

| Tool | Version |
|---|---|
| Compiler | g++ 13.3.0 (C++20 support ✓) |
| Build system | CMake 3.28.3 |
| OS | Ubuntu 24.04, Linux 7.0.0, x86_64 |

---

## Proposed Changes

### Repository Layout

```text
LexiCore/
├── CMakeLists.txt                  # [NEW] Top-level CMake with Debug/Release configs
├── README.md                       # [MODIFY] Full project narrative
├── RESULTS.md                      # [NEW] Measured benchmark data only
├── docs/                           # Existing — untouched
├── include/
│   ├── dictionary.hpp              # [NEW] Dictionary loader + normalizer
│   ├── edit_distance.hpp           # [NEW] Levenshtein DP
│   ├── trie.hpp                    # [NEW] Trie for prefix search
│   ├── bktree.hpp                  # [NEW] BK-tree for fuzzy search
│   ├── ranking.hpp                 # [NEW] Frequency-weighted result ranker
│   └── benchmark.hpp               # [NEW] Benchmark harness
├── src/
│   ├── dictionary.cpp              # [NEW]
│   ├── edit_distance.cpp           # [NEW]
│   ├── trie.cpp                    # [NEW]
│   ├── bktree.cpp                  # [NEW]
│   ├── ranking.cpp                 # [NEW]
│   └── benchmark.cpp               # [NEW]
├── app/
│   └── main.cpp                    # [NEW] CLI entry point
├── data/
│   └── words.txt                   # [NEW] Public domain word list (10K–100K+)
└── tests/
    ├── test_edit_distance.cpp      # [NEW] Unit tests
    ├── test_trie.cpp               # [NEW] Unit tests
    ├── test_bktree.cpp             # [NEW] Unit tests
    └── test_correctness.cpp        # [NEW] BK-tree vs linear-scan oracle
```

---

### Phase 1 — Foundation (Dictionary, Normalization, Edit Distance, Linear Baseline)

#### [NEW] [`dictionary.hpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/include/dictionary.hpp) / [`dictionary.cpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/src/dictionary.cpp)

- **`Dictionary` class** — loads a plain-text word file into:
  - `std::vector<std::string> words` — full word list (for linear scan)
  - `std::unordered_set<std::string> wordSet` — O(1) exact lookup
- **Normalization policy:** lowercase everything, strip non-alpha characters, skip blank lines, deduplicate on insert
- **Subsetting:** `getSubset(size_t n, unsigned seed)` — returns a **seeded random sample** of N words from the full dictionary. Not "first N words" — most word lists are alphabetically sorted, so a prefix slice would benchmark only words starting with 'a'/'b', skewing BK-tree branching and similarity distribution.
- **Shuffle support:** `getShuffledWords(unsigned seed)` for BK-tree insertion order — `std::shuffle` with a fixed seed for reproducibility

#### [NEW] [`edit_distance.hpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/include/edit_distance.hpp) / [`edit_distance.cpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/src/edit_distance.cpp)

- **`editDistance(a, b)`** — standard Levenshtein DP, O(n·m)
- **`editDistanceBounded(a, b, maxDist)`** — threshold-aware variant: only computes within a diagonal band of width `2*maxDist+1`, returns early if no row entry can be ≤ `maxDist`. Returns `maxDist+1` as a sentinel on early bail-out. **Used inside BK-tree only for the threshold check** ("is this word a match?"), **never for computing the pruning interval** — see BK-tree section for why.

---

### Phase 2 — Trie (Prefix Search / Autocomplete)

#### [NEW] [`trie.hpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/include/trie.hpp) / [`trie.cpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/src/trie.cpp)

- **`TrieNode`** — `std::unordered_map<char, std::unique_ptr<TrieNode>> children`, `bool isEndOfWord`, `int frequency`
- **Memory ownership:** `unique_ptr` throughout — no manual `delete`, no dangling pointers. If asked: "every parent uniquely owns its children; destruction is automatic and top-down."
- **Methods:**
  - `insert(word, freq)` — walk/create nodes, O(k)
  - `search(word) → bool` — exact match via traversal, O(k)
  - `autocomplete(prefix, limit) → vector<pair<string,int>>` — traverse to prefix node O(k), then DFS collecting completed words, return top N by frequency (use `std::partial_sort` or a min-heap). Cap at `limit` to prevent 50K-word dumps.
  - `getNodeCount() → size_t` — for memory profiling

---

### Phase 3 — BK-Tree (Fuzzy / Typo-Tolerant Search)

#### [NEW] [`bktree.hpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/include/bktree.hpp) / [`bktree.cpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/src/bktree.cpp)

- **`BKNode`** — `std::string word`, `std::unordered_map<int, std::unique_ptr<BKNode>> children`
- **Memory ownership:** `unique_ptr` for all nodes (same rationale as Trie)
- **Methods:**
  - `insert(word)` — compute distance from current node, descend to `children[dist]` if exists, else create there
  - `search(query, maxDistance) → vector<pair<string,int>>` — **two-step distance check at each node:**
    1. Compute **true distance** `d = editDistance(query, node.word)` — this value is needed to calculate the correct pruning interval `[d - maxDistance, d + maxDistance]`
    2. If `d ≤ maxDistance`, add to results
    3. **Prune:** only recurse into children with keys in `[d - maxDistance, d + maxDistance]` (triangle inequality)
    
    > ⚠️ **Correctness constraint:** Never use `editDistanceBounded()` to obtain `d` for pruning. The bounded variant returns a sentinel (`maxDist+1`) on early bail-out, which is **not the true distance** — using it to compute the pruning interval would silently skip valid subtrees. Use `editDistanceBounded()` only where you need a yes/no threshold check and will not reuse the value for range arithmetic.
  - `getNodeCount() → size_t` — total nodes in tree
  - `getMaxDepth() → size_t` — deepest path from root; verify after building to detect degenerate chains, especially important to confirm the shuffle is working and the `unique_ptr` ownership tree isn't pathologically deep
- **Critical detail:** Dictionary must be **shuffled** before BK-tree insertion. Alphabetically-sorted input creates degenerate chain-shaped trees. Use `std::shuffle` with a **fixed seed** (e.g., 42) for reproducibility. Document this in README.

---

### Phase 4 — Ranking

#### [NEW] [`ranking.hpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/include/ranking.hpp) / [`ranking.cpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/src/ranking.cpp)

- **`rankResults(results) → vector<RankedResult>`**
- **Sort order (deterministic, two-key):**
  1. Primary: ascending edit distance (closer matches first)
  2. Secondary: lexicographic order (deterministic output across runs)
- **`RankedResult` struct:** `{ string word, int distance }`

---

### Phase 5 — Benchmark Harness

#### [NEW] [`benchmark.hpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/include/benchmark.hpp) / [`benchmark.cpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/src/benchmark.cpp)

- **Design principles from the docs:**
  - ⚠️ **Release build only** (`-O2`) — debug benchmarks are meaningless
  - Multiple trials (5), report **median** — single runs are noisy
  - **Warm-up run** before timing — skip first-run page fault / cache population costs
  - **Same query set** across all strategies per trial
  - **Separate construction time from query time**
- **What gets measured:**

| Metric | Per-Strategy |
|---|---|
| Index build time (ms) | ✓ |
| Total query time (ms) | ✓ |
| Avg query latency (µs) | ✓ |
| Queries/sec | ✓ |
| p50 / p95 / p99 latency | ✓ |

- **Fair paired comparisons** — each query type benchmarks only the structures that serve it:

| Query Type | Baseline | Optimized |
|---|---|---|
| Exact match | Linear scan | Hash lookup |
| Prefix / autocomplete | Linear prefix scan | Trie |
| Fuzzy / typo-tolerant | Linear edit-distance scan | BK-tree |

  This makes every speedup claim technically defensible — you're comparing like-for-like, not mixing query types across structures.

- **Sweep:** Dictionary sizes 1K / 10K / full (actual size verified — see Phase 9)
- **Query generation:** fixed seed random sampling from dictionary + synthetic misspellings (character swap, insertion, deletion) for fuzzy queries
- **Structure diagnostics** recorded alongside timing:
  - Node counts per structure (`getNodeCount()`)
  - BK-tree max depth (`getMaxDepth()`) — with and without shuffle, to show shuffle impact
  - Estimated memory per structure: `nodeCount × approx sizeof(node)` — not RSS (per-process metric is unreliable per-structure), but node-count-based estimates are defensible
- **Output:** formatted table to stdout + machine-parseable data for `RESULTS.md` (including diagnostics above)
- **Environment recording:** CPU, compiler, flags, OS — automatically captured

---

### Phase 6 — CLI

#### [NEW] [`main.cpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/app/main.cpp)

```text
$ ./lexicore

╔═══════════════════════════════════════╗
║  LexiCore — Fuzzy Search Engine       ║
║  Dictionary: 102,401 words loaded     ║
╚═══════════════════════════════════════╝

LexiCore> exact apple
✓ Found: apple

LexiCore> prefix app
  apple
  application
  apply
  ...and 12 more

LexiCore> fuzzy ressturant
  restaurant   (dist: 2)
  restaurants  (dist: 3)

LexiCore> fuzzy aple 1
  ample        (dist: 1)
  apple        (dist: 1)

LexiCore> benchmark
Running benchmark suite...
[results table]

LexiCore> help
Commands: exact <word>, prefix <prefix> [limit], fuzzy <word> [maxDist], benchmark, help, quit

LexiCore> quit
```

- **Commands:** `exact <word>`, `prefix <prefix> [limit]`, `fuzzy <word> [maxDist]`, `benchmark`, `help`, `quit`
- **Output cap:** prefix and fuzzy results default to top 20, overridable
- **Error handling:** missing file → clear message, invalid command → help hint, invalid radius → validation message

---

### Phase 7 — Tests

#### [NEW] [`test_edit_distance.cpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/tests/test_edit_distance.cpp)
- Standard cases: identical strings → 0, single insert/delete/substitute → 1, empty strings
- Bounded variant: verify early bail-out returns `maxDist+1`
- **Property-based tests (randomized):**
  - **Symmetry:** `editDistance(a, b) == editDistance(b, a)` for N random string pairs
  - **Triangle inequality:** `editDistance(a, c) ≤ editDistance(a, b) + editDistance(b, c)` for N random triples
  - These are the two metric properties that BK-tree correctness depends on — testing them directly strengthens the BK-tree argument

#### [NEW] [`test_trie.cpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/tests/test_trie.cpp)
- Insert + search, prefix autocomplete, empty trie, single-char prefix, no-match prefix

#### [NEW] [`test_bktree.cpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/tests/test_bktree.cpp)
- Insert + fuzzy search, `maxDist=0` ≡ exact match, empty tree, large radius

#### [NEW] [`test_correctness.cpp`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/tests/test_correctness.cpp)
- **Correctness oracle:** for a battery of queries × thresholds, BK-tree result set must **exactly match** linear-scan result set. This is the most important test — it proves the pruning logic doesn't silently drop valid matches.

**Edge cases explicitly tested (from docs §21/§32):**
- Empty query string
- Empty dictionary
- Single-character prefix
- Query longer than any dictionary word
- Prefix matching zero words
- `maxDistance = 0`
- Duplicate words in source file
- Very long words
- Large search radius (weak pruning stress)
- Highly similar dictionary words
- No matching result

---

### Phase 8 — Build System

#### [NEW] [`CMakeLists.txt`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/CMakeLists.txt)

```cmake
cmake_minimum_required(VERSION 3.20)
project(LexiCore LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library target (all core modules)
add_library(lexicore_lib
    src/dictionary.cpp
    src/edit_distance.cpp
    src/trie.cpp
    src/bktree.cpp
    src/ranking.cpp
    src/benchmark.cpp
)
target_include_directories(lexicore_lib PUBLIC include)

# Main executable
add_executable(lexicore app/main.cpp)
target_link_libraries(lexicore PRIVATE lexicore_lib)

# Test executables
enable_testing()
foreach(test_name test_edit_distance test_trie test_bktree test_correctness)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE lexicore_lib)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
```

- **Sanitizer support:** via `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"` during development
- **Release benchmarking:** `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`

---

### Phase 9 — Dictionary Data

#### [NEW] [`data/words.txt`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/data/words.txt)

- Source: public domain English word list (e.g., `/usr/share/dict/words` on Linux, or a curated list from GitHub)
- Target: 100K+ words for meaningful benchmarks
- Format: one word per line, plain text
- **Verify actual size:** run `wc -l data/words.txt` after sourcing the file. README and RESULTS.md must reflect the **actual** word count shipped, not an assumed "100K+"

---

### Phase 10 — Documentation

#### [MODIFY] [`README.md`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/README.md)
- Project overview, architecture diagram, build instructions, usage examples
- Design rationale: why each structure for each query type
- Normalization policy documented
- BK-tree shuffle rationale documented
- Ranking policy documented (distance → lexicographic, no synthetic frequency)
- Known trade-offs and limitations
- Complexity summary table

#### [NEW] [`RESULTS.md`](file:///home/kumaresh/Desktop/Dev/Personal/LexiCore/RESULTS.md)
- Real measured benchmark numbers only
- Environment details (CPU, compiler, flags)
- Methodology (trials, warm-up, query set)
- Tables for each dictionary size × strategy

---

## Execution Order

| Step | What | Depends On |
|---|---|---|
| 1 | CMakeLists.txt + project skeleton | — |
| 2 | Dictionary loader + normalizer | Step 1 |
| 3 | Edit distance (both variants) | Step 1 |
| 4 | Linear-scan baseline search | Steps 2, 3 |
| 5 | Hash-table exact search | Step 2 |
| 6 | Trie (insert + search + autocomplete) | Step 2 |
| 7 | BK-tree (insert + fuzzy search w/ pruning) | Steps 2, 3 |
| 8 | Ranking module | Step 2 |
| 9 | Tests: unit + correctness oracle | Steps 3–8 |
| 10 | CLI (interactive loop) | Steps 4–8 |
| 11 | Benchmark harness | Steps 4–7 |
| 12 | Run benchmarks in Release mode | Step 11 |
| 13 | README + RESULTS.md | Steps 9, 12 |

### Day Checkpoints

Rough targets, not hard deadlines — use as progress checkpoints:

| Day | Target | Steps |
|---|---|---|
| **Day 1** | Foundation + edit distance | 1–3 |
| **Day 2** | Trie + exact/prefix search | 5–6 |
| **Day 3** | BK-tree | 7 |
| **Day 4** | Ranking + tests | 8–9 |
| **Day 5** | Benchmark harness | 11–12 |
| **Day 6** | CLI + cleanup | 4, 10 |
| **Day 7** | Correctness/performance polish + docs | 13 |

If something takes longer, shift — the dependency order above is what actually matters.

---

## Key Design Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Memory ownership | `std::unique_ptr` for all tree nodes | RAII, no manual delete, no leaks, strong interview answer |
| Trie children map | `unordered_map<char, unique_ptr<TrieNode>>` | Flexible alphabet, memory-efficient for sparse nodes; `array<Node*,26>` discussed as alternative in README |
| Ranking | Distance → lexicographic (no synthetic frequency) | Scientifically clean; frequency code path available for real data |
| BK-tree insertion order | Shuffled with fixed seed 42 | Avoids degenerate chains from sorted input; reproducible |
| Edit distance in BK-tree | True distance for pruning interval; bounded variant only for threshold checks | Bounded returns a sentinel on bail-out — using it for pruning range would silently skip valid subtrees |
| Output capping | Default limit = 20 | Prevents terminal flood on broad queries like `prefix a` |
| Test framework | Simple `assert()` / `cassert` | No external dependency; sufficient for this scope |

---

## Verification Plan

### Automated Tests
```bash
# Debug build with sanitizers
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"
cmake --build build-debug
cd build-debug && ctest --output-on-failure

# Release build for benchmarks
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
./build-release/lexicore
# Then run `benchmark` command inside the CLI
```

### Manual Verification
- Interactive CLI testing: `exact`, `prefix`, `fuzzy` commands with various inputs
- Verify edge cases manually (empty query, huge prefix, etc.)
- Inspect `RESULTS.md` for reasonable numbers before committing
- Verify BK-tree vs linear-scan correctness oracle passes with 0 mismatches
