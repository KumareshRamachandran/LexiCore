# LexiCore Study Guide — Phase 0: Foundations

> **Who this is for:** Competitive programmer (CF Specialist) reading LexiCore for placement prep.
> You already know: arrays, maps, sets, basic DP, recursion, DFS/BFS.
> You need to fill gaps in: C++ ownership model, STL internals, and the theory behind each data structure in this project.

---

## 1. C++ Concepts You Must Internalize

### 1.1 `std::unique_ptr` — The Core Ownership Model

LexiCore uses `unique_ptr` for every tree node. If you don't understand this, the trie and BK-tree code will be opaque.

**The problem it solves:**
```cpp
// Raw pointer — you must manually delete, easy to forget or double-delete
TrieNode* node = new TrieNode();
// ... lots of code ...
delete node; // miss this → memory leak, double-delete → crash

// unique_ptr — automatically deleted when it goes out of scope
std::unique_ptr<TrieNode> node = std::make_unique<TrieNode>();
// No delete needed. When node is destroyed, TrieNode is destroyed too.
```

**Key rules:**
- Only ONE `unique_ptr` can own an object at a time (hence "unique")
- When the `unique_ptr` is destroyed, the object it points to is also destroyed
- You **cannot copy** a `unique_ptr` — you can only `std::move()` it
- Get the raw pointer with `.get()` when you need to traverse without transferring ownership

**In this project:**
```cpp
// TrieNode owns its children
std::unordered_map<char, std::unique_ptr<TrieNode>> children;

// Access child without giving up ownership
TrieNode* current = root_.get(); // .get() returns raw pointer, no transfer
```

**Why it matters in interviews:**
> "Who owns each node, and who deletes it?"
> Answer: "Every parent uniquely owns its children via unique_ptr. Destruction is automatic and top-down when the root is destroyed — no manual delete anywhere."

### 1.2 RAII (Resource Acquisition Is Initialization)

The principle behind `unique_ptr`:
- **Acquire** a resource (memory, file handle) in the constructor
- **Release** it in the destructor
- This ensures no resource leaks even when exceptions are thrown

```cpp
// RAII example — file is closed automatically
{
    std::ifstream file("words.txt"); // acquire in ctor
    // use file...
} // destructor runs here, file closed automatically
```

All of LexiCore's memory management follows this. No explicit `delete` anywhere.

### 1.3 Move Semantics

```cpp
// This is a MOVE, not a copy
std::unique_ptr<Node> a = std::make_unique<Node>("hello");
std::unique_ptr<Node> b = std::move(a); // a is now nullptr, b owns it
```

In BK-tree:
```cpp
BKNode(std::string w) : word(std::move(w)) {} // move string in, no copy
```

### 1.4 `const` Correctness

You'll see `const` everywhere in the project:
```cpp
const TrieNode* findNode(const std::string& prefix) const;
//    ^--- ptr points to const    ^--- pass by const ref   ^--- method doesn't modify this
```

**Rule:** If a method doesn't modify the object, mark it `const`. If a parameter is read-only, pass `const&`. This is an interview-tested C++ discipline.

### 1.5 STL Containers You Need to Know Cold

| Container | Lookup | Insert | When to use |
|---|---|---|---|
| `unordered_set<T>` | O(1) avg | O(1) avg | "Is X in the set?" |
| `unordered_map<K,V>` | O(1) avg | O(1) avg | "Map key → value" |
| `vector<T>` | O(n) | O(1) amortized | Ordered list |
| `priority_queue<T>` | O(1) top | O(log n) | Always want min/max |

**Why `unordered_map` for trie children instead of `array<Node*,26>`?**
- `array<26>` is faster (O(1) with no hash) but allocates 26 pointers per node even if a node has 1 child
- `unordered_map` only allocates for actual children — better for sparse nodes
- **Trade-off you must be able to state:** speed vs memory

### 1.6 `std::mt19937` — Seeded Random Number Generation

```cpp
std::mt19937 rng(42);                    // seed 42 → reproducible
std::shuffle(words.begin(), words.end(), rng); // deterministic shuffle
```

Why seed matters: Same seed → same shuffle every time → reproducible benchmarks.

---

## 2. Algorithm Theory — What You Need Before Reading the Code

### 2.1 Levenshtein Edit Distance — Derive It Yourself

**The problem:** Given strings A and B, minimum number of single-character insertions, deletions, substitutions to turn A into B.

**State definition:**
```
dp[i][j] = minimum edits to transform A[0..i-1] into B[0..j-1]
```

**Base cases:**
```
dp[0][j] = j  (j insertions to turn "" into B[0..j-1])
dp[i][0] = i  (i deletions to turn A[0..i-1] into "")
```

**Transition:**
```
if A[i-1] == B[j-1]:
    dp[i][j] = dp[i-1][j-1]        // characters match, no cost
else:
    dp[i][j] = 1 + min(
        dp[i-1][j],                  // delete A[i-1]
        dp[i][j-1],                  // insert B[j-1]
        dp[i-1][j-1]                 // substitute
    )
```

**Example:** "cat" → "bat"
```
    ""  b  a  t
""   0  1  2  3
c    1  1  2  3
a    2  2  1  2
t    3  3  2  1  ← answer is 1 (one substitution: c→b)
```

**Complexity:** O(n·m) time, O(n·m) space — but can be optimized to O(m) space using two rows.

**Critical property for BK-trees:**
1. **Symmetry:** `dist(A,B) == dist(B,A)`
2. **Triangle inequality:** `dist(A,C) ≤ dist(A,B) + dist(B,C)`

These two properties make edit distance a **metric** — and BK-trees only work with metrics.

### 2.2 Trie — Prefix Retrieval Tree

**Structure:** A tree where each path from root to a node spells out a string.

```
         root
          |
          a
          |
          p
          |
          p ← "app" prefix node
         / \
        l   l
        |   |
        e   y
(apple)   (apply)
```

**Key insight:** Lookup and insert are O(k) where k = string length, completely **independent** of how many strings are in the trie. This is the entire reason tries exist.

**Contrast with hash map:**
- Hash: O(k) exact lookup, cannot do prefix queries
- Trie: O(k) exact AND prefix queries

**Autocomplete algorithm:**
1. Traverse to the prefix node: O(k)
2. DFS from that node, collect all complete words: O(output size)
3. Total: O(k + output) — independent of dictionary size

### 2.3 BK-Tree — The Core Novelty

**Motivation:** Given a misspelled word "aple", find all dictionary words within edit distance 1. Naive: compute distance against every word → O(n) calls to O(k²) DP = very slow.

**BK-tree idea:** Organize dictionary words in a tree such that the tree structure encodes edit distances, allowing us to skip most words entirely.

**Insertion rule:** Each node stores a word. Children are indexed by their edit distance from the parent.

```
         "book"
        /       \
      d=1       d=2
      /           \
   "cook"        "back"
```

**Why this works — triangle inequality:**

If we're searching for words within distance `r` of query `q`, and we're at node with word `w`:
- Compute `d = dist(q, w)`
- If `d ≤ r` → `w` is a match
- For any child with edge label `k`:
  - Triangle inequality says: `|dist(q, child) - d| ≤ r` if child is within `r` of `q`
  - So `dist(q, child) ∈ [d-r, d+r]`
  - **Prune:** only recurse into children with edge labels in `[d-r, d+r]`

**The trap (critical correctness issue):**
You need the **true distance** `d` to compute the pruning interval `[d-r, d+r]`. If you use a bounded edit distance that returns a sentinel on bail-out, the interval is wrong and you silently skip valid results.

```
// WRONG - sentinel value corrupts pruning interval
int d = editDistanceBounded(query, node.word, maxDist); // might return maxDist+1

// RIGHT - true distance for pruning, bounded only for threshold check
int d = editDistance(query, node.word);  // always returns true value
if (d <= maxDist) { /* match */ }
// Prune: [d - maxDist, d + maxDist] is always correct
```

**Performance:** Depends on dictionary distribution. Not guaranteed O(log n). With a well-shuffled dictionary and small radius, practical pruning is significant. For large radius, performance degrades toward linear.

### 2.4 Bounded Edit Distance — The Optimization

Full DP computes all O(nm) cells. If you only care whether distance ≤ k:
- Only compute cells within a diagonal band of width `2k+1`
- Exit early if no cell in a row can possibly be ≤ k

This is faster in practice for small k (e.g., typo correction rarely needs k > 3).

**Length difference shortcut:**
```
if |len(A) - len(B)| > k: return k+1 immediately
```
Because even with optimal edits you need at least `|len(A) - len(B)|` operations.

---

## 3. Complexity Concepts for Interviews

### Big-O — What You Must Be Able to Say Out Loud

| Structure | Build | Query | Space | Key caveat |
|---|---|---|---|---|
| Linear scan | O(1) | O(n·k) | O(n) | baseline, always correct |
| Hash set/map | O(n) | O(1) avg | O(n) | exact match only, hash collisions |
| Trie | O(n·k) | O(k) | O(alphabet × nodes) | k = key length |
| BK-tree | O(n·k²) | practical | O(n) | distribution-dependent |

**O(k) vs O(n):** For trie, k is word length (~5–15 chars). n is dictionary size (100K). So O(k) is effectively O(1) relative to O(n).

### Amortized Complexity

`vector::push_back` is O(1) **amortized** — individual insertions are O(1), but occasional resizes are O(n). Over n insertions total cost is O(n) → O(1) per operation on average.

### Space-Time Trade-offs

A recurring interview topic in this project:
- Trie: fast queries, high memory (unordered_map per node)
- array[26] children: faster, even higher memory, limited alphabet
- unordered_map children: slower, lower memory, flexible alphabet

There is no universally "best" choice — the right answer depends on data.

---

## 4. Benchmark Methodology — What Makes a Benchmark Credible

### Things that make benchmarks meaningless

| Mistake | Why wrong |
|---|---|
| Debug build | Compiler optimizations off, code is 10–100× slower |
| Single run | OS scheduling, cache state cause noise |
| Different queries per structure | Not comparing the same workload |
| Not warming up | First run pays page fault costs |

### What LexiCore does correctly

1. **Release build only** (`-O2`) — `cmake -DCMAKE_BUILD_TYPE=Release`
2. **5 trials, median reported** — not mean, not single run
3. **Warm-up pass** before timing
4. **Identical query set** across all strategies
5. **Separate build time from query time** — a structure can have slow build but fast queries
6. **p50/p95/p99** instead of just average — average hides tail latency

### The paired comparison design

Each query type is benchmarked against only the structures that answer it:
- **Exact:** linear scan vs hash
- **Prefix:** linear prefix scan vs trie
- **Fuzzy:** linear edit-distance scan vs BK-tree

This is technically defensible — you're not comparing a trie's fuzzy performance against a BK-tree's prefix performance.

---

## 5. C++ Sanitizers — What They Are and Why They Matter

During development, LexiCore is built with:
```bash
-fsanitize=address,undefined
```

**AddressSanitizer (ASan):**
- Detects: heap-use-after-free, heap buffer overflow, stack buffer overflow, double-free, memory leaks
- Cost: ~2x runtime overhead, ~3x memory overhead

**UndefinedBehaviorSanitizer (UBSan):**
- Detects: signed integer overflow, null pointer dereference, out-of-bounds access, misaligned access
- Cost: minimal overhead

**Why relevant in interviews:**
> "How did you verify there are no memory leaks?"
> "I ran with AddressSanitizer enabled on the entire test suite — zero violations."

---

## 6. SDE Interview Q&A — Phase 0 Topics

These are questions you should be able to answer without looking at code:

**C++ Ownership**
- Q: What is RAII and why does it matter in C++?
- Q: Difference between `unique_ptr`, `shared_ptr`, and `weak_ptr`?
- Q: Why can't you copy a `unique_ptr`?
- Q: When would you use `.get()` on a `unique_ptr`?
- Q: What happens to all the tree nodes when the root `unique_ptr` goes out of scope?

**Edit Distance**
- Q: Derive the Levenshtein DP recurrence from scratch.
- Q: What are the two metric properties of edit distance?
- Q: Why is edit distance O(n·m)? Can you reduce the space?
- Q: What's a bounded edit distance optimization and when does it help?
- Q: What's the edit distance between "kitten" and "sitting"? (Answer: 3)

**Tries**
- Q: Why is trie lookup O(k) and not O(n)?
- Q: How does autocomplete work on a trie?
- Q: What's the memory trade-off between `unordered_map` children and `array[26]` children?
- Q: When would a hash map be better than a trie?

**BK-Trees**
- Q: What makes edit distance suitable as a BK-tree metric?
- Q: Explain triangle-inequality pruning step by step.
- Q: Why must you shuffle before inserting into a BK-tree?
- Q: What can go wrong if you use a bounded edit distance for the pruning interval?
- Q: Is BK-tree O(log n)? What determines its practical performance?

**Benchmarking**
- Q: Why benchmark in Release mode?
- Q: What's the difference between average latency and p99 latency? When does p99 matter more?
- Q: What is a warm-up run and why is it needed?
- Q: Why report median instead of mean for benchmark trials?

---

## 7. Reading Order Checklist (before Phase 1)

Before reading any code, make sure you can:

- [ ] Derive Levenshtein DP recurrence on paper (don't look it up)
- [ ] Prove triangle inequality for edit distance on paper: `dist(a,c) ≤ dist(a,b) + dist(b,c)`
- [ ] Draw a trie for ["apple", "apply", "apt", "banana"] on paper
- [ ] Trace a BK-tree insertion for ["book", "cook", "back", "hook"] on paper (use edit distances)
- [ ] State the BK-tree search pruning rule in one sentence
- [ ] Explain what `unique_ptr` does and why you'd use it
- [ ] State one scenario where a trie is better than a hash map
- [ ] State one scenario where BK-tree is better than linear scan, and one where it isn't

Once you can do all of the above, proceed to Phase 1.
