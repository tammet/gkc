# GKC Strategy File Reference

GKC strategy files are JSON files parsed by `Reasoner/guide.c`. They control
one or more proof-search runs: time limits, output flags, clause-selection
heuristics, equality handling, SINE filtering, and answer limits.

Use them with:

```sh
./gkc problem.txt -strategy strategy.json
./gkc problem.txt -strategytext '{"max_seconds":5,"strategy":["negative_pref"]}'
```

For the command-line interface, see `Doc/cli_reference.md`. For the proof-search
architecture behind these settings, see `Doc/ARCHITECTURE.md`.

Unknown top-level settings produce warnings in `wr_parse_guide_section()`.
Unknown strategy names produce warnings in `wr_parse_guide_strategy_set()`.

## Basic Shape

A single-run strategy is one JSON object:

```json
{
  "print_level": 15,
  "max_seconds": 5,
  "strategy": ["query_focus"],
  "query_preference": 1
}
```

A multi-run strategy uses `runs`:

```json
{
  "print_level": 15,
  "runs": [
    {"max_seconds": 1, "strategy": ["unit"], "query_preference": 0},
    {"max_seconds": 1, "strategy": ["negative_pref"], "query_preference": 1},
    {"max_seconds": 5, "strategy": ["query_focus"], "query_preference": 1}
  ]
}
```

`wr_parse_guide_section()` walks object members in JSON order and recurses when
it reaches `runs`. Put shared defaults before `runs` so a selected run can
override them. Settings written after `runs` are applied after the selected run
and therefore override it. Strategy-name arrays only set flags, so a run's
`strategy` adds to flags set by an outer `strategy`; it does not clear them.

A run entry inside `runs` may itself be an array of strategy objects. That
selects the sequential-strategy path, which reuses search state between those
objects. A top-level array is rejected because its intended independent-versus-
sequential meaning is ambiguous; use an object with `runs` instead.

## Automatic Strategy

When no strategy file or strategy text is supplied, `wg_run_reasoner()` calls
`make_auto_guide()` after `wr_analyze_clause_list()` has analyzed the input.
The result is ordinary strategy JSON: it is parsed by the same guide parser as
a user-supplied strategy and then distributed across workers in the same way.

`make_auto_guide()` is implemented in `Reasoner/analyze.c`. It first calls
`make_sum_input_stats()` to form the `sin_*` statistics used for selection. For
an ordinary proof these are the local input statistics. With an attached shared
KB, local-query and KB counts are added, minima and maxima are combined, and
the KB symbol counts are used as estimates for the combined problem. Thus the
automatic guide can differ between proving a file directly and proving the same
query against a preloaded KB.

The generator derives several gates from those statistics:

* `eq` is true when any positive or negative equality clause is present. If it
  is false, the outer guide sets `"equality": 0`, and equality-dependent run
  templates are omitted.
* `ueq` is true only when every clause is a unit equality clause. This selects a
  dedicated unit-equality group at the start of `make_guide()`.
* SINE variants are enabled only above 50 clauses.
* query-preference variants are suppressed when the analyzed role distribution
  makes them unhelpful. In particular, preferences 2 and 3 are gated by the
  presence and polarity of axioms, assumptions, and goals; preference 1 is
  suppressed when goals make up more than two thirds of the local input.
* problems above 500,000 combined clauses start each template at 50
  deciseconds; other problems start at 1 decisecond.

`make_auto_guide()` emits five template blocks. The first uses the initial time
slice, the second multiplies it by 10, and later blocks multiply it by 5. Each
block calls the generated `make_guide()` in `Reasoner/makeguide.c`, which emits
the concrete portfolio: combinations of unit/query/hardness preferences,
equality and paramodulation restrictions, rewriting choices, SINE levels,
queue ratios, term weights, and clause limits. Conditions in that generated
function omit variants that do not fit the gates above. The generated guide is
an object containing a `runs` array, so normal parallel execution assigns run
indexes round-robin to worker numbers.

The `LTBSPECIAL` internal mode passes `guideparam == 1`. It uses the
`min_strat_timeloop_nr` and `max_strat_timeloop_nr` database-header bounds to
select template blocks and emits quieter output settings. It is not the normal
CLI default-strategy path.

Use `-print 13` or a higher run-printing level to inspect the generated runs.

```sh
./gkc Examples/example1.txt -print 13
```

The exact run list is implementation-specific. Selection and time growth live
in `Reasoner/analyze.c`; the concrete generated portfolio lives in
`Reasoner/makeguide.c`.

## Time Limits

### `max_seconds`

Maximum seconds for the current strategy run. `0` means no per-run limit.

If nonzero and below 2, GKC enables `use_strong_unit_cutoff` for that run.

### `max_dseconds`

Maximum deciseconds for the current strategy run. `0` means no per-run limit.

If nonzero and below 20, GKC enables `use_strong_unit_cutoff` for that run.

### `total_seconds`

Total seconds across all runs. This sets `g->max_seconds`.

### `total_dseconds`

Compatibility alias for `total_seconds`. A positive value is divided by 10 and
rounded upward, so values 1 through 10 become one second, 11 through 20 become
two seconds, and so on. If the same strategy object contains `total_seconds`,
that key takes precedence regardless of JSON property order.

This alias does not provide a subsecond total deadline. Use `max_dseconds` for
a subsecond per-run CPU-time limit.

The command-line `-seconds` option also sets a total search limit in the local
DB header before strategy parsing.


Strategy time checks use the C `clock()` value, so these limits measure process
CPU time rather than wall time. On Unix, the separate CLI `-seconds` limit also
installs a wall-clock `alarm()` deadline.
## Answer Limit

### `max_answers`

Maximum number of stored answers/proofs requested by the run. This sets
`g->required_answer_nr`. The default in `wr_glb_init_simple()` is `1`; automatic
strategies and examples may set other values.

For answer clauses with `$ans`, this controls how many answers are enough. For
ordinary refutation problems, the first proof is usually enough.

This CLI/strategy parser has one answer limit: `max_answers`. It does not
provide separate distinct-answer and proofs-per-answer strategy keys.

## Strategy Names

The `strategy` value may be a string or an array of strings.

```json
{"strategy": "negative_pref"}
```

```json
{"strategy": ["query_focus", "unit", "posunitpara"]}
```

Accepted names in `Reasoner/guide.c`:

| Name | Effect |
| --- | --- |
| `negative_pref` | Prefer negative literals/clauses in ordering decisions. |
| `positive_pref` | Prefer positive literals/clauses in ordering decisions. |
| `hyper` | Enable hyperresolution behavior and use `hyper_queue`. |
| `unit` | Require one resolution argument to be a unit clause. Sets `res_shortarglen_limit = 1`. |
| `double` | Require one resolution argument to have at most two non-answer literals. |
| `triple` | Require one resolution argument to have at most three non-answer literals. |
| `pure_unit` | Strict unit mode. Sets `res_arglen_limit = 1` and `res_strict_arglen_limit = 1`. |
| `hardness_pref` | Prefer clauses/literals by the hardness heuristics in `strat.c`. |
| `query_focus` | Use goal-oriented set-of-support style queue behavior. |
| `knuthbendix_pref` | Use Knuth-Bendix-style ordering preference for literals/terms. |
| `prohibit_nested_para` | Do not directly nest paramodulation derivations. |
| `prohibit_unordered_para` | Paramodulate only from ordered equalities. |
| `prohibit_deep_para` | Avoid deep paramodulation into terms. |
| `posunitpara` | Paramodulate from positive unit equalities only. |
| `max_ground_weight` | Use maximum ground-literal weight as the base clause weight. |
| `max_weight` | Use maximum literal weight as the base clause weight. |
| `hornpref` | Prefer Horn or closer-to-Horn clauses in selection. |
| `posweightdoublepref` | Enable positive weighted double preference. |
| `negweightdoublepref` | Enable negative weighted double preference. |

Strategy names combine by setting several flags. Some combinations are useful,
for example `query_focus` with `unit`; others may overconstrain search.

## Clause Queue and Role Settings

### `query_preference`

Controls how input roles are mapped to initial clause queues. Internally this
sets `g->cl_pick_queue_strategy`.

| Value | Meaning |
| --- | --- |
| `0` | Single queue for all clauses; no role preference. |
| `1` | Use roles as marked by input: goals, assumptions, and axioms are separated. |
| `2` | Treat non-included axioms and positive conjecture parts as assumptions. Imported/background axioms remain favored. |
| `3` | Only fully negative goal clauses stay in the goal queue; assumptions are treated as axioms. |
| `4` | Put every role into the axiom queue. |

The comment in `glb.h` lists only values 0 to 3, but `clstore.c` implements the
distinct value-4 behavior above.

### `given_queue_ratio`

Set `g->pick_given_queue_ratio`, the ratio controlling selection from priority
queues versus regular queues.

### `weight_select_ratio`

Alias for `given_queue_ratio`.

### `reverse_clauselist`

If nonzero, reverse the initial clause list without the usual sorting. This is
used mainly for non-query experiments and strategy tuning.

## Clause Limits

These limits control which derived clauses are kept. `0` means no limit.

| Key | Field | Meaning |
| --- | --- | --- |
| `max_weight` | `cl_maxkeep_weightlimit` | Maximum kept clause weight. |
| `max_size` | `cl_maxkeep_sizelimit` | Maximum kept clause size. |
| `max_depth` | `cl_maxkeep_depthlimit` | Maximum kept term depth. |
| `max_length` | `cl_maxkeep_lengthlimit` | Maximum kept clause length. |

After strategy parsing, `rmain.c` copies these into the dynamic `cl_keep_*`
limits used by the run.

## Clause Weighting

These keys influence clause and literal weight calculations.

| Key | Default in `wr_glb_init_simple()` | Meaning |
| --- | --- | --- |
| `depth_penalty` | `0` | Penalty for term depth. |
| `length_penalty` | `1` | Penalty for term/clause length. |
| `var_weight` | `5` | Weight for variable occurrences. |
| `repeat_var_weight` | `7` | Additional weight for repeated variables. |

Automatic strategies often override these values aggressively, especially for
large problems.

## Equality and Rewriting

### `equality`

Set `g->use_equality_strat`. Default is `1`. During run initialization,
`rmain.c` still disables equality if the analyzed input and attached KB contain
no equality clauses.

When enabled and relevant, GKC uses equality reflexivity, paramodulation, and
rewrite/demodulation support.

### `rewrite`

Set `g->use_rewrite_terms_strat`. Default is `1`. Use `0` to disable rewrite
term use while still allowing other equality behavior if `equality` remains on.

Additional equality restrictions are strategy names, not top-level keys:
`posunitpara`, `prohibit_nested_para`, `prohibit_unordered_para`, and
`prohibit_deep_para`.

## SINE

### `sine`

Set `g->sine_strat`. Common values are:

| Value | Meaning |
| --- | --- |
| `0` | No SINE filtering. |
| `1` | Less restrictive SINE filtering. |
| `2` | More restrictive SINE filtering. |

Automatic strategies may emit other experimental values. In `-usekb` runs,
`rmain.c` disables SINE for the local strategy run after detecting the shared KB
split.

## Advanced Generation Modes

### `instgen`

If nonzero, enable instantiation-generation behavior. This path is present in
`resolve.c` / `derive.c`, but it is more experimental than ordinary resolution.

### `propgen`

If nonzero, enable propositional-generation behavior and the follow-up
`wr_prop_solve_current()` call after a run. This path is experimental in this
checkout.

### Numeric instantiation

Numeric instantiation is a bounded arithmetic aid. It generates
only bounded numeric instances of a clause with recognized non-ground
arithmetic, then relies on the existing ground calculator and ordinary clause
processing. It is not SMT solving or general algebraic reasoning. Conservative
mode 1 is enabled by default; set the key to `0` to disable it explicitly.

| Key | Default | Meaning |
| --- | --- | --- |
| `arith_instantiation` | `1` | `0` disables it; `1` uses conservative one-variable search; `2` permits a bounded two-variable and deeper search. |
| `arith_inst_max_vars` | mode-dependent | Maximum variables in one candidate substitution (`1` or `2`). |
| `arith_inst_candidate_limit` | `4` / `6` | Local numeric candidates considered per arithmetic clause. |
| `arith_inst_probe_limit` | `16` / `32` | Candidate probes per clause. |
| `arith_inst_keep_limit` | `2` / `4` | Useful generated instances retained per clause. |
| `arith_inst_depth_limit` | `1` / `2` | Maximum numeric-instantiation ancestry depth. |
| `arith_inst_global_limit` | `1000` | Candidate probes allowed in one run. |

The paired values are for modes `1` and `2`. Explicit limits override those
mode defaults and are clamped to safe ranges. The feature is inert unless
computation functions are enabled and input contains a recognized non-ground
arithmetic operation, comparison, conversion, rounding operation, unary minus,
or numeric type test. It targets arithmetic arguments and comparison boundaries
sparingly; a generated instance is retained only when ground calculation gives
immediate syntactic progress. The original clause remains available.

For example, select the stronger mode with:

```json
{
  "strategy": ["unit"],
  "query_preference": 0,
  "arith_instantiation": 2
}
```

Use the ordinary JSON strategy mechanisms described above for portfolios and
per-run overrides. With statistics enabled, an enabled run prints `arithinst:`
counter lines for scanned clauses, probes, retained instances, proofs, and
rejections. See `Doc/ARITHMETIC_INSTANTIATION.md` and the arithmetic examples for
scope and limits.

## Output Keys

### `print`

`1` enables normal printing. `0` forces `print_level` to no-print behavior in
`rmain.c`.

### `print_level`

Detail level. The same level bands are used by the command-line `-print` flag.

In the `gkc` CLI, the nonzero database-header print level (default `15`, or the
value supplied with `-print`) is reapplied after each run object is parsed. It
therefore overrides this strategy key in normal use. With `-print 0`, the
header value is not reapplied and a strategy `print_level` can take effect:

| Level | Effect |
| --- | --- |
| `0` | Suppress almost all printing. |
| `1` to `10` | Tiny/default proof output. |
| `11` | Extra proof/history detail. |
| `12` | Run starts and strategy text. |
| `13` to `15` | Low-detail run/statistics output. |
| `16` to `20` | Include final given clauses. |
| `21` to `30` | Include initial/final given clauses and derived clauses. |
| `31` to `50` | Detailed parser, active/passive, derived, and selection diagnostics. |
| `51+` | Detailed diagnostics plus internal data structure dumps. |

### `print_json`

If nonzero, request JSON output and JSON clause printing.

### `print_tptp`

If nonzero, request TPTP/CASC-style output and proof printing. If set to `0`,
`guide.c` also disables FOF source/skolem proof storage flags and leaves
arithmetic computation enabled.

Do not request both JSON and TPTP output in the same run.

## Multiple Runs

The `runs` key is an array of run objects. `wg_run_reasoner()` tries the run
sections in order. In parallel mode, different worker processes may take
different subsequences of the run list, so the first proof found and trace order
can vary.

Use `-parallel 0` when comparing strategy behavior.

```json
{
  "print_level": 15,
  "runs": [
    {"max_seconds": 1, "strategy": ["unit"], "query_preference": 0},
    {"max_seconds": 1, "strategy": ["negative_pref"], "query_preference": 0},
    {"max_seconds": 5, "strategy": ["hardness_pref", "posunitpara"], "query_preference": 1}
  ]
}
```

## Query Roles

Roles come from the parser and affect search primarily through queue placement
when `query_focus` and nonzero `query_preference` are used.

In JSON/JSON-LD-LOGIC input, `@question` expands to a question role and answer
collection machinery. In TPTP-style input, roles such as `axiom`, `assumption`,
`conjecture`, and `negated_conjecture` influence the same role counters and
queues.

Operationally:

* goal/question clauses are useful with `query_focus`;
* assumptions can receive different initial queue treatment;
* imported/background axioms may be treated differently from local axioms under
  `query_preference: 2`.

See `Doc/ARCHITECTURE.md` for the active/passive initialization sequence.

## Practical Examples

Small deterministic proof run:

```json
{
  "print_level": 10,
  "max_seconds": 10,
  "strategy": ["negative_pref"],
  "query_preference": 0
}
```

Query-answering run:

```json
{
  "print_level": 15,
  "max_seconds": 30,
  "strategy": ["query_focus"],
  "query_preference": 1,
  "max_answers": 3
}
```

Equational run with restricted paramodulation:

```json
{
  "print_level": 15,
  "max_seconds": 20,
  "strategy": ["hardness_pref", "knuthbendix_pref", "posunitpara"],
  "query_preference": 0,
  "equality": 1,
  "rewrite": 1
}
```

Single-threaded debugging run:

```json
{
  "print_level": 30,
  "max_seconds": 5,
  "strategy": ["negative_pref"],
  "query_preference": 0,
  "reverse_clauselist": 0
}
```

Run it with:

```sh
./gkc problem.txt -parallel 0 -strategy debug_strategy.json
```

## Accepted Top-Level Keys

As of this checkout, `Reasoner/guide.c` accepts these top-level keys:

```text
print
print_json
print_tptp
print_level
max_answers
max_seconds
total_seconds
max_dseconds
total_dseconds
given_queue_ratio
weight_select_ratio
sine
max_weight
max_size
max_depth
max_length
depth_penalty
length_penalty
var_weight
repeat_var_weight
equality
rewrite
reverse_clauselist
query_preference
instgen
propgen
arith_instantiation
arith_inst_max_vars
arith_inst_candidate_limit
arith_inst_probe_limit
arith_inst_keep_limit
arith_inst_depth_limit
arith_inst_global_limit
strategy
runs
```

The accepted strategy names are listed in the Strategy Names section above.
