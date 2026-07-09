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

Top-level settings are parsed before the selected run object, so a run can
override top-level defaults.

If the whole strategy JSON is an array, `wr_parse_guide_section()` treats it as
a sequential strategy section. This path exists in code but is less commonly
used than `runs`.

## Automatic Strategy

When no strategy file or strategy text is supplied, `wg_run_reasoner()` calls
`make_auto_guide()` after input analysis. The generated guide is based on clause
statistics, equality presence, query roles, SINE suitability, time limits, and
shared-KB state. It usually contains many short runs with different strategies.

Use `-print 13` or a higher run-printing level to inspect the generated runs.

```sh
./gkc Examples/example1.txt -print 13
```

The exact generated strategy is implementation-specific and changes in
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

Total deciseconds across all runs. This sets `g->max_dseconds`.

The command-line `-seconds` option also sets a total search limit in the local
DB header before strategy parsing.

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

The comment in `glb.h` documents values 0 to 3. Some older text mentions value
4, but this checkout does not document a distinct behavior for it.

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

## Output Keys

### `print`

`1` enables normal printing. `0` forces `print_level` to no-print behavior in
`rmain.c`.

### `print_level`

Detail level. The same level bands are used by the command-line `-print` flag:

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
strategy
runs
```

The accepted strategy names are listed in the Strategy Names section above.
