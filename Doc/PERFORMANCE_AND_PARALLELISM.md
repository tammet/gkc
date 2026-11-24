# GKC Performance, Memory Arenas, and Parallelism

This document covers the runtime architecture invariants that determine
correctness and throughput in gkc: process-based parallelism, arena and clause
lifetimes, hot-path efficiency rules, and profiling workflows. It complements
`Doc/DATA_REPRESENTATION.md`, `Doc/TERM_MATCHING.md`, and
`Doc/SHARED_MEMORY.md`.

Primary source files:

* `Reasoner/rmain.c`: `fork()` worker pool, round-robin strategy dispatch,
  `waitpid()` termination, and `clock()`/`alarm()` time budgets.
* `Reasoner/glb.h` and `Reasoner/glb.c`: per-run arena buffers, `alloc_err`,
  and local vs. shared-KB allocation/cleanup hooks.
* `Reasoner/mem.h` and `Reasoner/mem.c`: `wr_alloc_from_cvec()`, `wr_malloc()`,
  and `vec`/`cvec` resizing.
* `Reasoner/build.c` and `Reasoner/derive.c`: term construction into scratch
  and bump arenas.
* `Reasoner/subsume.c`, `Reasoner/rgenloop.c`, and `Reasoner/history.c`:
  logical clause deletion and proof-history retention.

## 1. Process-Based Parallelism (`fork()`, Not Threads)

On Unix systems, gkc parallelizes strategy portfolios using operating-system
**processes** created with `fork()` in `wg_run_reasoner()` (`Reasoner/rmain.c`).
There are no threads, mutexes, or lock-free shared search structures.

Key mechanics:

1. **Platform gating:** Parallel workers are enabled on Unix and disabled on
   `_WIN32` and `__EMSCRIPTEN__` (where `maxforks = 0`).
2. **Worker cap:** `maxforks` comes from `dbmemsegh(local_db)->max_forks`
   (configured by `-parallel`). Values above 64 are clamped to 64 (`forkpids[64]`),
   and `maxforks == 1` collapses to `0` (runs directly in the single process
   without calling `fork()`).
3. **Copy-on-Write boundary:** Forking occurs **after** parsing, clausification,
   `wr_analyze_clause_list()`, and `make_auto_guide()` / guide parsing. All
   children inherit the parsed clause list, URI table, `analyze_g` statistics,
   and `cJSON *guide` via OS Copy-on-Write without re-parsing.
4. **Static round-robin dispatch:** Each worker runs the strategy loop
   `for (iter = 0; ; iter++)` and executes strategy `iter` iff:
   ```c
   if (!pid && maxforks && ((iter % maxforks) != forknr)) continue;
   ```
   The parent process does not execute strategies once children exist (`if (pid && forkscreated) break;`).
5. **Zero shared mutable search state:** Workers do not share derived clauses,
   queues, active indexes, varbanks, or hash tables. Each worker creates and
   frees its own `glb` per strategy iteration.
6. **First-proof-wins termination:** The parent loops in `waitpid(-1, &stat, 0)`.
   As soon as any child exits with status `0` (proof found), the parent sends
   `kill(forkpids[i], SIGKILL)` to every other live child, frees `analyze_g`,
   and calls `exit(0)`.
7. **Orphan prevention (Linux):** Immediately after `fork()`, each child calls
   `prctl(PR_SET_PDEATHSIG, SIGTERM)` and checks `if (getppid() == 1) exit(1);`
   so workers do not outlive a terminated parent.
8. **Output buffering:** `setbuf(stdout, 0)` is called before forking so child
   proof/trace lines are not duplicated by inherited stdio buffers.
9. **CPU vs. wall-clock timers:**
   * Per-run `max_seconds` / `max_dseconds` and `allruns_start_clock` checks in
     `rmain.c` / `rgenloop.c` use `clock()` (per-worker CPU time).
   * Global CLI `-seconds` and `LTBSPECIAL` timeouts install Unix `alarm()`
     (wall-clock time).

### Fork-Safety Rules for Developers

* **No shared mutable files:** Any temporary file created during search (for
  example, external SAT solver I/O) must incorporate `getpid()` or `forknr` in
  its filename so parallel workers never overwrite each other's files.
* **Assume sudden `SIGKILL`:** Any sibling worker may be terminated by
  `SIGKILL` at any instruction when another worker finds a proof. Never hold
  persistent locks, never mutate a shared `-usekb` segment, and do not rely on
  process-exit cleanup hooks to restore external state.
* **Debug with `-parallel 0`:** Because `stdout` is unbuffered across competing
  processes, debug logs and even the winning proof can vary under parallel
  runs. Always reproduce and bisect with `-parallel 0`.

## 2. Memory Arenas, Term Sharing, and Clause Lifetimes

### Ephemeral Scratch Buffers vs. Append-Only Bump Arenas

Search-time clauses and terms are not allocated individually with `malloc()` or
`wg_create_raw_record()`. Instead, `glb` owns fixed-capacity `cvec` arenas
allocated once per strategy run and carved linearly by `wr_alloc_from_cvec()`
(`Reasoner/mem.c`):

| Buffer | Lifetime class | Typical usage |
| --- | --- | --- |
| `given_termbuf` | Ephemeral scratch (reset per given clause) | Rebuilding and renaming the selected given clause into Bank 1. |
| `derived_termbuf` | Ephemeral scratch (reset per derivation) | Half-built resolvents, factors, and paramodulants in Bank 3. |
| `simplified_termbuf` | Ephemeral scratch (reset per simplification) | Intermediate rewritten/simplified terms and clauses. |
| `queue_termbuf` | Append-only bump arena (default 200M `gint`s) | Kept passive clauses copied into Bank 0. |
| `active_termbuf` | Append-only bump arena (default 100M `gint`s) | Activated clauses renamed into Bank 2 and indexed for search. |
| `hyper_termbuf` | Append-only bump arena (default 100M `gint`s) | Partial hyperresolution clauses. |

Critical arena invariants:

1. **No individual free in bump arenas:** Objects in `queue_termbuf`,
   `active_termbuf`, and `hyper_termbuf` are never individually freed. An
   immediate rollback (`CVEC_NEXT(buf) = saved_next`) is valid **only** if no
   later allocation has occurred from that buffer and no live pointer/index
   references the rolled-back region.
2. **Non-growing `wr_alloc_from_cvec()`:** Unlike `wr_cvec_push()`,
   `wr_alloc_from_cvec()` never reallocates or moves the buffer (moving it
   would invalidate every existing `gptr` and WhiteDB offset pointing inside
   it). On overflow, it sets `g->alloc_err` and returns `NULL` (or `WG_ILLEGAL`
   from callers). Every inner build loop must check `g->alloc_err` and unwind
   cleanly rather than dereferencing `NULL`.
3. **No global term sharing (copied trees):** Compound terms (`isdatarec`) are
   copied as fresh trees by `wr_build_calc_term()`. GKC does not hash-cons
   compound terms globally. Consequently:
   * `x == y` on two encoded data-record `gint`s is sufficient for equality,
     but `x != y` does **not** prove inequality — use `WR_EQUAL_TERM` or
     `wr_equal_term()`.
   * Interned URIs/symbols (with `uniquestrflag == 1`) and immediate scalars
     (smallints, variables, `ACONST_*`) *do* compare equal and unequal by direct
     `gint` identity.

### Clause-Deletion Lifetime Rule (Proof-History Retention)

When an active or passive clause is back-subsumed, backward-demodulated, or
blocked by unit propagation:

* **Do NOT free, reuse, or overwrite the clause record or its history record.**
  Later derived clauses and proof printing follow `HISTORY_PARENT1_POS` /
  `HISTORY_PARENT2_POS` / `HISTORY_PARA_PARENT*_POS` pointers back to deleted
  ancestors.
* **Logical deletion + index unlinking only:** Mark the clause deleted (for
  example via `wr_set_cl_backsubsumed()` / `backsubsume_values[clid]` or
  `RECORD_META_BLOCKED_CLAUSE`), and remove or unlink it from active search
  indexes (hash buckets, discrimination trees, subsumption lists) so future
  inferences do not waste time scanning dead clauses.

### Process-Heap vs. WhiteDB Storage Boundary

Choose the allocator based on whether the structure must live inside a shared
KB (`g->inkb != 0`):

* **Run-local structures (`g->inkb == 0`):** Indexes, symbol tables, ordering
  caches, and auxiliary trees that exist only for a single search run may use
  process-heap allocation (`sys_malloc` / `wr_malloc` / typed node pools) and
  plain C pointers. Always free them in `wr_glb_free_local_complex()`
  (`Reasoner/glb.c`) so multi-strategy runs do not leak memory across
  iterations.
* **Shared-KB structures (`g->inkb != 0`):** Anything persisted during `-readkb`
  and read by other processes under `-usekb` must live inside WhiteDB memory
  and store `db`-relative offsets (`rpto`/`rotp`), never raw C pointers. See
  `Doc/SHARED_MEMORY.md`.

## 3. Hot-Path Efficiency Rules

GKC's inner inference loops (subsumption, matching, unification, paramodulation
candidate filtering, demodulation, term ordering, and clause weighting) run
millions of times per second. Small constant-factor regressions compound quickly
across a strategy portfolio.

Follow these rules in hot paths:

1. **Zero `malloc()` / `free()` in inner loops:** Preallocate scratch stacks,
   variable-count arrays, candidate buffers, and node pools in `glb` (or use
   fixed-size stack arrays with safe bounds) instead of heap-allocating per
   clause, per literal, or per term comparison.
2. **$O(\text{touched})$ rollback, never full-array `memset`:** Variable trails
   (`varstack` + `wr_clear_varstack_topslice`), variable-occurrence counters,
   and temporary marker arrays must clear only the entries touched during the
   probe. Never zero a 5,000-slot `varbanks` array per candidate.
3. **Compare interned `gint` encodings or dense symbol IDs, never strings:**
   Avoid calling `wg_decode_uri()` + `strcmp()` inside matching, ordering,
   hashing, or weighting loops. With `uniquestrflag == 1`, functor/predicate
   URIs are already unique `gint` values; for precedence tables, weights, or
   indexing arrays, map each symbol `gint` once to a dense integer ID (`0..N-1`)
   and index flat arrays directly.
4. **Avoid redundant full-term walks:** Helpers like `wr_term_has_vars()`,
   `wr_calc_term_weight()`, `wr_count_cl_nonans_atoms()`, and
   `wr_order_eqterms()` walk entire subtrees. Compute or cache cheap summaries
   (weight, depth, variable presence, precomputed orientation) once at clause
   creation or activation rather than re-traversing the same term inside an
   $O(N^2)$ candidate scan.
5. **Prefer fast inline macros in tight loops:** Use `VARVAL_F` instead of
   `VARVAL` when dereferencing variables, `WR_EQUAL_TERM` (with local
   `eqencx`) for syntactic equality checks, and `litmeta` / `clmetablock`
   bitmasks before invoking full matching or unification.
6. **Keep index scans clean of dead clauses:** Checking `wr_get_cl_backsubsumed`
   lazily during scans still pays traversal cost on every query. Prefer eager
   removal or compaction in any new index structure (while preserving the
   underlying clause and history records in their arenas).
7. **Respect `g->alloc_err` and depth limits:** Recursive term functions must
   either bound recursion depth or use an explicit stack on deeply nested
   terms (for example, long arithmetic or Skolem chains) to avoid stack
   overflows.

## 4. Profiling and Benchmarking Workflow

1. **Deterministic single-process profiling:** Always pass `-parallel 0` when
   profiling or comparing inference counts, so worker scheduling does not skew
   measurements:
   ```sh
   ./gkc problem.p -parallel 0 -print 15 -seconds 10
   ```
   If isolating a single heuristic or index change, also pass a single-run
   `-strategy` file so portfolio switches do not confound the timing.
2. **Reading `-print 15` statistics:** Compare:
   * `given_candidates` vs. `given_used` (measures forward-subsumption /
     simplification effectiveness);
   * `derived`, `kept`, and `discarded` clause counts (measures pruning and
     weight/size limits);
   * `subsumed` and `backsubsumed` counts;
   * wall/CPU time to proof or given-clauses-per-second throughput.
3. **CPU profiling on Linux (`perf`):** Build with debug symbols (`-g` alongside
   `-O3`) and record a single-process run:
   ```sh
   perf record -g ./gkc problem.p -parallel 0 -seconds 15
   perf report --stdio --no-children | head -n 40
   ```
   Look for hotspots in `wr_subsume_cl_aux`, `wr_unify_term_aux`,
   `wr_match_term_aux`, `wr_order_eqterms`, `wr_build_calc_term`, and hash
   bucket scans in `resolve.c` / `rewrite.c`.
4. **Portfolio-level validation:** After micro-benchmarking single runs with
   `-parallel 0`, test under default parallel mode (`-parallel 4` or default)
   across representative FOF, UEQ, and arithmetic problems to confirm both
   correctness and real wall-clock gains.
