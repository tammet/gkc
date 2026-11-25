# GKC Search Loop and Derived-Clause Pipeline

This document traces the current runtime search pipeline in gkc: how
`wr_genloop()` selects, simplifies, subsumes, activates, and infers from a
given clause, and how newly derived clauses are built, filtered, indexed,
propagated, and queued. It complements `Doc/ARCHITECTURE.md` (high-level map),
`Doc/DATA_REPRESENTATION.md` (record/buffer/queue layouts),
`Doc/TERM_MATCHING.md` (varbanks and varstack), and
`Doc/STRATEGIES_AND_HEURISTICS_DEV.md` (strategy flags and portfolios).

Primary source files:

* `Reasoner/rgenloop.c`: main given-clause loop (`wr_genloop`), given rebuild
  (`wr_process_given_cl`), and active insertion (`wr_add_given_cl_active_list`,
  `wr_add_cl_to_active_unithash`).
* `Reasoner/clstore.c` and `Reasoner/clstore.h`: passive multi-queue storage
  and selection (`wr_push_cl_clpick_queues`, `wr_pick_from_clpick_queues`,
  `wr_pick_from_hyper_queue`, `wr_push_priorqueue`, `wr_pick_priorqueue`).
* `Reasoner/simp.c`: given-clause simplification (`wr_simplify_cl`) and
  2-literal double-cut simplification (`wr_simplify_doublecut_cl`).
* `Reasoner/subsume.c`: forward/backward active subsumption
  (`wr_given_cl_subsumed`, `wr_given_cl_backsubsume`), derived unit
  cut/subsumption (`wr_derived_cl_cut_and_subsume`), and double-cut helpers
  (`wr_atom_doublecut`, `wr_atom_doublecut_aux`).
* `Reasoner/resolve.c`: factoring, equality-reflexive resolution, binary
  resolution, and paramodulation candidate loops.
* `Reasoner/derive.c`: derived-clause post-processing
  (`wr_process_resolve_result`, `wr_process_paramodulate_result`,
  `wr_process_resolve_result_aux`, `wr_derived_weight_check`,
  `wr_add_cl_to_unithash`, `wr_add_cl_to_doublehash`).
* `Reasoner/propagate.c`: eager unit propagation (`wr_propagate_unit`,
  `wr_store_atom_occurrences`).

---

## 1. End-to-End Pipeline Overview

Every strategy run executes `wr_genloop(g)` (`Reasoner/rgenloop.c`), cycling
clauses through fixed arenas and variable banks:

```text
Passive queues (queue_termbuf, Bank 0)
  │  1. Time / passed_ratio / endgame checks
  │  2. Pick candidate: hyper_queue -> clpick_queues (4 role blocks x [priorqueue, simplequeue])
  │  3. Cheap filters: arglen/endgame, tautology, backsubsumed/blocked
  │  4. Given simplification: wr_sort_cl + wr_simplify_cl (rewrite/calc, unit cut/subs, doublecut)
  │  5. Meta & forward active subsumption: wr_calc_clause_meta, wr_given_cl_subsumed
  │  6. Optional bounded arithmetic instantiation: wr_generate_arith_instances
  │  7. Rebuild given into given_termbuf (Bank 1): wr_process_given_cl
  │  8. Optional backward active subsumption: wr_given_cl_backsubsume
  │  9. Literal selection: wr_calc_clause_resolvability
  │ 10. Activate into active_termbuf (Bank 2): wr_add_given_cl_active_list
  │ 11. Generating inferences (Bank 1 given x Bank 2 active):
  │       wr_factor -> wr_resolve_equality_reflexive -> wr_resolve_binary_all_active
  │       -> wr_paramodulate_from_all_active -> wr_paramodulate_into_all_active
  ▼
Derived-clause pipeline (wr_process_resolve_result / wr_process_paramodulate_result)
  │  A. Build half-built substituted clause in derived_termbuf (Bank 3)
  │  B. Unit cut/subsumption (hash_*_groundunits) + strong cut + duplicate removal
  │  C. Copy final clause to queue_termbuf (or hyper_termbuf) + build history
  │  D. Pure-answer check + [resolution only] wr_simplify_doublecut_cl
  │  E. Weight & dynamic resource/time cutoff (wr_calc_clause_weight + wr_derived_weight_check)
  │  F. Passive indexing & propagation (wr_calc_clause_meta, unithash, doublehash, propagate)
  │  G. Push into clpick_queues (both simplequeue and priorqueue)
  ▼
Back to Passive queues
```

---

## 2. The Given-Clause Loop (`Reasoner/rgenloop.c`)

Before entering the main `for (;;)` loop, `wr_genloop()` records
`g->run_start_clock = clock()`, clears all variable banks with
`wr_clear_all_varbanks(g)`, resets `g->proof_found` and `g->stat_given_*`, and
zeroes `g->backsubsume_values` if `g->back_subsume` is enabled.

### Step 2.1: Time Budgets and `passed_ratio` Phase Transitions

At the top of each iteration (`rgenloop.c`), gkc computes `run_seconds`,
`run_dseconds`, `total_seconds`, and `g->passed_ratio` (`elapsed / limit`):

* Returns `2` (timeout) if `max_run_seconds`, `max_run_dseconds`, or
  `max_seconds` is exceeded.
* In `instgen_strat`, returns `1` when `queue_termbuf` has fewer than
  1,000,000 `gint`s of free space remaining.
* Automatically narrows search as `g->passed_ratio` crosses fixed thresholds:
  * `> 0.50`: sets `g->pick_given_queue_ratio = 100` (strong weight preference).
  * `> 0.70`: sets `g->res_shortarglen_limit = 2`.
  * `> 0.85`: sets `g->res_shortarglen_limit = 1`.
  * `> 0.90`: sets `g->endgame_mode = 1` and `g->res_arglen_limit = 1` (unit
    clauses only; disables paramodulation, forward/backward active subsumption,
    and given rewriting).
* Checks `if (g->alloc_err) return -1;` before picking.

### Step 2.2: Clause Selection (`hyper_queue` and `clpick_queues`)

Candidate selection happens in two layers:

1. **Hyper queue first:** `wr_pick_from_hyper_queue(g, g->hyper_queue, given_cl_metablock)`
   drains partial hyperresolution clauses whenever `hyper_queue` is non-empty,
   setting `given_from_hyperqueue_flag = 1`.
2. **Multi-queue passive selection:** Otherwise,
   `wr_pick_from_clpick_queues(g, rotp(g, g->clpick_queues), given_cl_metablock)`
   selects from `g->clpick_queues` (`NROF_CLPICK_QUEUES == 4` role blocks in
   `Reasoner/clstore.c`):
   * Block 0 (`GOAL_ASSUMPTIONS_QUEUE_POS`): goal + assumptions combined.
   * Block 1 (`GOAL_QUEUE_POS`): goal / from-goal clauses.
   * Block 2 (`ASSUMPTIONS_QUEUE_POS`): assumptions / from-assumption clauses.
   * Block 3 (`AXIOMS_QUEUE_POS`): axioms / background clauses (used as the sole
     queue when `cl_pick_queue_strategy == 0` or `4`).

Inside `wr_pick_from_clpick_queues()`:

* Cycles across role blocks using `g->next_pick_given_queue_block_nr` and each
  block's `CLPICK_THIS_QUEUE_RATIO` / `CLPICK_THIS_QUEUE_RATIO_COUNTER`.
* Inside a block, interleaves the bucketed weight priority queue
  (`CLPICK_PRIORQUEUE_POS`, picked via `wr_pick_priorqueue()` in ascending
  weight order `0 .. MAX_USED_PRIOR`) with the FIFO age queue
  (`CLPICK_QUEUE_POS`, indexed by `CLPICK_QUEUE_GIVEN_POS`), governed by
  `CLPICK_PRIORQUEUE_RATIO` / `CLPICK_PRIORQUEUE_RATIO_COUNTER`.
* Because every kept clause is pushed into **both** the FIFO `simplequeue` and
  the weight `priorqueue` of its block, picked clauses are marked via
  `wr_cl_mark_given()` so the twin sub-queue skips already-picked clauses
  (`wr_cl_ismarked_given()`).
* If no candidate is found on the first iteration, returns `0` if answers exist
  or `2`; if Passive empties later, returns `1` (saturated without proof).

### Step 2.3: Pre-Simplification Filters

Before touching term builders, `wr_genloop()` discards `picked_given_cl_cand`
immediately if:

* `g->res_arglen_limit` is set and a rule clause has more non-answer literals
  than `res_arglen_limit`;
* `g->endgame_mode` is active and `wr_count_cl_nonans_atoms() > 1`;
* `wr_tautology_cl()` succeeds;
* `g->back_subsume && !g->endgame_mode && wr_get_cl_backsubsumed()` is true;
* `g->propagate && wg_rec_is_blocked_clause()` is true.

### Step 2.4: Given-Clause Simplification (`Reasoner/simp.c`)

1. Sets up temporary substitution state in `given_termbuf` (Bank 1) via
   `wr_process_given_cl_setupsubst(g, g->given_termbuf, 1, 1)`.
2. Sorts literals in place via `wr_sort_cl(g, picked_given_cl_cand)` **only if**
   `!wg_in_attached_kb(picked_given_cl_cand)`.
3. Calls `given_cl_cand = wr_simplify_cl(g, picked_given_cl_cand, given_cl_metablock)`,
   then restores varstack via `wr_process_given_cl_cleanupsubst(g)`:
   * **Fact clause branch (`!wg_rec_is_rule_clause`):** If rewriting or
     computable functions apply, runs `wr_build_calc_term()` into
     `queue_termbuf`. Returns `NULL` on tautology (`ACONST_TRUE`), sets
     `g->proof_found = 1` on `ACONST_FALSE`, or returns a new fact clause with
     simplify history if rewritten.
   * **Rule clause branch:** Allocates a temporary literal array in
     `derived_termbuf` and iterates through each literal:
     1. Rewrites / evaluates computable terms via `wr_build_calc_term()` when
        `use_rewrite_terms_strat && have_rewrite_terms` or `use_comp_funs`.
     2. Probes `hash_{pos,neg}_groundunits` (plus `r_kb_g(g)->hash_*_active_groundunits`)
        via `wr_atom_cut_and_subsume()`. **Important detail:** for an
        unrewritten unit clause (`len < 2` and no rewrites occurred), passes
        `subsflag = 0` so a passive unit cannot subsume itself against its own
        entry in `hash_*_groundunits`.
     3. For `len > 1` (if no unit cut/subsumption occurred and no doublecut was
        yet found), tries `wr_atom_doublecut()` against
        `hash_{pos,neg}_grounddoubles`.
     4. If any cuts or rewrites occurred, builds a new simplified clause in
        `queue_termbuf` with `wr_build_simplify_history()`; if all literals were
        cut (`rpos == 0`), sets `g->proof_found = 1`.
4. After `wr_simplify_cl()`:
   * Re-checks `wr_tautology_cl()`.
   * Computes `wr_calc_clause_meta(g, given_cl_cand, given_cl_metablock)`
     (guarded by `!wg_in_attached_kb(given_cl_cand)`).
   * Checks `ACONST_TRUE` / `ACONST_FALSE` and `wr_prop_truefalsity_clause()`
     (which can trigger proof found on propositional falsity).

### Step 2.5: Forward Active Subsumption and Arithmetic Instantiation

1. **Forward active subsumption (`wr_given_cl_subsumed()` in `Reasoner/subsume.c`,
   skipped in `endgame_mode`):**
   * First probes `hash_{pos,neg}_active_groundunits` (local and shared KB) for
     ground-unit subsumption.
   * Then linearly scans `g->clactivesubsume` (and `r_kb_g(g)->clactivesubsume`
     under `-usekb`) in 6-`gint` `CLMETABLOCK` blocks, prefiltering with
     `wr_clmetablock_can_subsume()` before calling `wr_subsume_cl()` (one-way
     matching with backtracking).
   * If subsumed, discards `given_cl_cand` and starts the next loop iteration.
2. **Bounded numeric instantiation (`Reasoner/arithinst.c`):** If enabled and
   within budget (`stat_arithinst_probes < arith_inst_global_limit`), calls
   `wr_generate_arith_instances(g, given_cl_cand)` before activation.

### Step 2.6: Rebuilding to Bank 1, Backward Subsumption, and Activation to Bank 2

1. **Rebuild given clause into Bank 1:** `wr_process_given_cl(g, given_cl_cand, g->given_termbuf)`
   renames variables into **Bank 1** inside `g->given_termbuf` (with
   `build_rewrite = 0` in `endgame_mode`), stamps given order on the history
   record (`wr_set_history_record_given_order`), and cleans varstack.
2. **Backward active subsumption (optional):** When `!endgame_mode && g->back_subsume`,
   `wr_given_cl_backsubsume(g, given_cl, given_cl_metablock)` scans
   `clactivesubsume` and marks subsumed active clauses via
   `wr_set_cl_backsubsumed()` (`backsubsume_values[clid] = 1`). Clause records
   and histories are **not** freed; active index scans lazily skip marked
   clauses.
3. **Literal selection:** `wr_calc_clause_resolvability(g, given_cl, 0, hyperpartial)`
   populates `g->tmp_resolvability_vec` marking which literals are eligible for
   resolution and paramodulation.
4. **Activation into Bank 2 (`wr_add_given_cl_active_list()`):**
   * If `given_from_hyperqueue_flag == 1`, only renames `given_cl` into
     `active_termbuf` (**Bank 2**) without inserting into active search indexes
     (partial hyperresolution clauses are not complete active clauses).
   * Otherwise, `wr_add_given_cl_active_list()`:
     1. Rebuilds `given_cl` into `active_termbuf` (**Bank 2**) as `active_cl`.
     2. Appends raw `(gint)active_cl` to `g->clactive`.
     3. Calls `wr_add_cl_to_active_unithash(g, active_cl)`: inserts **ground**
        unit clauses into `hash_{pos,neg}_active_groundunits` and returns `1`.
        Non-ground units and all multi-literal clauses return `0` and (when
        `subsflag == 1`) have their 5-`gint` `given_cl_metablock` plus
        `rpto(g, active_cl)` pushed into `g->clactivesubsume`.
     4. Indexes eligible resolution literals via `wr_cl_store_res_terms()`
        (3-level ground-prefix hashing into `hash_pos_atoms` / `hash_neg_atoms`).
     5. When `!endgame_mode && use_equality && use_equality_strat`, indexes
        paramodulation and rewrite terms via `wr_cl_store_para_terms()`
        (`hash_para_terms`, `hash_eq_terms`, `hash_rewrite_terms`).
     6. Increments `g->stat_given_used`.
   * If simplification shortened the clause to length 2 (`simplen != origlen && simplen == 2`),
     also inserts `given_cl` into `hash_{pos,neg}_grounddoubles` via
     `wr_add_cl_to_doublehash()`.

### Step 2.7: Generating Inferences on `given_cl`

All generating inferences unify `given_cl` (**Bank 1**) against Active clauses
(**Bank 2**) in fixed order, checking `proof_found` and `alloc_err` after each:

1. **`wr_factor(g, given_cl, given_cl_as_active)`** — factoring (skipped for
   non-satellite clauses under strict `hyperres_strat`).
2. **`wr_resolve_equality_reflexive(g, given_cl, given_cl_as_active)`** —
   resolving negative equalities against `X = X` (when `g->use_equality`).
3. **`wr_resolve_binary_all_active(g, given_cl, given_cl_as_active, resolvability)`**
   — binary resolution via 3-level ground-prefix hash probes in `hash_pos_atoms`
   / `hash_neg_atoms` (local and shared KB); each unifying pair invokes
   `wr_process_resolve_result()`, then clears `g->varstack`.
4. **`wr_paramodulate_from_all_active()`** followed by
   **`wr_paramodulate_into_all_active()`** — run when
   `use_equality && !rewrite_only_strat && !endgame_mode` (and not blocked by
   `prohibit_nested_para` on a paramodulation-derived given clause). Each
   ordering-allowed unifying pair invokes `wr_process_paramodulate_result()`,
   then clears `g->varstack`.

---

## 3. The Derived-Clause Pipeline (`Reasoner/derive.c`)

When an inference unifies a pair of literals/terms, the caller leaves the mgu
active in `g->varbanks` and calls `wr_process_resolve_result()` (binary
resolution), `wr_process_paramodulate_result()` (paramodulation), or
`wr_process_factor_result()` (factoring). The caller clears `g->varstack` after
the function returns.

All three entry points follow the same four-phase structure:

### Phase A: Half-Built Clause in `derived_termbuf` (Bank 3)

1. Increments `stat_derived_cl` (and `stat_binres_derived_cl` /
   `stat_para_derived_cl`). If both parents are units being resolved
   (`rlen == 0`), immediately sets `g->proof_found = 1`, builds the proof
   history, registers the answer, and returns.
2. Resets `(g->derived_termbuf)[1] = 2` and reserves `rlen` `gint`s in
   `derived_termbuf` for a flat `(meta, atom)` array `rptr`.
3. Calls `wr_process_resolve_result_setupsubst(g)`: enables `build_subst = 1`,
   `build_rename = 1`, `build_rename_banknr = 3` (**Bank 3**),
   `build_buffer = g->derived_termbuf`, and `use_comp_funs = use_comp_funs_strat`.
4. Calls `wr_process_resolve_result_aux()` on each parent clause:
   * Skips `cutatom` (for resolution) or replaces the target subterm at
     `replpath` via `wr_build_calc_term_replace()` (for paramodulation); all
     other literals are built via `wr_build_calc_term()`.
   * Evaluates computable predicates: `ACONST_TRUE` drops a negative literal or
     discards the clause if positive; `ACONST_FALSE` drops a positive literal
     or discards the clause if negative.
   * Scans earlier entries `0 .. *rpos - 1` of `rptr` with `wr_equal_term()`:
     drops same-polarity syntactic duplicates and discards the entire clause as
     a tautology (`stat_tautologies_discarded`) on opposite-polarity matches.
5. Calls `wr_process_resolve_result_cleanupsubst(g)` to clear Bank 3 slots. If
   `rpos == 0`, records `g->proof_found = 1` and returns.

### Phase B: Pre-Commit Unit Cut, Subsumption, and Duplicate Removal

Still operating on the half-built array `rptr` (before touching `queue_termbuf`):

1. **`wr_derived_cl_cut_and_subsume(g, rptr, rpos, NULL)` (`Reasoner/subsume.c`):**
   * For each literal `i = 0 .. rpos - 1`, computes `hash = wr_lit_hash(g, xatom)`
     and probes `hash_{pos,neg}_groundunits` (local and `r_kb_g(g)`):
     * **Same polarity match** (when `forward_subsume_derived` is on): returns
       `-1` immediately — the derived clause is subsumed and discarded.
     * **Opposite polarity match:** marks `rptr[i].meta = 0`, appends the
       cutting unit clause to `g->cut_clvec`, and increments `cuts`.
2. **Optional strong unit cut:** If `g->use_strong_unit_cutoff` is enabled,
   `wr_derived_cl_strong_cut()` attempts non-ground matching against
   `hash_{pos,neg}_units`.
3. If any cuts occurred, `wr_process_resolve_result_remove_cuts()` compacts
   `rptr`; if `rpos == 0`, sets `g->proof_found = 1` (including `g->cut_clvec`
   in the proof history) and returns.
4. If `rpos > 1 && g->use_strong_duplicates`, calls
   `wr_find_strong_duplicates()` to drop subsumed duplicate literals.

### Phase C: Final Clause Copy Into `queue_termbuf` (or `hyper_termbuf`)

1. `wr_process_resolve_result_isrulecl(g, rptr, rpos)` determines whether the
   result can be stored as a compact fact clause (positive ground unit) or must
   be a rule clause.
2. Chooses the target arena:
   * Partial hyperresolution clauses (`hyperres_strat && !wr_hyperres_satellite_tmpres`)
     use `wr_process_resolve_result_setuphypercopy(g)` (`g->hyper_termbuf`).
   * All normal clauses use `wr_process_resolve_result_setupquecopy(g)`
     (`g->queue_termbuf`, `build_subst = 0`, `build_rename = 0`,
     `use_comp_funs = 0`).
   * Snapshots `initial_queue_termbuf_next = CVEC_NEXT(g->build_buffer)` so the
     allocation can be rolled back if the clause fails weight checks below.
3. Builds the history record (`wr_build_resolve_history` /
   `wr_build_para_history`) and constructs the final clause record `res` via
   `wr_derived_build_cl_from_initial_cl(g, rptr, rpos, ruleflag, history)`.
4. **Pure answer check:** `wr_cl_derived_is_answer(g, res)` checks for
   answer-only clauses; if true, calls `wr_register_answer()`, sets
   `g->proof_found = 1`, and returns.
5. **Double-cut simplification (`wr_process_resolve_result` only):**
   Calls `wr_simplify_doublecut_cl(g, res, cl_metablock2)` (`Reasoner/simp.c`),
   which probes `hash_{pos,neg}_grounddoubles` via `wr_atom_doublecut()` /
   `wr_atom_doublecut_aux()` for every literal of `res` (for rule clauses with
   `len >= 2`). If a 2-literal clause cuts a literal, builds a replacement
   clause with simplify history (or reports proof if empty). Note that
   `wr_process_paramodulate_result()` does **not** call
   `wr_simplify_doublecut_cl()`.

### Phase D: Weight Cutoff, Passive Indexing, Propagation, and Queue Insertion

For partial hyperresolution clauses, `wr_calc_clause_weight()` is computed and
the clause is pushed directly to `g->hyper_queue` via
`wr_push_cl_hyper_queue()`.

For all normal finished clauses:

1. **Weight and resource cutoff:**
   * Computes `weight = wr_calc_clause_weight(g, res, &size, &depth, &length)`.
   * Calls `wr_derived_weight_check(g, avg, weight, size, depth, length, xatomnr, yatomnr)`,
     which rejects the clause if any of the following fail:
     * `HARD_DEPTH_LIMIT` / `HARD_SIZE_LIMIT`,
     * strategy caps (`cl_keep_weightlimit`, `cl_keep_sizelimit`,
       `cl_keep_depthlimit`, `cl_keep_lengthlimit`),
     * `queue_termbuf` fullness thresholds (> 1/2 full: requires `weight < avg`;
       > 3/4 full: requires `weight * 2 < avg`; > 7/8 full: rejects all),
     * `passed_ratio` thresholds (> 0.50: `weight < avg`; > 0.70:
       `weight * 2 < avg`; > 0.90: rejects all).
   * **On rejection:** increments `stat_weight_discarded_cl`, restores
     `CVEC_NEXT(g->build_buffer) = initial_queue_termbuf_next` (rolling back the
     `queue_termbuf` allocation), and returns.
2. **Statistics update:** Increments `g->stat_kept_cl` and updates running
   `g->avg_kept_weight`.
3. **Metadata and passive unit/double hashing:**
   * Calls `resmeta = wr_calc_clause_meta(g, res, cl_metablock)` (updates rule
     literal meta fields on `res`; `cl_metablock` itself is local to this call).
   * Calls `hash = wr_add_cl_to_unithash(g, res, resmeta)`: inserts **all**
     1-literal kept clauses (both ground and non-ground) into
     `hash_{pos,neg}_groundunits`, returning the literal hash (or `-1` for
     non-units).
   * Calls `wr_add_cl_to_doublehash(g, res)`: inserts all 2-literal rule
     clauses into `hash_{pos,neg}_grounddoubles` under both literals' hashes.
4. **Optional eager unit propagation (`wr_process_resolve_result` when `g->propagate` is set):**
   * If `hash >= 0` (kept unit): calls `wr_propagate_unit(g, res, resmeta, hash, xcl_as_active)`
     (`Reasoner/propagate.c`), which looks up `g->hash_atom_occurrences` and
     resolves away opposite-polarity occurrences via
     `wr_resolve_propagated_clause()` (note: the same-polarity `blockvec` loop
     in `propagate.c` is currently disabled).
   * If `hash < 0` (multi-literal): calls `wr_store_atom_occurrences(g, res)` to
     record every non-answer literal in `g->hash_atom_occurrences`.
5. **Optional strong-unit indexing:** If `g->use_strong_unit_cutoff`, calls
   `wr_cl_store_res_units(g, res)`.
6. **Passive queue insertion:** `wr_push_cl_clpick_queues(g, g->clpick_queues, res, weight)`
   inspects the clause's `HISTORY_PRIORITY_POS` role (and
   `g->cl_pick_queue_strategy`) to select one of the 4 role blocks, then pushes
   `(gint)res` to `simplequeue` and `res` (at bucket `weight`) to `priorqueue`.
7. **Propositional instantiation hook:** Calls `wr_cl_create_propinst(g, res)`.

---

## 4. Quick Reference: Where Each Index Is Written vs. Read

| Index / Structure | Scope | Written when | Read when |
|---|---|---|---|
| `clpick_queues` (4 blocks × `simplequeue` + `priorqueue`) | Passive | Derived Phase D (`wr_push_cl_clpick_queues`) | Loop Step 2.2 (`wr_pick_from_clpick_queues`) |
| `hyper_queue` | Partial hyperres | Derived Phase D (`wr_push_cl_hyper_queue`) | Loop Step 2.2 (`wr_pick_from_hyper_queue`, before `clpick_queues`) |
| `hash_{pos,neg}_groundunits` | All kept units (ground + non-ground) | Derived Phase D (`wr_add_cl_to_unithash`) | Derived Phase B (`wr_derived_cl_cut_and_subsume`) & Given Step 2.4 (`wr_simplify_cl`) |
| `hash_{pos,neg}_grounddoubles` | All kept 2-literal rule clauses | Derived Phase D (`wr_add_cl_to_doublehash`) & Given Step 2.6 (if shortened to len 2) | Derived Phase C (`wr_simplify_doublecut_cl` in resolution) & Given Step 2.4 (`wr_simplify_cl`) |
| `hash_atom_occurrences` | Multi-literal clauses (when `g->propagate`) | Derived Phase D (`wr_store_atom_occurrences`) | Derived Phase D (`wr_propagate_unit`) |
| `hash_{pos,neg}_active_groundunits` | Active **ground** units only | Given Step 2.6 (`wr_add_cl_to_active_unithash`) | Given Step 2.5 (`wr_given_cl_subsumed`) & `-usekb` lookups |
| `clactivesubsume` | Active non-ground units + active multi-literal clauses | Given Step 2.6 (`wr_add_given_cl_active_list`) | Given Step 2.5 (`wr_given_cl_subsumed`) & Step 2.6 (`wr_given_cl_backsubsume`) |
| `hash_{pos,neg}_atoms` | Active eligible resolution literals (3-level prefix hash) | Given Step 2.6 (`wr_cl_store_res_terms`) | Given Step 2.7 (`wr_resolve_binary_all_active`) |
| `hash_para_terms`, `hash_eq_terms`, `hash_rewrite_terms` | Active equality/paramodulation/rewrite terms | Given Step 2.6 (`wr_cl_store_para_terms`) | Given Step 2.4 (`wr_simplify_cl`), Derived Phase A (`wr_build_calc_term`), & Given Step 2.7 (`wr_paramodulate_{from,into}_all_active`) |
