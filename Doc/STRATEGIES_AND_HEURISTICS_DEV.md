# GKC Strategies, Heuristics, and Portfolio Internals

This document explains how gkc analyzes an input problem, constructs and
executes strategy portfolios, weighs and selects clauses, selects literals,
orders equalities, and balances incomplete heuristics against complete fallback
strategies. For user-facing JSON syntax, see `Doc/strategy_reference.md`.

Primary source files:

* `Reasoner/analyze.c`: pre-search problem analysis (`wr_analyze_clause_list`),
  summed stats (`sin_*`), and automatic guide assembly (`make_auto_guide`).
* `Reasoner/makeguide.c`: generated C schedule emitter (`make_guide`) for UEQ,
  equality (FEQ), and non-equality (FNE) portfolios.
* `Reasoner/guide.c`: JSON guide parser (`wr_parse_guide_*`,
  `wr_set_strat_from_guide`) mapping strategy keys onto `glb` fields.
* `Reasoner/clmeta.c`: clause weighting (`wr_calc_clause_weight`) and clause
  metablock computation (`wr_calc_clause_meta`).
* `Reasoner/clstore.c`: multi-queue clause storage and picking
  (`clpick_queues`, priority queues, `hyper_queue`).
* `Reasoner/strat.c`: literal selection (`wr_calc_clause_resolvability`,
  `wr_calc_atom_hardness`) and equality term ordering (`wr_order_eqterms`).
* `Reasoner/rgenloop.c`: given-clause loop, phase transitions, and endgame
  mode.

## 1. End-to-End Strategy Wiring

A search run flows through five stages:

```text
Input clauses (local_db [+ kb_db])
  -> wr_analyze_clause_list() & make_sum_input_stats()   [analyze.c: sin_* stats]
  -> make_auto_guide() -> make_guide()                   [analyze.c + makeguide.c: JSON]
  -> wr_parse_guide_str() / wr_parse_guide_file()        [guide.c: cJSON tree]
  -> per-iteration glb setup in wg_run_reasoner()        [rmain.c + guide.c]
  -> wr_genloop()                                        [rgenloop.c + strat.c + clstore.c]
```

### Stage A: Input Analysis (`Reasoner/analyze.c`)

Before forking workers, `wr_analyze_clause_list()` walks all input clauses
(merging local and shared-KB counts in `make_sum_input_stats()`) and populates
`sin_*` summary fields on `analyze_g`:

* Clause counts by kind/role: `sin_clause_count`, `sin_rule_clause_count`,
  `sin_fact_clause_count`, `sin_goal_count`, `sin_neg_goal_count`,
  `sin_pos_goal_count`, `sin_posunit_goal_count`, `sin_assumption_count`,
  `sin_axiom_count`.
* Structural shape: `sin_unit_clause_count`, `sin_horn_clause_count`,
  `sin_pos_clause_count`, `sin_neg_clause_count`, `sin_poseq_clause_count`,
  `sin_negeq_clause_count`, `sin_unitposeq_clause_count`,
  `sin_chain_clause_count`, min/max/average length, depth, size, and variable
  counts.
* Feature flags: `in_has_fof`, `sin_has_nonground_arithmetic`.

From these counts, `make_auto_guide()` computes routing flags passed to
`make_guide()`:

* `eq`: `1` iff `(sin_poseq_clause_count + sin_negeq_clause_count) > 0`.
* `ueq`: unit-equality problem gate (all clauses are unit equalities).
* `probsize`: `sin_clause_count` (gates SInE and large-theory slices).
* `qp1ok`, `qp2ok`, `qp3ok`: suppress `query_preference` variants when goal or
  goal+assumption clauses dominate the problem (> 2/3 of input clauses) or when
  axioms/assumptions are absent.

### Stage B: Portfolio Schedule Generation (`Reasoner/makeguide.c`)

`make_auto_guide()` calls `make_guide()` across **5 escalating time iterations**
(`iteration = 0..4`), multiplying `dsecs` (deciseconds per strategy run) on
each round so fast strategies finish quickly while harder problems receive
progressively longer slices.

Inside `make_guide()`:

* **UEQ branch (`if (ueq)`):** Uses `unit` (and under native builds, early-tier
  `prohibit_unordered_para` and `prohibit_deep_para` slices for `iteration < 4`),
  varying `query_preference`, `weight_select_ratio`, `depth_penalty`,
  `var_weight`/`repeat_var_weight`, `rewrite`, and SInE slices for larger
  problems. Emscripten builds use a shorter 28-strategy schedule so time
  escalation happens within browser budgets.
* **Non-UEQ branches (`eq` vs. `!eq`):** Mix `negative_pref`, `query_focus`,
  `positive_pref`, `unit`, `hyper`, `hardness_pref`, `sos`, SInE slices, and
  paramodulation/rewrite toggles across iterations.
* Note that `makeguide.c` is generated/maintained as a large C string-emitting
  function (`sprintf` into a dynamically sized JSON buffer). Check buffer
  sizing in `make_auto_guide()` whenever adding a large number of runs.

### Stage C: JSON to `glb` Mapping (`Reasoner/guide.c`)

For each strategy run in `"runs": [...]`, `rmain.c` initializes a fresh `glb`
via `wr_glb_init_simple()` (setting defaults in `Reasoner/glb.c`) and applies
the JSON object in `guide.c`. Note that a top-level strategy file must be a
JSON object (`{ "runs": [ ... ] }` or a single run object), not a bare JSON
array.

## 2. Clause Weighting, Multi-Queue Selection, and Endgame Mode

### Clause Weighting (`Reasoner/clmeta.c`)

`wr_calc_clause_weight(g, cl, &size, &depth, &length)` computes the integer
priority used to insert a clause into priority queues and to enforce
`max_weight` / `max_size` / `max_depth` discard limits:

* Walks each literal and term, accumulating symbol weights, variable penalties
  (`var_weight`, `repeat_var_weight`), term depth (`depth_penalty`), and
  clause-length factors.
* Evaluates equality orientation via `wr_order_eqterms()` when equality weight
  adjustments apply.
* Outputs `size`, `depth`, and `length` alongside the combined weight so the
  caller can check hard limits without re-walking the clause.

### Multi-Queue Clause Selection (`Reasoner/clstore.c`)

Passive clauses are partitioned into `g->clpick_queues`, a vector of 4
role-based queue blocks (`NROF_CLPICK_QUEUES == 4`, each 10 `gint`s):

| Block | Constant | Role |
| --- | --- | --- |
| 0 | `GOAL_ASSUMPTIONS_QUEUE_POS` (1) | Goal + assumptions combined |
| 1 | `GOAL_QUEUE_POS` (11) | Goal / from-goal clauses |
| 2 | `ASSUMPTIONS_QUEUE_POS` (21) | Assumptions |
| 3 | `AXIOMS_QUEUE_POS` (31) | Axioms / background clauses |

Selection order in `wr_genloop()` (`Reasoner/rgenloop.c`):

1. **Hyper queue first:** `wr_pick_from_hyper_queue()` drains `g->hyper_queue`
   (partial hyperresolution clauses) before consulting normal passive queues.
2. **Role-queue interleaving:** `wr_pick_from_clpick_queues()` cycles among the
   4 role blocks according to `CLPICK_THIS_QUEUE_RATIO` / `_COUNTER` (set from
   `query_preference` and related strategy flags).
3. **Age vs. weight interleaving inside each block:** Each block pairs a FIFO
   ordinary queue (`CLPICK_QUEUE_POS`, age order) with a bucketed priority
   queue (`CLPICK_PRIORQUEUE_POS`, weight order), controlled by
   `CLPICK_PRIORQUEUE_RATIO` / `_COUNTER` (`weight_select_ratio` /
   `pick_given_queue_ratio`).

### Time-Ratio Phases and Endgame Mode (`Reasoner/rgenloop.c`)

As `g->passed_ratio` (fraction of the current strategy run's time budget
elapsed) rises inside `wr_genloop()`, gkc progressively narrows clause
selection to hunt for short-clause contradictions before timeout:

| `passed_ratio` threshold | Automatic adjustment in `rgenloop.c` |
| --- | --- |
| `> 0.50` | Sets `g->pick_given_queue_ratio = 100` (strong weight bias). |
| `> 0.70` | Caps `g->res_shortarglen_limit = 2`. |
| `> 0.85` | Caps `g->res_shortarglen_limit = 1`. |
| `> 0.90` | Enters `g->endgame_mode = 1`, sets `g->res_arglen_limit = 1`, skips non-unit given clauses (`wr_count_cl_nonans_atoms > 1`), and skips `back_subsume` checks on given candidates. |

Be aware of this behavior when debugging runs near their `max_seconds` /
`max_dseconds` limit: a strategy that behaves differently in its final 10–50%
of runtime is likely crossing one of these `passed_ratio` thresholds.

## 3. Literal Selection, Term Ordering, and Completeness

### Literal Selection (`Reasoner/strat.c`)

`wr_calc_clause_resolvability(g, cl, allowall, hyperpartial)` determines which
literals of an active/given clause are eligible for resolution and
paramodulation:

* Returns a bitmask/selection status marking eligible literals according to the
  active strategy (`negative_pref`, `positive_pref`, `hardness_pref`, `unit`,
  `hyper`, `query_focus`, etc.).
* When hardness-based selection is active, `wr_calc_atom_hardness()` /
  `wr_calc_atom_hardness_aux()` scores each literal by polarity, depth, size,
  variable structure, and symbol occurrence frequency so the search selects
  restrictive literals first.

### Equality Term Ordering (`Reasoner/strat.c`)

`wr_order_eqterms(g, a, b, vb)` compares two sides of an equality (optionally
under current varbank bindings `vb`) and returns:

* `1` if `a > b` (orient left-to-right),
* `2` if `b > a` (orient right-to-left),
* `0` if incomparable (or equal).

Implementation overview:

1. Variable/occurs-in checks (`wr_order_eqterms_occurs_in`) handle subterm
   containment when one side is a variable.
2. Constant comparisons use `wr_order_eqterms_const_lex_smaller()`.
3. Complex terms compute variable-aware weights via
   `wr_order_eqterms_weight_vars()` (with `xcountedvarlist` /
   `ycountedvarlist`), breaking ties via `wr_order_eqterms_lex_order()`.

**Important developer caveats on `wr_order_eqterms`:**

* The current ordering is an ad-hoc weight + lexicographic heuristic rather
  than a full Knuth-Bendix Ordering (KBO) or Lexicographic Path Ordering (LPO).
  In particular, tie-breaking and variable-count handling do not satisfy full
  simplification-ordering / stability-under-substitution invariants in all edge
  cases.
* Never assume `wr_order_eqterms(g, a, b, vb) != 0` guarantees termination of
  rewriting on non-ground equalities without the existing variable-containment
  guards, and do not assume completeness when combining aggressive equality
  restrictions (`prohibit_unordered_para`, `prohibit_deep_para`, `posunitpara`).

### Complete vs. Incomplete Strategy Flags

GKC intentionally mixes fast **incomplete** heuristic strategies with
**complete** fallback strategies inside its portfolio. When modifying
strategies or building new portfolio slices, keep the completeness boundary
explicit:

| Strategy / flag | Completeness status | Why / practical rule |
| --- | --- | --- |
| `negative_pref` (without aggressive limits) | Complete for non-equality resolution; watch equality restrictions | Selects negative literals when present; standard workhorse for FOF/Horn. |
| `sos` (Set of Support) | Complete only when the non-SoS axioms are satisfiable (and equality axioms/orientations are not over-restricted) | Powerful for goal-directed search; always pair with a non-SoS run (`query_preference: 0`) in portfolios. |
| `unit` / `pure_unit` | **Incomplete** for general non-Horn FOF; complete for Horn/UEQ (subject to equality ordering) | Restricts resolution to unit partners; fast on Horn and UEQ. |
| `hyper` | Complete for positive hyperresolution in non-equality CNF; interacts subtly with equality | Generates positive clauses only; pair with binary resolution runs. |
| `positive_pref` | Complete for resolution in CNF, but often slower on typical Horn goals | Useful complementary diversity slice. |
| `query_focus` | **Incomplete** in general | Heavily biases search toward goal-derived clauses; great early slice, never sole fallback. |
| `prohibit_unordered_para` | **Incomplete** (especially with non-orientable equations such as commutativity/associativity) | Drops paramodulation from/into unoriented equations; `makeguide.c` restricts these to `iteration < 4`. |
| `prohibit_deep_para` / `prohibit_nested_para` | **Incomplete** | Blocks paramodulation at deep subterm positions; fast heuristic filter only. |
| `posunitpara` | **Incomplete** for non-unit equality | Restricts paramodulation to positive unit equalities. |
| `max_size` / `max_depth` / `max_weight` caps | **Incomplete** | Prunes large derived clauses; keep uncapped runs in later portfolio tiers. |
| `sine` (axiom selection) | **Incomplete** (drops unselected axioms) | Essential for large theories (`probsize > 9`); always pair with full-axiom fallback runs. |
| `rewrite: 0` | Preserves completeness (disables demodulation) | Valuable fallback when aggressive rewriting or non-KBO orientation stalls a proof. |

**Portfolio design rule:** Fast incomplete heuristics belong in early slices
(`iteration < 4`, small `dsecs`). Later tiers and fallback slices must always
retain unrestricted, complete configurations (no `max_depth`/`max_size` caps,
no `prohibit_unordered_para`/`prohibit_deep_para`, `query_preference: 0`, and
at least one `rewrite: 0` fallback for equality).

## 4. Checklist for Adding a New Strategy Heuristic or Portfolio Slice

1. **Add control fields:** Declare the flag/parameter in `glb` (`Reasoner/glb.h`)
   and initialize a safe backward-compatible default in `wr_glb_init_simple()`
   (`Reasoner/glb.c`).
2. **Parse from JSON:** Wire the JSON key in `Reasoner/guide.c` and document it
   in `Doc/strategy_reference.md` if user-facing.
3. **Wire into search:** Apply the heuristic in the narrowest owning module
   (`clmeta.c` for weights, `clstore.c` for queues, `strat.c` for literal
   selection / ordering, `resolve.c` / `rewrite.c` for inference filtering,
   `rgenloop.c` for loop phases).
4. **Verify in isolation first:** Test with `-parallel 0` and an explicit
   single-run `-strategy` JSON file before touching `makeguide.c`.
5. **Respect completeness and time budgets in `makeguide.c`:**
   * Gate problem-specific slices with `analyze.c` flags (`eq`, `ueq`,
     `probsize`, `qp*ok`, `sineok`) so irrelevant strategies do not waste
     portfolio time.
   * Keep incomplete heuristics out of late fallback tiers (`iteration >= 4`).
   * Verify `make_auto_guide()` buffer capacity in `Reasoner/analyze.c` if the
     emitted JSON string grows substantially.
   * Keep the `#ifdef __EMSCRIPTEN__` compact schedule in mind for browser
     builds.
6. **Check hot-path cost:** Ensure the new heuristic does not introduce
   `malloc()`, string decoding, or redundant term walks inside inner loops
   (see `Doc/PERFORMANCE_AND_PARALLELISM.md`).
