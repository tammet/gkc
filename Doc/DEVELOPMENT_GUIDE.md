# Developing and Extending GKC

This guide is a practical companion to `Doc/ARCHITECTURE.md`. It focuses
on how to change gkc safely: where to look, what helper functions to use, which
state must be reset, and which mistakes tend to corrupt terms, clauses,
variable bindings, or shared-memory KBs.

## 1. Start Here

Before changing code, read:

1. `README.md`: build modes, command-line flags, input syntaxes, and strategy
   file basics.
2. `Examples/README.md`: proof examples and what output should look like.
3. `Doc/ARCHITECTURE.md`: source-file map, process flow, shared-KB setup, and search-loop structure.
4. `Doc/DATA_REPRESENTATION.md`: encoded values, records, clauses, terms, varbanks, and buffers.
5. `Reasoner/clterm.h`: authoritative clause, literal, atom, and term access macros.
6. `Reasoner/unify.h` and `Reasoner/unify.c`: equality, matching,
   unification, varbanks, and varstack.
7. `Reasoner/derive.c` and `Reasoner/build.c`: how resolvents and new clauses
   are built.
8. `Reasoner/subsume.c`: canonical matching/backtracking idioms.
9. `Doc/SHARED_MEMORY.md`: shared-memory KB invariants.
10. `Doc/TERM_MATCHING.md`: equality, matching, unification, varstack, and subsumption traps.

Use `rg` first when exploring:

```sh
rg -n "wr_process_resolve_result|wr_build_calc_term|wr_match_term" Reasoner
rg -n "wg_in_attached_kb|local_db|kb_g" Db Reasoner Main
```

## 2. Where to Make a Change

Start from the subsystem that owns the behavior. Many bugs in this codebase come
from fixing the symptom in the loop while leaving the real owner unchanged.

| Change area | Primary files | Notes |
| --- | --- | --- |
| Command-line mode, memory setup, input dispatch | `Main/gkc.c` | Store run options in the local DB header when they must survive into `rmain.c`. |
| Text/TPTP/JSON parsing | `Parser/dbparse.c`, `Parser/jsparse.c`, parser `.l`/`.y` files, `Builtparser/` generated copies | Keep generated parser files in sync when syntax changes. |
| FOF-to-clause conversion | `Parser/dbclausify.c` | Skolemization, definition predicates, distribution, and flattening happen before search. |
| Reasoner entry, analysis, strategy runs | `Reasoner/rmain.c`, `Reasoner/analyze.c`, `Reasoner/guide.c`, `Reasoner/makeguide.c` | `rmain.c` owns the database split, `analyze_g`, guide parsing, per-run `glb`, and `wr_genloop()` calls. |
| Given-clause lifecycle | `Reasoner/rgenloop.c` | Candidate picking, simplification, forward/back subsumption, active insertion, and inference call order. |
| Binary resolution, factoring, paramodulation | `Reasoner/resolve.c` | Candidate scanning only; final clause construction belongs in `derive.c`. |
| Derived clause construction | `Reasoner/derive.c`, `Reasoner/build.c`, `Reasoner/history.c` | Use build setup/cleanup helpers and create histories before queueing. |
| Clause and term layout | `Reasoner/clterm.h`, `Reasoner/clterm.c`, `Doc/DATA_REPRESENTATION.md` | Treat macros here as authoritative. Update the representation doc when invariants change. |
| Matching, unification, subsumption | `Reasoner/unify.c`, `Reasoner/subsume.c`, `Doc/TERM_MATCHING.md` | Direction and varstack cleanup are usually the failure points. |
| Active/passive indexes | `Reasoner/clstore.c`, `Reasoner/hash.c` | Shared KB indexes are read through `r_kb_g(g)` during query. |
| Shared-memory KB behavior | `Db/dbmem.c`, `Db/dballoc.h`, `Db/dbdata.c`, `Reasoner/rmain.c`, `Doc/SHARED_MEMORY.md` | Query-time writes must go to `g->local_db` or be skipped for KB-resident objects. |

Keep the process boundaries clear:

1. `Main/gkc.c` imports input and calls the converter or reasoner.
2. Parsers and clausification create database records and histories.
3. `wg_run_reasoner()` analyzes input, builds/parses a strategy guide, and
   initializes a fresh `glb` for each run.
4. `wr_genloop()` owns the active/passive given-clause loop.
5. `resolve.c` finds inference candidates; `derive.c` and `build.c` create the
   derived clause and queue it.

If a change crosses one of these boundaries, update both sides deliberately and
add a regression that reaches the handoff.

## 3. Build and Smoke Test

The simplest build path is:

```sh
./compile.sh
```

or:

```sh
make
```

Useful smoke tests:

```sh
./gkc Examples/example1.txt -parallel 0
./gkc Examples/example2.txt -parallel 0
./gkc Examples/steam.txt -parallel 0
./gkc Examples/arithmetic1.txt -parallel 0
```

Use `-parallel 0` when debugging or comparing output. Default Unix runs use
multiple worker processes, so trace output and even the first proof found can
vary.

Useful print levels:

* `-print 10`: default proof.
* `-print 11`: proof plus successful strategy.
* `-print 15`: stats and planned runs.
* `-print 20`: given clauses.
* `-print 40`: derived clauses.
* `-print 50` or higher: detailed rule/literal selection diagnostics.

For derived-clause debugging:

```sh
./gkc problem.txt -parallel 0 -derived
```

## 4. Adding or Changing a Command-Line Option

Main command-line parsing lives in `Main/gkc.c`. Strategy parsing lives in
`Reasoner/guide.c`.

For a command-line option:

1. Add or update help text in `Main/gkc.c`.
2. Parse the flag into the database header or local variables.
3. Make sure `wg_run_reasoner()` copies the value into the per-run `glb` if it
   is a search option.
4. If a strategy-file equivalent is needed, add parsing in `guide.c` and a
   field in `glb.h`.
5. Update `README.md` or a separate reference doc if it is user-visible.

Watch for the database split:

* Global run flags stored in `dbmemsegh(local_db)` should be read from
  `local_db` in `rmain.c`, not from the shared KB header.
* Do not write query-time options into a shared KB header.

## 5. Adding a Strategy Field

Most strategy fields land in `glb` and are parsed from guide JSON.

Checklist:

1. Add the field to `glb` in `Reasoner/glb.h`.
2. Set a default in `wr_glb_init_simple()` or the relevant init function in
   `Reasoner/glb.c`.
3. Parse it in `Reasoner/guide.c`.
4. Use it in the smallest relevant location, such as `rgenloop.c`,
   `resolve.c`, `derive.c`, or `clstore.c`.
5. Confirm behavior with a one-run strategy file to avoid auto-strategy noise.

Example one-run strategy:

```json
{
  "print_level": 15,
  "max_seconds": 5,
  "strategy": ["negative_pref"],
  "query_preference": 0
}
```

Run with:

```sh
./gkc Examples/example1.txt -parallel 0 -strategy mystrategy.json
```

## 6. Working With Clauses

Always branch on clause kind:

```c
if (wg_rec_is_rule_clause(g->db, cl)) {
  int n = wg_count_clause_atoms(g->db, cl);
  for (int i = 0; i < n; i++) {
    gint meta = wg_get_rule_clause_atom_meta(g->db, cl, i);
    gint atom = wg_get_rule_clause_atom(g->db, cl, i);
  }
} else if (wg_rec_is_fact_clause(g->db, cl)) {
  gint atom = rpto(g, cl);
}
```

Do not assume a unit is a fact clause. A unit with variables or a negative unit
is a rule clause.

When creating a clause:

* Use `wr_create_fact_clause()` only for ground positive units.
* Use `wr_create_rule_clause()` for negative, non-ground, or multi-literal
  clauses.
* Set rule-clause literal meta and atom fields through the macros/helpers in
  `clterm.h`.
* Build or copy nested atom/term records through `wr_build_calc_term()` or
  `wr_copy_record()` unless you know exactly which database and term header
  semantics you need.

When mutating a clause in place, guard shared-KB records:

```c
if (!wg_in_attached_kb(cl)) {
  wr_sort_cl(g, cl);
  wr_calc_clause_meta(g, cl, metablock);
}
```

Avoid in-place mutation of clauses unless the surrounding code already expects
it. Derived clauses are normally built fresh.

## 7. Working With Literals and Polarity

Literal polarity is encoded in the literal meta, not in the atom:

```c
int neg = wg_atom_meta_is_neg(g->db, meta) != 0;
```

Resolution requires opposite polarity:

```c
if (litmeta_negpolarities(meta1, meta2)) {
  /* candidates for a resolution cut */
}
```

Subsumption requires same polarity:

```c
if (!litmeta_negpolarities(meta1, meta2)) {
  /* candidates for matching */
}
```

If you are scanning a fact clause, it is positive by construction. There is no
literal meta field to inspect.

## 8. Working With Terms and Atoms

Do not manually iterate from field 0. Use the unification configuration:

```c
int start = RECORD_HEADER_GINTS + g->unify_firstuseterm;
int funpos = RECORD_HEADER_GINTS + g->unify_funpos;
```

Normal values:

* field 0: reserved;
* field 1: functor or predicate URI;
* field 2 and later: arguments.

When checking a predicate/functor:

```c
gptr tptr = rotp(g, atom);
gint fun = tptr[RECORD_HEADER_GINTS + g->unify_funpos];
if (wg_get_encoded_type(g->db, fun) == WG_URITYPE) {
  char *name = wg_decode_uri(g->db, fun);
}
```

Functor equality is only a prefilter. After selecting candidates by functor or
hash, still call `wr_equal_term()`, `wr_match_term()`, or `wr_unify_term()`.

## 9. Matching, Unification, and Varstack Discipline

Use the right relation:

```c
/* syntactic equality */
int same = wr_equal_term(g, a, b, 1);

/* general pattern `pat` matches instance `inst` */
int ok = wr_match_term(g, pat, inst, 1);

/* symmetric unification */
int unifies = wr_unify_term(g, a, b, 1);
```

`wr_match_term()` and `wr_unify_term()` write bindings into `g->varbanks` and
push changed cells onto `g->varstack`.

For isolated probes:

```c
int vc = CVEC_NEXT(g->varstack);
int ok = wr_match_term(g, general, instance, 1);

if (ok) {
  /* inspect bindings or build an instantiated term */
}

if (vc != CVEC_NEXT(g->varstack)) {
  wr_clear_varstack_topslice(g, g->varstack, vc);
}
```

For top-level resolution candidate loops, clear all bindings after each
candidate:

```c
if (wr_unify_term(g, xatom, yatom, 1)) {
  wr_process_resolve_result(g, xatom, xcl, yatom, ycl, xcl_as_active);
}
wr_clear_varstack(g, g->varstack);
```

For a brand-new matching context outside normal `wr_genloop()` setup:

```c
if (g->varbanks == NULL) {
  g->varbanks = wr_vec_new(g, NROF_VARBANKS * NROF_VARSINBANK);
}
if (g->varstack == NULL) {
  g->varstack = wr_cvec_new(g, NROF_VARBANKS * NROF_VARSINBANK);
}
CVEC_NEXT(g->varstack) = 2;
wr_clear_all_varbanks(g);
```

This matters because `varbanks` is a `vec` used as a raw array. Slot 0 must be
cleared to `UNASSIGNED`, even though that overwrites the usual vector length
header.

## 10. Building an Instantiated Term or Clause

Use the build helpers in `Reasoner/build.c`. They are controlled by `glb`
fields set by setup helpers in `derive.c` and `rgenloop.c`.

Common build phases:

* Given-clause rebuild: `wr_process_given_cl_setupsubst()`,
  `wr_build_calc_cl()`, `wr_process_given_cl_cleanupsubst()`.
* Resolution result first phase: `wr_process_resolve_result_setupsubst()`,
  then `wr_process_resolve_result_aux()`, then cleanup.
* Resolution result final queue copy:
  `wr_process_resolve_result_setupquecopy()`, then
  `wr_derived_build_cl_from_initial_cl()`.
* Partial hyperresolution copy:
  `wr_process_resolve_result_setuphypercopy()`.

For an atom/term under current bindings:

```c
gint built = wr_build_calc_term(g, atom);
if (built == WG_ILLEGAL) {
  /* allocation or build error */
}
```

For paramodulation-style replacement:

```c
int path = 0;
gint built = wr_build_calc_term_replace(g, term, replpos, replterm, &path);
```

Do not leave build flags or `g->build_buffer` in a changed state. Follow the
existing setup/cleanup pattern around the code you are modifying.

## 11. Adding a New Inference Rule

A new inference rule should follow the same lifecycle as resolution and
paramodulation.

1. Select candidate literals or terms using existing indexes where possible.
2. Use `wr_equal_term`, `wr_match_term`, or `wr_unify_term` with clear
   semantics.
3. Snapshot/restore `varstack` around each candidate.
4. Build a half-built literal vector in `g->derived_termbuf`.
5. Apply substitution and variable renaming through `wr_build_calc_term()`.
6. Run cut/subsumption checks before allocating a final clause.
7. Build a history record. If needed, add a new history tag in `history.h`.
8. Build the final clause into `queue_termbuf` or `hyper_termbuf`.
9. Check for pure answer clauses with `wr_cl_derived_is_answer()`.
10. Apply simplification, weight/size/depth limits, metadata calculation,
    hashes, propagation, and queue insertion exactly as existing derivations
    do.

Prefer adding a new `wr_process_*_result()` function modelled on
`wr_process_resolve_result()` or `wr_process_paramodulate_result()` rather than
splicing final-clause queue logic into candidate selection.

## 12. Adding a New Special Predicate or Function

Special predicates/functions are recognized mainly in `Reasoner/build.c`.

Relevant helpers:

* `wr_answer_lit()` recognizes `$ans`.
* `wr_specialcomp_lit()` recognizes computable comparison predicates.
* `wr_equality_atom()` recognizes equality.
* `wr_computable_termptr()` detects computable terms.
* `wr_compute_from_termptr()` dispatches to compute helpers.
* `wr_compute_fun_arith2()`, `wr_compute_fun_arith1()`,
  `wr_compute_fun_str()`, and `wr_compute_fun_list()` implement operations.

Checklist:

1. Add an opcode in `build.h`.
2. Extend detection in `wr_computable_termptr()` or special-literal detection.
3. Implement computation.
4. Encode newly computed values through `g->local_db`, not `g->db`.
5. Return `ACONST_TRUE` or `ACONST_FALSE` only where the caller expects a
   computable predicate truth value.
6. Add examples covering positive and negative cases.

Shared-KB warning:

```c
/* Good for newly computed values */
return wg_encode_double(g->local_db, result);

/* Bad in -usekb runs: g->db may be a read-only shared KB */
return wg_encode_double(g->db, result);
```

## 13. Adding or Changing Indexing

Indexing code lives mainly in `Reasoner/clstore.c`, with hashing helpers in
`Reasoner/hash.c`.

When adding an index:

1. Add fields to `glb.h`.
2. Initialize them in the shared/local complex init functions in `glb.c`.
3. Free them in the corresponding cleanup functions.
4. Store only offset values if the index may live inside a shared KB.
5. Use `rotp` and `rpto` consistently.
6. If the index is built during `-readkb`, treat it as read-only during
   `-usekb` and read through `r_kb_g(g)`.

If an index stores terms or clauses from both local and KB segments, remember
that `g->db` is the offset base. Local encodings produced through `local_db`
are rebased by `encode_kb_offset()`.

## 14. Shared-Memory KB Development Rules

When `-usekb` is active:

* `g->db` is the shared KB base.
* `g->local_db` is the writable child database.
* `r_kb_g(g)` is a private copy of the KB `glb` for reading KB indexes.

Never write to:

* KB clause records;
* KB history records;
* KB URI/string records;
* KB hash/index structures;
* the KB database header.

Before any in-place record mutation:

```c
if (wg_in_attached_kb(ptr)) {
  /* skip, copy, or redirect to local_db */
}
```

Before adding a new allocation site, ask:

* Is this a new run-time object? Use `g->local_db`.
* Is this a build-time KB object in `-readkb`? Allocation in the KB is allowed.
* Is this a malloc-only helper vector? Use `wr_malloc` or the vector helpers.

Regression idea for KB work:

1. Build a throwaway KB with `-readkb`.
2. Hash or dump the segment.
3. Run representative `-usekb` queries.
4. Hash or dump again.
5. The shared KB bytes should be identical.

Also run with several concurrent `-usekb` processes if the code touches shared
index read paths.

## 15. Parser Changes

Parser source files live in `Parser/`; generated files live in both `Parser/`
and `Builtparser/`.

When changing syntax:

1. Modify the `.l` and/or `.y` source.
2. Regenerate parser outputs if flex/bison are available.
3. Update checked-in generated files used by the simple build.
4. Confirm `./compile.sh` still works without requiring flex/bison.
5. Add or update examples in `Examples/`.
6. Test simple syntax, TPTP syntax, and JSON syntax if the change affects
   shared parser/clausifier paths.

Parser-built atoms/terms must still obey the same field layout:

* field 0 reserved;
* field 1 functor;
* fields 2+ arguments;
* positive ground units may become fact clauses;
* negative or non-ground units must remain rule clauses.

## 16. Printing and Debugging

Printing helpers are spread across `Reasoner/` and `Printer/`.

Useful functions during local debugging:

* `wr_print_clause(g, cl)`
* `wr_print_term(g, term)`
* `wr_print_record(g, rec)`
* `wr_print_vardata(g)`
* `wr_print_clause_resolvability(g, cl)`
* `wr_show_clqueue(g)`
* `wr_show_clactive(g)`

Do not leave unconditional debug `printf()` calls in hot paths. Many runs are
parallel by default and debug output from forked processes interleaves badly.
Use existing print flags or `#ifdef DEBUG` style local diagnostics.

## 17. Tests and Example-Based Regression

This repository does not have a single modern test runner covering all
reasoner behavior, so combine targeted examples with focused new cases.

For a narrow change, create or reuse a small problem in `Examples/` or `Test/`
that forces the changed path:

* Matching/subsumption: a problem with a general rule and several candidate
  ground instances.
* Resolution clause building: multi-literal rules whose resolvent is a
  non-ground rule, a ground fact, and an answer clause.
* Equality: examples with rewriting and paramodulation.
* Shared KB: build a KB, query with `-usekb`, and verify no segment mutation.
* Parser changes: include one positive parse and one syntax error case.

Use `-parallel 0` for deterministic traces:

```sh
./gkc Examples/example1.txt -parallel 0 -print 15
```

For performance-sensitive changes, compare at least:

* number of given clauses;
* number of derived/kept/discarded clauses;
* proof found/not found;
* output proof shape if deterministic enough.

## 18. Common Bug Patterns

### Treating a Fact as a Rule Clause

Bad:

```c
gint atom = wg_get_rule_clause_atom(g->db, cl, 0);
```

Good:

```c
gint atom = wg_rec_is_rule_clause(g->db, cl)
  ? wg_get_rule_clause_atom(g->db, cl, 0)
  : rpto(g, cl);
```

### Binding Variables and Forgetting to Roll Back

Bad:

```c
if (wr_match_term(g, pat, inst, 1)) {
  return 1;
}
/* bindings may still be live */
```

Good:

```c
int vc = CVEC_NEXT(g->varstack);
int ok = wr_match_term(g, pat, inst, 1);
if (vc != CVEC_NEXT(g->varstack)) {
  wr_clear_varstack_topslice(g, g->varstack, vc);
}
return ok;
```

### Allocating Into the Shared KB During Query

Bad:

```c
gptr rec = wg_create_raw_record(g->db, len);
gint n = wg_encode_int(g->db, value);
```

Good:

```c
gptr rec = wg_create_raw_record(g->local_db, len);
gint n = wg_encode_int(g->local_db, value);
```

### Using Functor Hash as Equality

Bad:

```c
if (wr_atom_funhash(g, a) == wr_atom_funhash(g, b)) {
  /* same atom */
}
```

Good:

```c
if (wr_atom_funhash(g, a) == wr_atom_funhash(g, b) &&
    wr_unify_term(g, a, b, 1)) {
  /* unifiable atoms */
}
wr_clear_varstack(g, g->varstack);
```

### Reading Atom Field 0

Bad:

```c
gint maybe_subterm = atomptr[RECORD_HEADER_GINTS + 0];
```

Good:

```c
for (int i = g->unify_firstuseterm; i < get_record_len(atomptr); i++) {
  gint subterm = atomptr[RECORD_HEADER_GINTS + i];
}
```

## 19. Keeping Documentation in Sync

Update the focused document that owns the invariant you changed:

* `Doc/ARCHITECTURE.md`: source ownership, process order, and high-level
  search architecture.
* `Doc/DATA_REPRESENTATION.md`: encoded values, record layouts, clause/term
  representation, buffers, varbanks, and offset-base rules.
* `Doc/TERM_MATCHING.md`: equality, one-way matching, unification, varstack,
  and subsumption behavior.
* `Doc/SHARED_MEMORY.md`: shared KB build/query behavior, read-only guards,
  local allocation, and platform hazards.
* `README.md` and `Examples/README.md`: user-visible commands, examples, and
  expected output behavior.

Do not add new references to removed memo files, deleted technical outlines, or
old document names. In GKC maintenance docs, prefer the GKC-specific docs above
over broader or historical notes. After documentation edits, search for removed
filenames and stale document paths before finishing.

## 20. Review Checklist

Before finishing a change:

* Clause kind checks handle fact and rule clauses separately.
* Literal polarity is read from meta.
* Term iteration starts at `g->unify_firstuseterm`.
* Matching/unification uses the right direction and relation.
* Varstack is restored on every failed or completed probe.
* New records and computed values allocate through `g->local_db`.
* Shared-KB records are not mutated.
* `vec`/`cvec` helper return values are reassigned.
* Build flags and buffers are restored by existing cleanup helpers.
* Histories are created for new derivations.
* New strategy/CLI fields have defaults.
* `-parallel 0` smoke tests pass.
* Shared-memory changes have a no-mutation regression plan.

