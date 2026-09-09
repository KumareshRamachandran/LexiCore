# LexiCore — Complete Project Guide
### High-Performance Fuzzy Search & Autocomplete Engine

**Time budget:** 2-3 days
**Builds on:** your existing Autocorrect Spell Checker (C++, STL, File I/O)
**Risk level:** Low — no new unfamiliar systems concepts, purely DSA you already have strength in

---

## 1. What This Project Actually Is

A search engine that takes a dictionary of words and answers three kinds of queries efficiently:
1. **Exact match** — "is this word in the dictionary?"
2. **Prefix match** — "what words start with 'kumar'?" (autocomplete)
3. **Fuzzy match** — "what words are close to 'ressturant'?" (spell correction / typo tolerance)

...and then **proves** each data structure choice was correct by benchmarking it against the naive alternative, instead of just asserting it's fast.

This is not a toy exercise — it's the actual retrieval core of every autocomplete/spellcheck system you've ever used, scaled down to something you build and fully understand.

---

## 2. Tools & Tech Stack

| Component | Choice | Why |
|---|---|---|
| Language | C++20 | Matches your existing project, matches your resume's core strength |
| Data structures | STL only (`unordered_map`, `vector`, `priority_queue`) | No external libs — everything you write is something you can explain |
| Build system | CMake | Standard, resume-relevant, easy for anyone to clone and run |
| Dictionary source | A public plain-text word list (10K–100K+ words) | Free, widely available, gives you a real-scale dataset instead of 20 test words |
| Benchmarking | `<chrono>` for timing, a small custom harness | No need for external benchmarking libraries — a simple timed loop is enough and easier to explain |

---

## 3. Architecture

```
                    Dictionary File (word list)
                              │
                        Tokenizer/Loader
                    (lowercase, strip punctuation)
                              │
              ┌───────────────┼───────────────┬───────────────┐
              ▼               ▼               ▼               ▼
        Linear Scan      Hash Table         Trie          BK-Tree
        (baseline)      (exact match)   (prefix search)  (fuzzy search)
              │               │               │               │
              └───────────────┴───────┬───────┴───────────────┘
                                       ▼
                              Frequency-based Ranking
                                       ▼
                                Query Results
                                       ▼
                              Benchmark Harness
                        (compare all 4 across dictionary sizes)
```

---

## 4. Features — Full Breakdown

### Core (must-have)
- **Dictionary loader** — reads a word list file into memory, normalizes casing/punctuation.
- **Linear scan baseline** — the "naive" version everything else is measured against.
- **Hash table exact match** — O(1) average lookup, using `unordered_map<string, int>` (word → frequency).
- **Trie for prefix search / autocomplete** — returns all words matching a given prefix.
- **BK-tree for fuzzy search** — returns words within a given edit-distance threshold of a misspelled query.
- **Frequency-weighted ranking** — when multiple fuzzy matches exist, rank by a combination of edit distance and word frequency.
- **Benchmark suite** — times all 4 strategies across multiple dictionary sizes (e.g., 1K / 10K / 100K words) and multiple query types.
- **CLI interface** — interactive query loop so you (and anyone reviewing your project) can actually try it live.

### Stretch (only if time remains)
- **Autocomplete ranking by prefix + frequency** — not just "all matches," but "top N most likely completions."
- **Levenshtein automaton** — an even faster fuzzy-search technique than brute-force BK-tree traversal, if you want an extra differentiator.
- **Simple REST-free HTTP-less web demo** — skip this. Don't reintroduce web-dev scope into a project whose whole point is avoiding that risk.

---

## 5. Build Plan (Day by Day)

### Day 1 — Trie (prefix search / autocomplete)
1. Define a `TrieNode`: `unordered_map<char, TrieNode*> children`, plus `bool isEndOfWord` and optionally `int frequency`.
2. Implement `insert(const string& word)` — walk/create nodes character by character.
3. Implement `search(const string& word)` — exact match via trie traversal (mostly for completeness/comparison).
4. Implement `startsWith(const string& prefix)` — traverse to the prefix node, then DFS from there collecting all complete words below it.
5. Load your real dictionary file and sanity-test with a handful of prefixes.

**You already know this shape from CP** — the main new part is thinking about it as a *retrieval system* (returning ranked results) rather than a single boolean answer.

### Day 2 — BK-tree (fuzzy search)
1. Reuse your existing edit-distance (Levenshtein) function from Autocorrect — no need to rewrite it.
2. Define a `BKNode`: a word, plus `unordered_map<int, BKNode*> children` keyed by edit distance from the parent.
3. Implement `insert(const string& word)`:
   - If the tree is empty, this word becomes the root.
   - Otherwise, compute `distance(word, currentNode)`, descend to `children[distance]` if it exists, else create it there.
4. Implement `search(const string& query, int maxDistance)`:
   - At each node, compute `d = distance(query, node)`.
   - If `d <= maxDistance`, add this node's word to results.
   - **The pruning step (this is the actual algorithmic insight):** only recurse into children keyed by distances in the range `[d - maxDistance, d + maxDistance]` — this is the triangle inequality in action, and it's what makes a BK-tree faster than checking every word.
5. Build the tree from your full dictionary, test fuzzy queries against known misspellings.

### Day 3 — Ranking, dictionary scale-up, and benchmark suite
1. Add frequency data to your dictionary (either from a real frequency-annotated word list, or approximate it — be honest about which if asked).
2. Ranking function: sort fuzzy-search results primarily by edit distance (ascending), secondarily by frequency (descending) for ties.
3. **Build the benchmark harness:**
   - For each of the 4 strategies (linear, hash, trie, BK-tree), time a fixed batch of queries.
   - Repeat at dictionary sizes: 1K, 10K, 100K words (or whatever your source data supports).
   - Record and tabulate real numbers — do not estimate or invent these.
4. Write a short `RESULTS.md` in the repo with your actual benchmark table — this becomes both your proof and your resume metric source.

---

## 6. Novelty — What Makes This Stand Out

Most student resumes with a "spell checker" project stop at edit-distance correction on a small word list. Here's what differentiates LexiCore specifically:

1. **The benchmark-driven design** — you're not just picking a data structure and hoping it's right, you're empirically proving the trade-off (linear vs. hash vs. trie vs. BK-tree) at multiple scales. This is a genuinely rare thing to see in a student project and signals engineering maturity, not just "I know a data structure."
2. **The BK-tree itself** — most spell-checkers use brute-force edit distance against every word, or a simpler trie-only approach. A BK-tree with triangle-inequality pruning is a legitimately more sophisticated technique that most CS students haven't implemented, even if they've heard of edit distance.
3. **Multi-strategy retrieval in one system** — exact, prefix, and fuzzy search side by side, each backed by the *right* structure for that query type, is a more complete system than any single-technique spell checker.

---

## 7. Problems You'll Likely Hit (and how to think about them)

| Problem | Why it happens | How to think through it |
|---|---|---|
| Trie memory usage blows up on large dictionaries | Each node allocates a full `unordered_map` even for nodes with 1 child | Acceptable for this project's scale; if asked, you can discuss a compressed trie (radix tree) as the production fix — you don't need to implement it, just understand the trade-off |
| BK-tree becomes unbalanced/skewed | Insertion order affects tree shape — alphabetically sorted input creates long chains | Shuffle your dictionary before inserting; mention this as a known BK-tree characteristic if asked |
| Edit distance is slow for very long words | Classic DP edit distance is O(n·m) per comparison | For this project's word-length scale (natural language words, not paragraphs) this is fine — flag it as a scaling limitation, not a bug |
| Benchmark numbers look "too good" for the trie/BK-tree at small dictionary sizes | Overhead of the more complex structures isn't worth it until the dictionary is large enough | This is actually a *good* result to report — it shows you understand that data-structure choice depends on scale, not just "always pick the fancy one" |
| Frequency data isn't available in your word list | Most simple word lists are just words, no usage frequency | Either find a frequency-annotated list, or state clearly in your README that frequency is approximated/synthetic — don't misrepresent this |

---

## 8. Learning Outcomes (map these explicitly when talking about the project)

- **Tries** — prefix-based retrieval, O(k) operations independent of dictionary size.
- **BK-trees** — metric-space search structures, triangle-inequality pruning.
- **Edit distance (Levenshtein)** — classic DP, reused and extended from prior work.
- **Empirical algorithm analysis** — benchmarking real implementations instead of relying on theoretical Big-O alone.
- **Trade-off reasoning** — understanding *when* a fancier data structure is worth its overhead, not just that it exists.

---

## 9. Resume Bullet Draft

> **LexiCore — High-Performance Fuzzy Search & Autocomplete Engine | C++20, STL**
> • Upgraded a spell-checker into a multi-strategy search engine, implementing a trie for O(k) prefix/autocomplete retrieval and a BK-tree with triangle-inequality pruning for fuzzy search, reducing fuzzy-query latency by [X]% over linear scan at a [N]-word dictionary scale.
> • Built a benchmark suite empirically comparing linear scan, hash lookup, trie, and BK-tree retrieval across dictionary sizes from 1K to [N] words, quantifying the scale at which each structure's overhead becomes worthwhile.
> • Implemented frequency-weighted ranking to surface the most likely correction among multiple fuzzy matches.

*(Only fill in [X]% and [N] once you've actually run the benchmark — use your real numbers.)*

---

## 10. Interview Defense — Questions You Must Answer Cold

- **"Why is a BK-tree faster than linear scan for fuzzy search?"**
  → Triangle inequality lets you prune entire subtrees without computing full edit distance against every word — you only recurse into children whose distance range could possibly contain a match within your threshold.

- **"What's the time complexity of trie insert/search?"**
  → O(k), where k is word length — independent of how many words are in the dictionary. This is the core advantage over a hash map for prefix queries, which can't do prefix matching at all.

- **"Why not just use a hash map for everything?"**
  → Hash maps give O(1) exact match but offer no way to do prefix or fuzzy queries — you'd have to scan every key, which defeats the purpose. Different query types need different structures.

- **"Walk me through your edit-distance function."**
  → Standard DP: `dp[i][j]` = minimum edits to transform the first `i` characters of word A into the first `j` characters of word B, built from insert/delete/substitute base cases.

- **"What would you change if the dictionary were 10x larger?"**
  → Good answer: discuss compressed tries (radix trees) to cut memory overhead, and note that the benchmark data itself should guide the decision — this is a great place to reference your own benchmark results as evidence, not guesswork.

- **"Is your frequency data real or synthetic?"**
  → Answer honestly based on what you actually did — this is a small detail that, if you're caught misrepresenting it, damages trust in everything else you say about the project.

---

## 11. Definition of Done
- [ ] Trie supports insert, exact search, and prefix search
- [ ] BK-tree supports insert and fuzzy search with pruning
- [ ] Ranking combines edit distance + frequency
- [ ] Benchmark suite runs and produces a real results table (in repo as `RESULTS.md`)
- [ ] CLI lets you interactively query all three modes
- [ ] You can explain every function in the codebase without looking at the code
