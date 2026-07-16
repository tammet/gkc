# GKC bounded arithmetic instantiation

This is the current-code guide for the numeric-instantiation pass in
`Reasoner/arithinst.c`. The pass is a sound, bounded candidate-generation
heuristic. It is not an arithmetic decision procedure, symbolic algebra
system, or SMT solver: `Reasoner/build.c` remains the component that evaluates
and decides arithmetic literals after they become ground.

## User boundary and defaults

The strategy key has three modes:

| Mode | Meaning | Mode-derived limits |
| ---: | --- | --- |
| `0` | Disabled. | No arithmetic scans, probes, or cache allocation. |
| `1` | Conservative and default; instantiate at most one variable. | 4 candidates, 16 probes and 2 kept instances per clause, ancestry depth 1. |
| `2` | Stronger opt-in; instantiate at most two variables. | 6 candidates, 32 probes and 4 kept instances per clause, ancestry depth 2. |

Every mode has a run-wide default limit of 1000 probes. Explicit strategy
limits are validated and clamped; see
[`strategy_reference.md`](strategy_reference.md#numeric-instantiation).

Mode 1 is initialized in `wr_glb_init_simple()`. An omitted strategy key
therefore inherits conservative mode. Use this explicit opt-out when comparing
ordinary proof search:

```json
{"strategy":["unit"],"query_preference":0,"arith_instantiation":0}
```

## Runtime gate and scheduling

Input analysis in `Reasoner/analyze.c` sets
`have_nonground_arithmetic` only when a recognized arithmetic function,
numeric conversion/type test, rounding operation, unary minus, or numeric
comparison contains a variable. Attached-KB statistics combine the local query
and shared KB flags.

`wr_genloop()` calls `wr_generate_arith_instances()` after the selected clause
has passed simplification, truth/falsehood handling, and forward subsumption,
but before the ordinary given-clause rebuild and activation. The call requires:

```text
arith_instantiation != 0
use_comp_funs_strat != 0
use_comp_arithmetic != 0
have_nonground_arithmetic != 0
remaining global probe budget
```

Consequently, non-arithmetic input performs no arithmetic clause scans or
probes. The duplicate cache is allocated lazily only when an actual arithmetic
clause reaches the generator, so non-arithmetic runs allocate no cache.

## Scan, candidates, and progress gate

`Reasoner/arithinst.c` follows this pipeline:

1. Scan the selected clause for arithmetic/comparison frontiers whose variables
   can become calculator-ready after a small substitution.
2. Collect typed integer/real candidates from visible clause numbers,
   run-global observed numbers, comparison boundaries and neighbours, simple
   inverse-operation hints, evaluated shallow closures, and the fallback values
   `-1`, `0`, and `1`.
3. Rank and deduplicate candidates, then enumerate at most the mode's configured
   one- or two-variable tuples.
4. Bind candidates through the normal varbank/varstack machinery and rebuild
   the parent through `wr_process_instgen_result_aux()` and
   `wr_build_calc_term()`.
5. Retain the result only if the existing calculator made immediate syntactic
   progress: a literal disappeared, an answer/proof was produced, a useful
   ground unit resulted, or the stronger mode reduced the remaining frontier
   count.
6. Store surviving clauses through the ordinary derived-clause queue, weight,
   metadata, unit-hash, propagation, and answer-registration lifecycle.

The original selected clause continues through normal activation. Numeric
instantiation adds logical instances; it does not replace or strengthen the
parent clause.

Comparisons are first-class frontiers. For example, `$greater(X,7)` can use its
boundary candidate `7`, while `$lesseq(X,7)` can use the neighbouring value
`8` that makes the predicate false.

## Duplicate, ancestry, and memory bounds

The fixed-size duplicate cache key contains the parent clause id, substituted
variable numbers, numeric kinds, and value bits. It suppresses repeated
parent/substitution work within one `glb`. Cache saturation fails closed by
stopping duplicate insertion; all clause and run probe/keep limits still apply.

`wr_arith_ancestry_depth()` follows validated ARITHINST history records and
enforces the mode-derived consecutive-depth limit. This prevents an accepted
instance from recursively opening an unbounded enumeration chain.

Candidate arrays and scan frontiers have compile-time caps in
`Reasoner/arithinst.h`. The cache is ordinary process-local memory, is never
stored in a WhiteDB segment, and is freed by `wr_glb_free_local_complex()`.
Numeric constants created while querying an attached KB are encoded in
`g->local_db`, never the read-only shared segment.

## Proof history and statistics

`WR_HISTORY_TAG_ARITHINST` is a specialized one-parent history. The record
contains the parent clause, substitution count, `(variable, encoded value)`
pairs, and any clauses cut while simplifying the instance. Plain and TPTP
proofs identify the inference as `arithinst`; flattened history traversal
follows both the parent and cut clauses.

At statistical print levels, GKC reports:

```text
arithinst: scanned=... frontiers=... candidates=... probes=... calculated=...
arithinst: kept=... proofs=... rejected(no-progress=... tautology=... cache=...)
arithinst: rejected(budget=...) truncated=...
```

These counters distinguish candidate generation from calculator progress and
retained logical work.

## Examples and regression tests

The tutorial keeps two examples:

- [`arithmetic_instantiation_apples.txt`](../Examples/arithmetic_instantiation_apples.txt):
  default mode 1 derives `$ans(8)`;
- [`arithmetic_instantiation_product.txt`](../Examples/arithmetic_instantiation_product.txt):
  mode 1 stays within its one-variable bound and mode 2 proves.

The full matrix, ground calculator controls, overflow checks, ordinary-
unification controls, comparison boundaries, and expected-hard cases live in
[`Test/arithmetic/`](../Test/arithmetic/). Run:

```sh
python3 Test/arithmetic/check_arithmetic_instantiation.py
```

Known limits include coupled multi-equation solving, variables hidden by
Skolemization, broad integer enumeration, polynomial normalization, and
general inequality propagation. Those require theory reasoning beyond this
heuristic.
