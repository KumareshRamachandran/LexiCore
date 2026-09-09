# LexiCore
## High-Performance Fuzzy Search Engine (Updated)

**Primary purpose:** Upgrade your existing C++ spell-checker into a genuine search/autocomplete/fuzzy-retrieval engine.

**Target:** A project that demonstrates data structures, algorithms, search indexing, complexity analysis, performance engineering, and C++ design — no web development or third-party frameworks required.

**Time budget: 2-3 days.** Day 1 = trie. Day 2 = BK-tree. Day 3 = ranking + benchmark suite + README writeup. Don't let any single stage bleed past its day without a deliberate decision to cut something else.

---

# 1. Why build LexiCore?

Your original spell-checker already gives you a foundation in C++, STL, string processing, file I/O, and spelling correction.

The upgrade turns it into something substantially more interesting:

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
                Results
```

The important idea is not simply adding features. The project demonstrates that **different query types require different data structures.**

---

# 2. Final functionality

LexiCore should support three primary operations, plus benchmarking.

**Exact search**
```text
> exact apple
apple
```

**Prefix/autocomplete**
```text
> prefix app
apple
application
apply
appreciate
...
```

**Fuzzy search**
```text
> fuzzy aple
apple
ample
apply
...
```

---

# 3. Final architecture

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
                                  |
                                Output
```

---

# 4. Technology stack

```text
C++20
STL
CMake
```

No React, Node.js, database, cloud service, or external framework. STL-only, deliberately.

Recommended components: `unordered_map`, `unordered_set`, `vector`, `list`, `queue`, `string`, `fstream`, `chrono`, `algorithm`, `memory`.

---

# 5. Repository structure

```text
lexicore/
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
│   └── dictionary.txt
└── tests/
    ├── test_edit_distance.cpp
    ├── test_trie.cpp
    └── test_bktree.cpp
```

`RESULTS.md` is new — this is where your real, measured benchmark numbers live, separate from the README's narrative description. Simple assertion-based tests are fine; you don't need a testing framework.

---

# 6. Feature 1 — Dictionary loading

```cpp
std::vector<std::string> words;
std::unordered_set<std::string> exactWords;
std::unordered_map<std::string, int> frequency; // if supported
```

**Deduplicate during load.** If your source word list has duplicate entries, decide now whether to skip them or merge frequency counts — don't let this surface as a silent bug later.

---

# 7. Feature 2 — Normalization

```text
"Apple" → "apple"
```

Lowercase, strip unnecessary punctuation, normalize whitespace. Keep the policy simple and document it explicitly in the README — an interviewer may ask "how do you handle case/punctuation," and "I made a deliberate, simple choice and documented it" is a much better answer than uncertainty.

---

# 8. Feature 3 — Baseline linear fuzzy search

Build this **first**, before anything clever:

```text
Query → for every dictionary word → calculate edit distance → keep words within threshold
```

This becomes your performance baseline and the thing every other structure is measured against.

---

# 9. Feature 4 — Edit distance

Classic Levenshtein DP:

```text
dp[i][j] = min(
    dp[i-1][j] + 1,        // deletion
    dp[i][j-1] + 1,        // insertion
    dp[i-1][j-1] + cost    // substitution, cost = 0 if chars equal else 1
)
```

Directly reusable from your existing autocorrect work.

**Complexity:** O(n·m) time and space for two strings of length n and m — mention this explicitly if asked, and note that for natural-language word lengths this is fine, but it wouldn't scale to comparing long documents.

---

# 10. Feature 5 — Hash-table exact search

```cpp
std::unordered_set<std::string>
```

Average O(1) lookup. This gives you a clean comparison point: hash maps are excellent for exact lookup, a poor fit for prefix/fuzzy search — a strong, simple interview answer.

---

# 11. Feature 6 — Trie

```text
             root
              |
              a
              |
              p
              |
              p
            /   \
           l     r
           |      |
           e      y
```

Each node stores `children` and `isEndOfWord`. Implement `insert(word)`, `search(word)`, `startsWith(prefix)`.

---

# 12. Trie autocomplete

```text
prefix = "app" → traverse to node "app" → DFS → all completed words below
```

Complexity: traversal to prefix node is O(k), result enumeration is O(output), where k is prefix length — independent of total dictionary size. This is the key interview point for tries.

---

# 13. Feature 7 — BK-tree

Your main novelty. Stores values using a distance metric; each node contains a word and `children[distance]`.

```text
                book
              /      \
           d=1        d=2
           /            \
         cook           back
```

The child edge represents edit distance from the parent.

---

# 14. BK-tree insertion

```text
new word → distance from current node → child[distance] → exists? yes: continue / no: insert here
```

Implement this yourself rather than using a library.

**Important — insertion order affects tree shape.** If you insert a dictionary that's alphabetically sorted (or otherwise ordered in a way that correlates with edit distance from the root), you get a skewed, chain-like tree instead of a well-branched one — and your benchmark numbers will look artificially bad for reasons that have nothing to do with your implementation being wrong. **Fix: shuffle the dictionary (e.g., `std::shuffle` with a fixed seed for reproducibility) before inserting into the BK-tree.** Note this explicitly in your README — it's a real, known BK-tree characteristic, and mentioning it unprompted signals genuine understanding.

---

# 15. BK-tree fuzzy search

```text
query = "booc", radius = 1
d = editDistance(query, node.word)
```

You do not explore every child. The triangle inequality bounds the relevant range of child distances:

```text
d - radius ≤ child distance ≤ d + radius
```

Branches outside that interval can be skipped. **That pruning is the core idea you need to understand and explain.**

Do not claim the BK-tree is guaranteed O(log n) — its practical performance depends on dictionary distribution, the edit-distance metric, and the maximum search radius.

---

# 16. Feature 8 — Result ranking

```text
apple  distance=1
ample  distance=1
apply  distance=1
```

Primary sort key: smaller edit distance. Secondary (tie-break): higher word frequency. Keep the ranking function simple enough to explain in one sentence.

---

# 17. Feature 9 — CLI

```text
$ lexicore
LexiCore> exact apple
Found: apple

LexiCore> prefix app
apple
application
apply

LexiCore> fuzzy aple
apple
ample

LexiCore> benchmark
Running...
```

No graphical interface needed.

**Cap output size.** If `prefix a` matches 50,000 words, don't print all of them — limit to a reasonable N (e.g., top 20 by frequency) with a note like "...and 49,980 more." This is both a UX decision and a demonstration that you thought about the failure mode of a too-broad query.

---

# 18. Feature 10 — Benchmark engine

Compare Linear Search, Hash Lookup, Trie, and BK-tree across dictionary sizes: 1,000 / 10,000 / 100,000 words.

Record: total execution time, average query time, queries/sec. Optionally: memory usage.

## Benchmark methodology — do this properly, not casually
- **Build in Release mode with optimizations on** (`-O2` or CMake's `Release` build type). A Debug-build comparison is close to meaningless and an easy thing to get challenged on if you quote numbers from one.
- **Run multiple trials per configuration and report the median**, not a single run — a single timed run is noisy (OS scheduling, cache state, background processes) and can mislead you about which structure actually won.
- **Include a warm-up run before timing** — the very first execution pays one-time costs (page faults, cache population) that don't reflect steady-state performance.
- **Use the same query set across all four strategies** for a given trial — comparing different queries against different structures isn't a fair benchmark.
- **Record your environment** (CPU, compiler, optimization flags, OS) in `RESULTS.md` alongside the numbers — this is what makes a benchmark claim credible rather than assertion.

---

# 19. Important benchmark principle

Do not write "BK-tree is 50x faster" unless you actually measured 50x. Document methodology in your README:

```text
Dictionary: 100K words
Queries: 10K (same set across all 4 strategies)
Trials: 5, median reported
Build: Release / -O2
Hardware: ...
```

Then publish your actual measurements in `RESULTS.md`. Fill in performance claims on your resume only after real benchmarking.

---

# 20. Problems you will probably encounter

## Problem 1 — Trie memory usage
A naive trie can create a huge number of nodes. `unordered_map<char, Node*>` vs. `array<Node*, 26>` is a genuine speed-vs-memory trade-off worth discussing if asked.

## Problem 2 — BK-tree gives poor performance
Not universal — check: poor dictionary distribution, search radius too large, highly similar words, small dictionary, **or unshuffled insertion order (see Section 14)**. This is itself a good learning outcome to describe.

## Problem 3 — Huge output from autocomplete
Add a result limit (see Section 17) rather than printing everything.

## Problem 4 — Duplicate words
Normalize and deduplicate during dictionary loading (see Section 6).

## Problem 5 — Frequency data isn't real
Most simple public word lists are just words, no usage frequency. **Decide now, before you're asked in an interview, whether your frequency data is real or synthetic/approximated — and be ready to say so plainly.** This is a small integrity detail; getting caught overstating it damages trust in everything else you say about the project.

---

# 21. Edge cases to explicitly test
Don't skip this — these are the actual bugs you'll hit in practice, not theoretical ones:
- Empty query string
- Empty dictionary (zero words loaded)
- Single-character prefix (e.g., `prefix a`) — should not crash, should respect the output cap
- Query longer than any word in the dictionary
- Prefix matching zero words
- Fuzzy query with `maxDistance = 0` (should behave like exact match)
- Word appearing twice in the dictionary file (dedup check)

---

# 22. Novelty

The novelty is **not** "I created a spell checker." The novelty is:

> A multi-strategy retrieval engine that selects data structures according to query semantics and experimentally compares their performance.

```text
Exact   → Hash
Prefix  → Trie
Fuzzy   → BK-tree
Baseline → Linear scan
```

Then you demonstrate the difference experimentally — that's substantially stronger than any single-technique spell checker.

---

# 23. What you should learn before starting

**C++:** classes, references, pointers, STL, templates basics, RAII, const correctness, CMake.
**Algorithms:** hashing, DFS, dynamic programming, edit distance, sorting, complexity analysis.
**Data structures:** hash table, trie, linked list, tree, BK-tree.

---

# 24. What not to add
React, REST API, MongoDB, Web UI, cloud, LLM. None of these improve this project's core purpose.

---

# 25. Complexity summary — know this table cold

| Structure | Search | Insert | Space | Notes |
|---|---|---|---|---|
| Linear scan | O(n·m) | O(1) | O(n) | n = dict size, m = word length; the baseline |
| Hash table | O(1) avg | O(1) avg | O(n) | exact match only, no prefix/fuzzy capability |
| Trie | O(k) | O(k) | O(alphabet × nodes) | k = query/prefix length, independent of n |
| BK-tree | practical, not worst-case bounded | O(depth) | O(n) | performance depends on tree balance/distribution |

---

# 26. Final outcome

When complete, you should be able to honestly say:

> "I built an indexing/search engine in C++ that supports exact, prefix, and fuzzy retrieval. I implemented the data structures myself and benchmarked different retrieval strategies — with proper methodology, not casual timing — to understand their real performance trade-offs."

---

# 27. Resume entry

**LexiCore — High-Performance Fuzzy Search Engine | C++20, STL**

> • Upgraded a C++ spell-checker into a multi-strategy search engine using trie-based prefix retrieval and BK-tree edit-distance pruning for fuzzy search.
> • Built a benchmark suite (Release build, median-of-5 trials) comparing linear scan, hash lookup, trie traversal, and BK-tree search across dictionary sizes from 1K to 100K words, and added frequency-weighted ranking for ambiguous matches.

*(Only add specific speedup numbers once you've actually measured them in `RESULTS.md`.)*

---

# 28. Interview questions you must eventually master

```text
Why trie instead of unordered_map?
Why BK-tree?
How does triangle-inequality pruning work?
How does edit distance work? What's its complexity?
What is trie complexity, and why is it independent of dictionary size?
Why is hash lookup unsuitable for prefix search?
What happens if BK-tree performs worse than expected — what would you check?
How does insertion order affect a BK-tree's shape, and how did you handle it?
How does ranking work?
What was your benchmark methodology — trials, build mode, warm-up?
How did dictionary size affect performance?
What is the memory complexity of each structure?
Is your frequency data real or synthetic?
```

If you can confidently answer all of these, LexiCore is interview-ready.
