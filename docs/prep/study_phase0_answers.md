# LexiCore — Phase 0 Interview Answer File

> Written as spoken interview answers — concise, precise, ready to deliver.
> Format: **Short answer first** (1–2 sentences), then **deep explanation** if the interviewer probes.

---

## Section A — C++ Ownership

---

### Q1: What is RAII and why does it matter in C++?

**Short answer:**
RAII means tying a resource's lifetime to an object's lifetime — you acquire the resource in the constructor and release it in the destructor. It matters because C++ has no garbage collector, so RAII is the idiomatic way to guarantee no leaks or double-frees even when exceptions occur.

**Deep:**
```cpp
// Without RAII — leak if exception thrown between new and delete
Node* n = new Node();
riskyOperation(); // throws? → n is never deleted → leak
delete n;

// With RAII (unique_ptr) — destructor runs even if exception thrown
auto n = std::make_unique<Node>();
riskyOperation(); // throws? → n goes out of scope → Node deleted automatically
```

In LexiCore, every trie and BK-tree node is owned by a `unique_ptr`. When the root is destroyed (e.g., `Trie` goes out of scope), the destructor cascade fires top-down through `unique_ptr` chains — zero manual `delete`, zero leaks. This was verified by running the full test suite under AddressSanitizer.

---

### Q2: Difference between `unique_ptr`, `shared_ptr`, and `weak_ptr`?

**Short answer:**
`unique_ptr` = single owner, zero overhead, not copyable. `shared_ptr` = shared ownership via reference count, copyable, overhead. `weak_ptr` = non-owning observer of a `shared_ptr`, breaks cycles.

**Deep:**

| Smart pointer | Ownership | Overhead | Use when |
|---|---|---|---|
| `unique_ptr<T>` | Exactly 1 owner | Zero (same as raw pointer) | Default choice for heap objects |
| `shared_ptr<T>` | Multiple owners | Ref-count (atomic, expensive) | Graph nodes, shared caches |
| `weak_ptr<T>` | No ownership | Small | Observing without extending lifetime; breaking `shared_ptr` cycles |

**Why LexiCore uses `unique_ptr`:**
Tree nodes always have exactly one parent. There's no shared ownership scenario. `shared_ptr` would add unnecessary atomic reference-count overhead for no benefit.

**Memory layout:**
```
unique_ptr<T>: [ptr]           → just one pointer, no heap allocation for control block
shared_ptr<T>: [ptr][ctrl_ptr] → extra pointer to heap-allocated control block
```

---

### Q3: Why can't you copy a `unique_ptr`?

**Short answer:**
Because copying would create two owners for the same object. When either owner's destructor ran, it would delete the object, leaving the other with a dangling pointer — undefined behavior. So the copy constructor and copy assignment are explicitly deleted.

**Deep:**
```cpp
auto a = std::make_unique<int>(42);
auto b = a;  // COMPILE ERROR — copy is deleted

// You can move: transfers ownership, a becomes nullptr
auto b = std::move(a);  // b owns the int, a == nullptr
```

`std::move` doesn't actually "move" data — it casts to an rvalue reference, telling the move constructor to take ownership and null out the source. After `std::move(a)`, accessing `a` is valid (it's nullptr) but dereferencing it is undefined behavior.

---

### Q4: When would you use `.get()` on a `unique_ptr`?

**Short answer:**
When you need to pass the raw pointer to a function that doesn't take ownership — typically for traversal or read-only access.

**Deep:**
```cpp
std::unique_ptr<TrieNode> root_ = std::make_unique<TrieNode>();

// Traversal — don't want to transfer ownership, just read
TrieNode* current = root_.get(); // borrow the pointer, unique_ptr still owns it

for (char c : word) {
    auto it = current->children.find(c);
    current = it->second.get(); // borrow child's pointer
}
```

**Rule of thumb:**
- Pass `unique_ptr&` if the function might transfer ownership later
- Pass raw `.get()` if the function just reads/traverses — it's a "borrow"
- Never store `.get()` results long-term — the `unique_ptr` might be destroyed

---

### Q5: What happens to all the tree nodes when the root `unique_ptr` goes out of scope?

**Short answer:**
The root's destructor fires, which destroys its `unique_ptr<children>` map, which destroys each child `unique_ptr`, which fires each child's destructor — recursively, the entire tree is destroyed top-down with no manual memory management.

**Deep:**
```cpp
{
    Trie trie;
    trie.insert("apple");
    trie.insert("apply");
} // trie destructor runs here:
  //   → ~Trie() destroys root_ (unique_ptr<TrieNode>)
  //     → ~TrieNode() destroys children (unordered_map<char, unique_ptr<TrieNode>>)
  //       → map destructor destroys each unique_ptr<TrieNode>
  //         → each child's ~TrieNode() fires
  //           → ... recursively until all leaf nodes are freed
```

This is O(n) time to destroy n nodes — exactly as expected. No leaks, no double-frees. AddressSanitizer confirms this in LexiCore's test suite.

---

## Section B — Edit Distance

---

### Q6: Derive the Levenshtein DP recurrence from scratch.

**Short answer:**
Define `dp[i][j]` = minimum edits to transform first i chars of A into first j chars of B. Base cases: `dp[0][j]=j`, `dp[i][0]=i`. Transition: if chars match, carry diagonal; else take minimum of delete/insert/substitute, each costing 1.

**Full derivation:**

**State:** `dp[i][j]` = min edits to turn `A[0..i-1]` into `B[0..j-1]`

**Base cases:**
```
dp[0][j] = j   // turn "" into B[0..j-1]: j insertions
dp[i][0] = i   // turn A[0..i-1] into "": i deletions
```

**Transition (the key insight — last operation on A[i-1]):**
```
Case 1: A[i-1] == B[j-1]
  → no cost for this character: dp[i][j] = dp[i-1][j-1]

Case 2: A[i-1] != B[j-1]
  → best of 3 operations:
    dp[i-1][j]   + 1  // delete A[i-1], now match A[0..i-2] to B[0..j-1]
    dp[i][j-1]   + 1  // insert B[j-1] at end, now match A[0..i-1] to B[0..j-2]
    dp[i-1][j-1] + 1  // substitute A[i-1]→B[j-1], now match A[0..i-2] to B[0..j-2]
  → dp[i][j] = 1 + min(dp[i-1][j], dp[i][j-1], dp[i-1][j-1])
```

**Worked example: "cat" → "bat" (answer: 1)**
```
     ""  b  a  t
""    0  1  2  3
c     1  1  2  3
a     2  2  1  2
t     3  3  2  1  ← dp[3][3] = 1 ✓
```

**Complexity:** O(nm) time. Space can be reduced to O(m) by keeping only previous row and current row.

---

### Q7: What are the two metric properties of edit distance?

**Short answer:**
Symmetry — `dist(A,B) = dist(B,A)`, and triangle inequality — `dist(A,C) ≤ dist(A,B) + dist(B,C)`. These two properties (plus non-negativity and identity) make edit distance a **metric**, which is the mathematical requirement for BK-trees to work.

**Deep:**

**Symmetry:** Every edit sequence from A→B has a corresponding reverse sequence B→A of the same length (deletions become insertions, vice versa). So the minimum must be equal.

**Triangle inequality proof sketch:**
- Let P = optimal path A→B (cost = dist(A,B))
- Let Q = optimal path B→C (cost = dist(B,C))
- Concatenating P then Q gives a path A→C of cost dist(A,B) + dist(B,C)
- The optimal path A→C can only be cheaper: dist(A,C) ≤ dist(A,B) + dist(B,C) ✓

**Why this matters for BK-trees:**
If `dist(query, node) = d` and we're searching radius `r`, any match `x` satisfies:
- `dist(query, x) ≤ r`
- By triangle inequality: `dist(node, x) ≥ dist(query, node) - dist(query, x) ≥ d - r`
- Also: `dist(node, x) ≤ dist(node, query) + dist(query, x) ≤ d + r`

So `dist(node, x) ∈ [d-r, d+r]` — any match lives in this child range.

---

### Q8: Why is edit distance O(n·m)? Can you reduce the space?

**Short answer:**
O(n·m) because we fill an (n+1)×(m+1) table where each cell depends only on its neighbors. Space reduces to O(m) by keeping only the previous and current rows — we never need rows older than one step back.

**Deep — 2-row optimization:**
```cpp
vector<int> prev(m + 1), curr(m + 1);
// Initialize prev as base case (prev[j] = j)
for (int i = 1; i <= n; i++) {
    curr[0] = i;
    for (int j = 1; j <= m; j++) {
        int cost = (A[i-1] == B[j-1]) ? 0 : 1;
        curr[j] = min({prev[j]+1, curr[j-1]+1, prev[j-1]+cost});
    }
    swap(prev, curr); // prev becomes curr for next iteration
}
// Answer is prev[m]
```

Space: O(m) — two vectors of size m+1. LexiCore uses this exact approach.

**Can time be reduced?** For small edit distances, yes — the bounded variant only computes a diagonal band of width `2k+1`, skipping cells guaranteed to exceed threshold. But worst-case time is still O(nm).

---

### Q9: What's a bounded edit distance optimization and when does it help?

**Short answer:**
Only compute DP cells within a diagonal band of width `2k+1` (where k = max distance you care about). Cells outside this band cannot possibly produce a result ≤ k. Also bail out early if an entire row has no value ≤ k.

**Deep:**
```
If we only care whether dist ≤ k=2, in each row i only columns [i-2, i+2] matter.
Everything outside that band will be > k regardless.

Band diagram for k=2, comparing "abc" (n=3) vs "abcde" (m=5):
Row i can only affect columns [i-2, i+2]
```

**Length shortcut first:**
```cpp
if (abs(n - m) > k) return k + 1; // length difference alone > k → impossible
```

**When it helps:**
- Small k (typo correction, k ≤ 3): most cells are skipped → significant speedup
- Large k or strings of similar length: band covers most of table → minimal benefit

**Critical note for BK-tree:**
Bounded edit distance returns `k+1` as a sentinel when distance exceeds k. This is **not the true distance**. Using this sentinel value in the BK-tree pruning interval `[d-r, d+r]` would give a wrong range and silently drop valid results. LexiCore uses true `editDistance()` for pruning and `editDistanceBounded()` only for threshold checks (yes/no questions).

---

### Q10: What's the edit distance between "kitten" and "sitting"? (Answer: 3)

**Short answer:**
3. The transformations are: kitten → sitten (substitute k→s), sitten → sittin (substitute e→i), sittin → sitting (insert g at end).

**Verification with DP:**
```
       ""  s  i  t  t  i  n  g
""      0  1  2  3  4  5  6  7
k       1  1  2  3  4  5  6  7
i       2  2  1  2  3  4  5  6
t       3  3  2  1  2  3  4  5
t       4  4  3  2  1  2  3  4
e       5  5  4  3  2  2  3  4
n       6  6  5  4  3  3  2  3  ← dp[6][7] = 3 ✓
```

---

## Section C — Tries

---

### Q11: Why is trie lookup O(k) and not O(n)?

**Short answer:**
Because the trie's structure encodes where each character is — lookup follows one edge per character of the query, regardless of how many other words are in the trie. The dictionary size n doesn't affect the traversal path length.

**Deep:**
```
Trie with 1,000,000 words:
  search("apple") → follow edges a→p→p→l→e → 5 steps total

Trie with 10 words:
  search("apple") → follow edges a→p→p→l→e → still 5 steps
```

This contrasts with hash map lookup (O(k) to compute hash, then O(1) lookup — also fast but different reasons) and linear scan (O(n·k) — must check every word).

**The key insight:** A hash map also gives O(k) lookup (for hashing the key), but a hash map cannot answer prefix queries. The trie gives O(k) lookup AND O(k + output) prefix queries — the hash map cannot do the second at all without scanning all keys.

---

### Q12: How does autocomplete work on a trie?

**Short answer:**
Traverse to the prefix node in O(k), then DFS from there collecting all complete words. Total cost is O(k + output size), independent of dictionary size.

**Step by step:**
```
Trie contains: apple, application, apply, banana

Query: autocomplete("app")

Step 1: Traverse to "app" node
  root → a → p → p  (3 steps, O(k=3))

Step 2: DFS from "app" node, collect all complete words below
  "app" → l → e → "apple" ✓
  "app" → l → i → c → a → t → i → o → n → "application" ✓
  "app" → l → y → "apply" ✓

Step 3: Sort lexicographically (for deterministic output)
  Result: [apple, application, apply]
```

**Output cap:** If prefix is "a", DFS could return 50,000 words. Cap at limit (default 20) by stopping DFS early once limit is reached.

---

### Q13: What's the memory trade-off between `unordered_map` children and `array[26]` children?

**Short answer:**
`array[26]` is faster (O(1) direct index) but wastes 26 pointers per node even for leaf nodes with no children. `unordered_map` only allocates for actual children — much less memory for sparse tries, but slower due to hashing.

**Concrete numbers:**
```
array[26] per node:
  26 pointers × 8 bytes = 208 bytes/node minimum
  For 185,264 nodes (LexiCore's trie): 185,264 × 208 = ~38 MB

unordered_map per node:
  Only allocates for actual children
  Average ~2-4 children for English words
  Much less total memory
```

**When to use `array[26]`:**
- Known fixed alphabet (lowercase English only)
- Memory is plentiful
- Query speed is critical (cache-friendly, no hash overhead)

**When to use `unordered_map`:**
- Mixed case, Unicode, or unknown alphabet
- Memory-constrained
- Sparse nodes (most nodes have few children)

**Production answer:** Compressed tries (radix trees / Patricia tries) combine the best of both — only one edge per branching point, O(k) lookup, minimal memory. LexiCore doesn't implement this but it's worth mentioning.

---

### Q14: When would a hash map be better than a trie?

**Short answer:**
For exact match queries only. Hash maps give O(1) average lookup with minimal memory overhead — if you never need prefix or fuzzy queries, a trie's extra memory and complexity isn't worth it.

**Decision table:**
```
Need exact match only?          → use hash map (unordered_set)
Need prefix/autocomplete?       → use trie
Need fuzzy/typo-tolerant?       → use BK-tree
Need all three?                 → use all three (like LexiCore does)
```

**Also prefer hash map when:**
- Dictionary is small (trie overhead not worth it)
- Memory is very tight
- You're doing batch membership queries (set intersection, union)

---

## Section D — BK-Trees

---

### Q15: What makes edit distance suitable as a BK-tree metric?

**Short answer:**
Edit distance satisfies the four metric axioms: non-negativity (`dist ≥ 0`), identity (`dist(a,a) = 0`), symmetry (`dist(a,b) = dist(b,a)`), and triangle inequality (`dist(a,c) ≤ dist(a,b) + dist(b,c)`). BK-trees only work correctly when the distance function is a true metric — the triangle inequality is what makes pruning valid.

**Why the triangle inequality is the critical one:**
It lets you reason: "If the current node is distance d from the query, and we're searching radius r, then any valid match must be within [d-r, d+r] of the current node." This is the pruning rule. Without the triangle inequality, this reasoning breaks down entirely.

---

### Q16: Explain triangle-inequality pruning step by step.

**Short answer:**
At each BK-tree node, compute true distance d to query. Only recurse into children whose edge label k satisfies `d-r ≤ k ≤ d+r`. Children outside this range cannot possibly contain any match within radius r of the query.

**Step-by-step proof:**
```
Let:
  w = current node's word
  q = query
  d = editDistance(q, w)
  r = maxDistance (search radius)
  x = some word in child subtree with edge label k (meaning editDistance(w, x) = k)

We want to know: can x be a match? i.e., can dist(q, x) ≤ r?

From triangle inequality:
  dist(q, x) ≤ dist(q, w) + dist(w, x) = d + k
  dist(q, x) ≥ dist(q, w) - dist(w, x) = d - k  (rearranged: dist(w,x) ≥ dist(q,w) - dist(q,x))

So: d - k ≤ dist(q, x) ≤ d + k

For dist(q, x) ≤ r to be possible:
  d - k ≤ r  →  k ≥ d - r
  (no useful upper constraint from this direction alone)

From the lower bound: dist(q, x) ≥ |d - k|
For dist(q, x) ≤ r:
  |d - k| ≤ r
  d - r ≤ k ≤ d + r  ✓
```

**Concrete example:**
```
Query "booc", radius r=1
At node "book": d = editDistance("booc", "book") = 1

Children to check: [d-r, d+r] = [0, 2]
  Child with edge=1? Yes → recurse (might contain match)
  Child with edge=3? Skip (|1-3| = 2 > 1 → no match possible in this subtree)
  Child with edge=5? Skip (|1-5| = 4 > 1)
```

---

### Q17: Why must you shuffle before inserting into a BK-tree?

**Short answer:**
If you insert words in alphabetical order, adjacent words have very similar edit distances from the root, creating a degenerate chain rather than a branching tree. A shuffled insertion order produces many distinct distances, creating more branches and enabling better pruning.

**Concrete example:**
```
Alphabetical insertion of: a, aa, aaa, aaaa, aaaaa ...
  root: "a"
    child[1]: "aa"
      child[1]: "aaa"
        child[1]: "aaaa"  ← chain, max depth = n, no pruning at all

Shuffled insertion:
  root: "cat"
    child[1]: "bat" (dist=1)
    child[2]: "apple" (dist=2)  ← branching, pruning works
    child[3]: "xyz" (dist=3)
```

**Practical impact in LexiCore:**
- Without shuffle: max depth could be 1000+ for 88K alphabetically-sorted words
- With shuffle (seed 42): max depth = **19**

**Reproducibility:** Use a fixed seed (`std::mt19937(42)`) so benchmarks are reproducible. Different seeds give different tree shapes, potentially different benchmark numbers — the fixed seed eliminates this variable.

---

### Q18: What can go wrong if you use a bounded edit distance for the pruning interval?

**Short answer:**
`editDistanceBounded(q, w, r)` returns `r+1` as a sentinel when the true distance exceeds `r`. If you use this sentinel as `d` to compute the pruning interval `[d-r, d+r]`, you get `[1, 2r+1]` instead of the correct `[trueD-r, trueD+r]` — this silently skips valid child subtrees.

**Worked example of the bug:**
```
True scenario:
  query = "abc", node = "xyz", maxDist = 2
  True editDistance("abc", "xyz") = 3
  Correct pruning interval: [3-2, 3+2] = [1, 5]
  → Recurse into children with edge labels 1, 2, 3, 4, 5

Buggy scenario (using bounded):
  editDistanceBounded("abc", "xyz", 2) = 3 (returns sentinel = maxDist+1 = 3)
  d = 3 (same in this case, but...)

  Different example:
  True editDistance("abcde", "xyz") = 5
  editDistanceBounded("abcde", "xyz", 2) = 3 (sentinel, true value is 5)
  
  If you use d=3 for pruning: interval = [3-2, 3+2] = [1, 5]
  Correct interval would be: [5-2, 5+2] = [3, 7]
  
  You INCLUDE edge labels 1,2 that shouldn't be visited
  (wasted work, but not wrong in terms of correctness here)
  
  Worse case: if true d=7, sentinel d=3:
  Correct interval: [5, 9]
  Buggy interval: [1, 5]  ← misses edge labels 6, 7, 8, 9 → drops valid matches!
```

**LexiCore's correct approach:**
```cpp
int d = editDistance(query, node.word);  // always true distance
if (d <= maxDistance) results.emplace_back(node.word, d); // threshold check
// Prune using true d:
for (auto& [k, child] : node.children) {
    if (k >= d - maxDistance && k <= d + maxDistance)
        searchImpl(child.get(), query, maxDistance, results);
}
```

---

### Q19: Is BK-tree O(log n)? What determines its practical performance?

**Short answer:**
No — BK-tree search has no guaranteed O(log n) bound. Practical performance depends on tree balance (insertion order), dictionary word distribution, search radius, and how clustered the dictionary words are in edit-distance space.

**Why no O(log n) guarantee:**
- BST gets O(log n) because it has one comparison per level and splits in two
- BK-tree can have many children per node (one per distinct edit distance)
- Pruning effectiveness varies — at large radius r, almost all children must be visited

**What determines performance:**
| Factor | Good performance | Poor performance |
|---|---|---|
| Insertion order | Shuffled (well-branched) | Alphabetical (chain-like) |
| Search radius r | Small (r=1,2) | Large (r=5+) |
| Word distribution | Diverse edit distances | Many words at same distance from root |
| Dictionary size | Large (more pruning gain) | Tiny (overhead not worth it) |

**The honest interview answer:**
"BK-tree delivers practical speedup — LexiCore measured 2.4× faster than linear scan at 88K words with radius 2. But it's not guaranteed O(log n), and for very large radius or pathological word distributions, it can approach linear performance. The benchmark is what tells you whether the trade-off was worth it."

---

## Section E — Benchmarking

---

### Q20: Why benchmark in Release mode?

**Short answer:**
Debug mode disables optimizations — loop unrolling, inlining, constant folding are all off. Benchmark numbers from Debug builds can be 10–100× worse than production performance and are meaningless for comparing algorithms.

**What `-O2` (Release) enables:**
- Function inlining: eliminates call overhead for small functions
- Loop unrolling: reduces branch overhead
- Dead code elimination: removes code that never executes
- Auto-vectorization: SIMD instructions for data-parallel operations

```bash
# LexiCore's release build:
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
# CMake automatically adds -O2 for Release mode
```

**The embarrassing mistake:** If you benchmark in Debug and report "BK-tree is 50× faster than linear scan" — someone will ask "which build?" and your credibility disappears.

---

### Q21: What's the difference between average latency and p99 latency? When does p99 matter more?

**Short answer:**
Average is the mean over all queries. p99 is the value below which 99% of queries fall — it captures tail latency that the average hides. p99 matters more when you care about worst-case user experience, not average.

**Concrete example from LexiCore results:**
```
Trie autocomplete at 88K words:
  avg = 4.39 µs
  p99 = 9.45 µs  ← 2× slower than average

BK-tree at 88K words:
  avg = 5729 µs
  p99 = 8797 µs  ← 1.5× slower than average
```

**When p99 > average:**
This means some queries are much slower than typical. For tries, these are likely broad prefixes ("a") that match thousands of words. For BK-tree, large radius queries that defeat pruning.

**When p99 matters most:**
- User-facing search (users notice slow responses, not averages)
- SLAs that specify "99% of requests under X ms"
- Systems where a slow query blocks others

**When average is fine:**
- Batch processing where total throughput matters
- Internal pipelines where tail latency doesn't affect user experience

---

### Q22: What is a warm-up run and why is it needed?

**Short answer:**
A warm-up run executes the workload once before timing starts, to populate CPU caches and trigger any one-time initialization. Without it, the first measured trial pays page-fault costs that all subsequent trials don't — making the first trial artificially slow.

**What happens without warm-up:**
```
Trial 1 (cold): data not in CPU cache → many cache misses → slow
Trial 2 (warm): data in CPU L1/L2 cache → fast
Trial 3 (warm): fast
...

If you include Trial 1 in your average, you're measuring cache-cold performance
that doesn't represent steady-state behavior.
```

**What the warm-up achieves:**
- Brings data into CPU cache (L1/L2/L3)
- Triggers OS page faults (physical memory allocation)
- JIT-compiles any lazy initialization

**LexiCore's approach:** Execute one full pass of queries before starting the timed 5-trial loop.

---

### Q23: Why report median instead of mean for benchmark trials?

**Short answer:**
The mean is distorted by outlier runs (OS scheduled another process in the middle, garbage collection, network interrupt). The median is robust to a single bad trial — if 4 of 5 trials are consistent and 1 is 10× slower due to OS noise, the median is unaffected.

**Example:**
```
5 trial times (ms): [10.1, 10.3, 10.2, 47.5, 10.4]  ← one bad trial

Mean = (10.1 + 10.3 + 10.2 + 47.5 + 10.4) / 5 = 17.7 ms  ← inflated by outlier
Median = 10.3 ms  ← stable, representative of steady-state
```

**Why 5 trials is a reasonable minimum:**
- Enough to identify one outlier
- Odd number → single middle value as median (no ambiguity)
- More trials = more time; 5 is the sweet spot for offline benchmarks

**Production benchmarking tools** (like Google Benchmark, criterion for Rust) use statistical methods to determine how many trials needed to reach a given confidence interval — but for a project benchmark, 5 trials median is defensible and honest.

---

### Q24: Why does the benchmark use separate query sets for exact, prefix, and fuzzy searches?

**Short answer:**
Each query type exercises different code paths and structures. Using the same query set across all three would mean either: prefix queries that happen to be complete words (biasing exact benchmarks), or fuzzy queries with exact-match hits (biasing fuzzy benchmarks). Separate generation ensures each strategy faces a representative workload.

**Query generation in LexiCore:**
```
Exact queries:     random words from dictionary (many will be found)
Prefix queries:    random prefixes of length 1-5 (exercises trie traversal depth)
Fuzzy queries:     random words with one random mutation applied
                   (guaranteed to produce typos, exercises edit distance)
```

**Why the same seed matters:**
Different structures get the same batch of queries within each query type (e.g., both linear scan and hash lookup see the same 1000 exact queries). This makes the comparison apples-to-apples — the only variable is the data structure, not the workload.

---

### Q25: What does a credible benchmark report include?

**Short answer:**
Hardware, compiler version, optimization flags, OS, methodology (trials, warm-up, query generation), and the actual numbers — never a claim without the backing data.

**Template (from RESULTS.md):**
```
CPU: x86_64 (specific model)
OS: Ubuntu 24.04
Compiler: g++ 13.3.0
Build: Release (-O2)
Dictionary: 88,344 unique words
Queries: 1,000 per trial
Trials: 5, median reported
Warm-up: 1 full pass before timing
```

**What kills credibility:**
- "BK-tree is 50× faster" without specifying dictionary size, radius, hardware
- Numbers from Debug build
- Single-trial measurements
- Different query sets per structure

---

## Summary: The 5 Things That Impress Interviewers Most

1. **BK-tree pruning** — explain triangle inequality from first principles, not just "it prunes children"
2. **The sentinel trap** — "I specifically used true editDistance() for pruning because the bounded variant returns a sentinel that would corrupt the pruning interval" — this shows you thought deeply
3. **Shuffle rationale** — unprompted: "I shuffled the dictionary before BK-tree insertion with a fixed seed to prevent alphabetical correlation from creating degenerate chains"
4. **Benchmark honesty** — "We measured 2.4× speedup at 88K words, Release build, median-of-5 trials. The BK-tree doesn't guarantee O(log n) — this is empirically measured"
5. **Memory ownership** — "unique_ptr on all nodes, verified zero leaks under AddressSanitizer across all test cases including the 500-word × 200-query × 4-threshold correctness oracle"
