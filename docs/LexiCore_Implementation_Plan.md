# LexiCore — Implementation Plan
### High-Performance Fuzzy Search & Autocomplete Engine (C++20 / STL)

**Time budget:** 2–3 days. Day 1 = Trie. Day 2 = BK-tree. Day 3 = ranking + benchmarking + writeup.
**Builds on:** your existing C++ autocorrect spell-checker (STL, file I/O, edit distance).
**Risk level:** low — pure DSA territory you already have strength in, no new frameworks.

---

## 1. What you're building and why

LexiCore turns a spell-checker into a small multi-strategy retrieval engine that answers three query types, each with the data structure suited to it:

| Query type | Structure | Complexity |
|---|---|---|
| Exact match | Hash set | O(1) avg |
| Prefix / autocomplete | Trie | O(k) traversal, independent of dictionary size |
| Fuzzy / typo-tolerant | BK-tree | Practical, not worst-case bounded — depends on distribution |
| Baseline | Linear scan | O(n·m) — what everything else is measured against |

The novelty is **not** "I built a spell checker." It's:

> A multi-strategy retrieval engine that selects data structures according to query semantics, validates them for correctness against a baseline, and experimentally proves their performance trade-offs.

That's a materially stronger story than a single-technique project, and it only holds up if you actually measure things instead of asserting them.

---

## 2. Tech stack

- **Language:** C++20
- **Structures:** STL only — `unordered_map`, `unordered_set`, `vector`, `list`, `queue`, `priority_queue`, `string`, `fstream`, `chrono`, `algorithm`, `memory`
- **Build:** CMake, with explicit Debug/Release configs
- **Dictionary:** a public plain-text word list, 10K–100K+ words
- **Benchmarking:** `<chrono>` + a small custom harness — no external benchmark library needed

**Explicitly out of scope:** React, REST APIs, MongoDB, any web UI, cloud services, LLM integration. None of these serve the project's purpose, and adding them dilutes the C++/DSA story you're trying to tell.

---

## 3. Architecture

```
                    Dictionary File
                          │
                    Loader / Normalizer
                  (lowercase, strip punctuation)
                          │
              ┌───────────┼───────────┬──────────────┐
              ▼           ▼           ▼              ▼
        Linear Scan   Hash Table     Trie          BK-Tree
        (baseline)   (exact match) (prefix)   (fuzzy, pruned)
              │           │           │              │
              └───────────┴─────┬─────┴──────────────┘
                                 ▼
                        Frequency-weighted Ranking
                                 ▼
                             CLI Output
                                 ▼
                        Benchmark Harness
                 (compares all 4 across dictionary sizes)
```

---

## 4. Repository layout

```
lexicore/
├── CMakeLists.txt
├── README.md
├── RESULTS.md              # real, measured benchmark numbers — separate from narrative README
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
    ├── test_bktree.cpp
    └── test_bktree_vs_baseline.cpp   # correctness oracle, see §7
```

Simple assertion-based tests are fine — you don't need a testing framework.

---

## 5. Day-by-day build plan

### Day 1 — Dictionary, normalization, baseline, edit distance, Trie

1. **Dictionary loader.** Read the word list into `vector<string>`, build an `unordered_set<string>` for exact lookup, and optionally an `unordered_map<string,int>` for frequency. **Deduplicate on load** — decide now whether duplicates get skipped or merged into a frequency count, don't let it become a silent bug later.
2. **Normalization.** Lowercase, strip punctuation, normalize whitespace. Keep the policy simple and write it down in the README — "I made a deliberate, simple choice and documented it" is a strong interview answer.
3. **Baseline linear fuzzy search — build this before anything clever.** For every query, compute edit distance against every dictionary word, keep matches within threshold. This is both your performance baseline *and* your correctness oracle for the BK-tree later (§7).
4. **Edit distance (Levenshtein DP).** Reuse from your autocorrect project if possible.
   ```
   dp[i][j] = min(
       dp[i-1][j] + 1,       // deletion
       dp[i][j-1] + 1,       // insertion
       dp[i-1][j-1] + cost   // substitution, cost = 0 if chars match else 1
   )
   ```
   O(n·m) time/space — fine for word-length strings, would not scale to long documents. Say so if asked.
5. **Hash-table exact search.** `unordered_set<string>`, average O(1). This is your clean "hash maps are great for exact match, useless for prefix/fuzzy" talking point.
6. **Trie.** Node holds `children` + `isEndOfWord` (+ optional frequency). Implement `insert`, `search`, `startsWith`. For `startsWith`, traverse to the prefix node (O(k)) then DFS to collect completions (O(output)) — independent of dictionary size, which is the key interview point.
7. Sanity-test against your real dictionary with a handful of prefixes.

### Day 2 — BK-tree

1. Node holds a `word` + `unordered_map<int, BKNode*> children` keyed by edit distance from the parent.
2. **Insertion:** empty tree → new node becomes root. Otherwise compute distance from current node, descend to `children[distance]`, recurse or insert. Implement this yourself, don't reach for a library.
3. **Shuffle before inserting.** If you insert an alphabetically-sorted (or otherwise correlated) dictionary, you get a skewed, chain-like tree and benchmark numbers that look artificially bad for reasons unrelated to your implementation. Fix: `std::shuffle` with a fixed seed (for reproducibility) before building the tree. Mention this explicitly in the README — it's a genuine, known BK-tree characteristic and signals real understanding.
4. **Fuzzy search with pruning.** At each node, `d = editDistance(query, node.word)`. If `d ≤ maxDistance`, it's a match. Only recurse into children whose keyed distance falls in `[d - maxDistance, d + maxDistance]` — this triangle-inequality pruning is the core idea you must be able to explain. Do **not** claim guaranteed O(log n) — practical performance depends on dictionary distribution, the metric, and the search radius.
5. **(Optional, worth doing) Threshold-aware edit distance.** A full Levenshtein DP computes the whole matrix even though you only care whether `distance ≤ maxDistance`. You can skip work outside the relevant diagonal band or bail out early once a row can no longer produce a valid result. Useful because edit-distance computation itself can dominate BK-tree query cost. Don't claim an improved Big-O bound unless your implementation actually earns it — describe it as "less work in practice for small thresholds."
6. Build the tree from the full (shuffled) dictionary and test fuzzy queries against known misspellings.

### Day 3 — Correctness, ranking, benchmarking, writeup

1. **Correctness before performance.** Before trusting any benchmark number, validate the BK-tree against the linear-scan baseline: for a battery of test queries and thresholds, the BK-tree's result set must exactly match the baseline's. This separates "is it right" from "is it fast," and catches pruning-logic bugs that would otherwise just look like weird performance numbers.
2. **Ranking.** Primary key: smaller edit distance. Secondary (tie-break): higher frequency. Recommended tertiary tie-break: lexicographic order, so output is deterministic across runs and easy to test. Keep the ranking function explainable in one sentence.
3. **Frequency data.** Most public word lists don't have real usage frequency. Decide now — real, approximated, or synthetic — and be ready to say so plainly if asked. Overstating this is a small integrity slip that undermines trust in everything else you say about the project.
4. **CLI.** Interactive loop: `exact <word>`, `prefix <p>`, `fuzzy <q>`, `benchmark`. **Cap output size** — if `prefix a` matches 50,000 words, show the top N by frequency plus a note like "...and 49,980 more." This is both a UX call and evidence you thought about failure modes.
5. **Benchmark suite** — see §6 below for methodology.
6. **README + RESULTS.md.** README carries the narrative (architecture, design rationale, trade-offs, known limitations). RESULTS.md carries only the actual measured numbers and the environment they were measured in. Never quote a speedup number that RESULTS.md doesn't back up.

---

## 6. Benchmark methodology — do it properly

- **Release build only.** `-O2` or CMake's `Release` config. A Debug-build comparison is close to meaningless and an easy thing to get challenged on.
  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build
  ```
- **Multiple trials, report the median**, not a single run — single runs are noisy (scheduling, cache state, background load).
- **Warm-up run before timing** — the first execution pays one-time costs (page faults, cache population) that steady-state queries don't.
- **Same query set across all four strategies** in a given trial — different queries per structure isn't a fair comparison.
- **Separate index-build time from query time.** A structure can have expensive construction but excellent query latency — that trade-off matters and gets lost if you only report one number.
- **Report distribution, not just an average**, where practical: p50/p95/p99 alongside queries/sec, especially for BK-tree where pruning effectiveness can vary a lot by query.
- **Record your environment** (CPU, compiler, flags, OS) in `RESULTS.md` — this is what makes the numbers a credible claim rather than an assertion.
- **Sweep dictionary sizes:** 1K / 10K / 100K words (or whatever your source supports).

Document methodology explicitly:
```
Dictionary: 100K words
Queries: 10K (same set across all 4 strategies)
Trials: 5, median reported
Build: Release / -O2
Hardware: ...
```
Then publish the real measurements. Only put speedup numbers on your resume once RESULTS.md actually contains them.

---

## 7. Testing strategy

Keep these categories distinct — don't mix correctness assertions with performance timing in the same test:

- **Unit tests** — edit distance, trie, BK-tree in isolation.
- **Cross-implementation (correctness oracle)** — BK-tree fuzzy results vs. linear-scan baseline results, must match exactly for the same query/threshold.
- **Integration tests** — dictionary → query → ranking → CLI, end to end.
- **Benchmark tests** — performance only, no assertions about correctness.

### Edge cases to explicitly test
- Empty query string
- Empty dictionary (zero words loaded)
- Single-character prefix (`prefix a`) — must not crash, must respect the output cap
- Query longer than any dictionary word
- Prefix matching zero words
- Fuzzy query with `maxDistance = 0` (should behave like exact match)
- Duplicate word in the dictionary file (dedup check)
- Very long words (edit-distance cost)
- Large search radius (weak-pruning stress case for BK-tree)
- Highly similar dictionary words (stresses BK-tree branching)
- No matching result at all

### Also worth doing (lower priority, good if time allows)
- **AddressSanitizer / UndefinedBehaviorSanitizer** during development — worthwhile for any C++ project built around raw trees and pointers.
- **Deliberate memory ownership.** Prefer `unique_ptr<Node>` for trie/BK-tree nodes where practical. If you use raw pointers internally, be able to answer: who owns each node, who deletes it, can a node outlive its parent? This is a real interview dimension you get almost for free.
- **Error handling in the CLI.** Missing dictionary file, malformed line, invalid command, invalid radius, negative limit — fail with a clear message, don't crash.

---

## 8. Known trade-offs to have ready

| Issue | Cause | How to talk about it |
|---|---|---|
| Trie memory usage | `unordered_map<char,Node*>` per node adds up on large dictionaries | Genuine speed-vs-memory choice vs. `array<Node*,26>`; a compressed/radix trie is the production answer, you don't need to implement it, just understand it |
| BK-tree underperforms | Poor dictionary distribution, radius too large, highly similar words, small dictionary, or **unshuffled insertion order** | Not universal — this is itself a good thing to be able to diagnose out loud |
| Small dictionaries make trie/BK-tree look worse than linear scan | Structural overhead isn't worth it below some size | Good result to report — shows you understand structure choice depends on scale, not "always pick the fancy one" |
| Frequency data isn't real | Most public word lists have no usage stats | State plainly whether yours is real, approximated, or synthetic |

---

## 9. Definition of done

- [ ] Trie supports insert, exact search, prefix search
- [ ] BK-tree supports insert and fuzzy search with triangle-inequality pruning
- [ ] BK-tree results validated against linear-scan baseline for correctness
- [ ] Ranking combines edit distance + frequency, deterministic tie-breaking
- [ ] Benchmark suite runs in Release mode, median-of-N trials, produces `RESULTS.md`
- [ ] CLI supports `exact`, `prefix`, `fuzzy`, `benchmark`, with output capping
- [ ] Edge cases from §7 covered by tests
- [ ] README documents normalization policy, frequency-data honesty, BK-tree shuffle rationale, and structure trade-offs
- [ ] You can explain every function in the codebase without looking at the code

---

## 10. Resume bullets (fill in numbers only after RESULTS.md exists)

**LexiCore — High-Performance Fuzzy Search Engine | C++20, STL**
> • Upgraded a C++ spell-checker into a multi-strategy search engine using trie-based O(k) prefix retrieval and a BK-tree with triangle-inequality pruning for fuzzy search, validated for correctness against a linear-scan baseline.
> • Built a benchmark suite (Release build, median-of-5 trials) comparing linear scan, hash lookup, trie, and BK-tree across dictionary sizes from 1K to 100K words; added frequency-weighted, deterministically-ranked results for ambiguous matches.

---

## 11. Interview questions to master

```
Why trie instead of unordered_map for prefix search?
Why BK-tree instead of brute-force fuzzy search?
How does triangle-inequality pruning work?
How does edit distance work, and what's its complexity?
Why is trie complexity independent of dictionary size?
Why is hash lookup unsuitable for prefix/fuzzy search?
How does insertion order affect BK-tree shape — how did you handle it?
How did you validate BK-tree correctness before benchmarking it?
What was your benchmark methodology — trials, build mode, warm-up, environment?
How did dictionary size affect relative performance?
What's the memory complexity of each structure?
Who owns your trie/BK-tree nodes, and how do you know there's no leak or dangling pointer?
Is your frequency data real or synthetic?
What would you check first if BK-tree performed worse than expected?
```

If you can answer all of these cold, LexiCore is interview-ready.
