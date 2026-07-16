# GKC Term Equality, Matching, and Unification

This document is a practical guide to the equality, one-way matching, full
unification, and clause-subsumption machinery in gkc. It complements
`Doc/DATA_REPRESENTATION.md`, which describes the record formats and encoded
values these operations traverse.

Primary source files:

* `Reasoner/unify.h` and `Reasoner/unify.c`: term equality, matching,
  unification, varbanks, varstack, and occurs-check logic.
* `Reasoner/subsume.c`: clause subsumption and the canonical recursive
  backtracking pattern.
* `Reasoner/resolve.c`: binary resolution, factoring, and paramodulation call
  sites for full unification.
* `Reasoner/rewrite.c`: rewrite matching and equality checks.
* `Reasoner/clterm.h`: term/atom/clause layout and literal polarity macros.
* `Reasoner/clmeta.h`: literal and clause metablock prefilters.

## 1. Terms Seen by the Algorithms

The equality/matching/unification functions operate on encoded `gint` values.
A value may be an immediate constant, an encoded variable, or an encoded offset
to a WhiteDB record. Complex terms and atoms are data records.

Normal atom/term fields are:

```text
field 0: reserved term-meta slot
field 1: functor or predicate symbol
field 2: first argument
field 3: second argument
...
```

The functions do not hard-code field 1 as the start. They use the current
`glb` configuration:

| Field | Usual value | Meaning |
| --- | --- | --- |
| `g->unify_firstuseterm` | 1 | First record field compared by equality/match/unify. |
| `g->unify_funpos` | 1 | Functor/predicate position. |
| `g->unify_funarg1pos` | 2 | First argument position. |
| `g->unify_funarg2pos` | 3 | Second argument position. |
| `g->unify_maxuseterms` | 0 | Optional maximum number of compared fields; 0 means no limit. |
| `g->unify_samelen` | usually true | Whether compared records must have the same length. |

Do not feed field 0 to these algorithms in the normal build. `USE_TERM_META` is
not enabled, so field 0 is a reserved slot, not a meaningful subterm.

## 2. Variable Storage

Variables are immediate encoded values. `decode_var(x)` is the full index into
`g->varbanks`, including the bank offset.

`g->varbanks` is allocated as a `vec`, but unification treats it as a raw array
of `NROF_VARBANKS * NROF_VARSINBANK` cells. `wr_clear_all_varbanks(g)` writes
`UNASSIGNED` from slot 0 onward and intentionally destroys the normal vec length
header. A newly allocated varbank is therefore not ready for matching until it
has been cleared.

The five banks are:

| Bank | Purpose |
| --- | --- |
| 0 | Input/passive clauses. |
| 1 | Given-clause renamed variables. |
| 2 | Active-clause renamed variables. |
| 3 | Derived-clause renamed variables. |
| 4 | Temporary rename area; should remain unassigned. |

`g->varstack` is a `cvec` of addresses of assigned varbank cells. Its layout is:

```text
varstack[0] = vector length
varstack[1] = next free position, 2 when empty
varstack[2...] = addresses of varbank cells that have been assigned
```

`SETVAR(x, y, vb, vstk, vc)` writes `vb[decode_var(x)] = y` and pushes the cell
address to the varstack. Clearing the stack writes `UNASSIGNED` back to each
pushed cell and resets `varstack[1]`.

## 3. The Three Core Relations

Use the relation that matches the intended semantics:

| Function | Relation | Bindings |
| --- | --- | --- |
| `wr_equal_term(g, x, y, 1)` / `WR_EQUAL_TERM` | Structural equality | Creates no bindings. Variables compare as literal encoded values. |
| `wr_match_term(g, x, y, 1)` | One-way matching: `x` is the general pattern, `y` is the instance | Binds variables from `x` only. |
| `wr_unify_term(g, x, y, 1)` | Symmetric unification | Binds variables on either side. |

`uniquestrflag == 1` is the normal reasoner setting. It assumes URI/string
values are interned and therefore long strings compare equal only when their
encoded values are the same. The non-unique string path performs slower type,
language, and byte comparisons and is not the normal in-database path.

Functor equality and hashes are only prefilters. If two atoms share a functor,
they may still differ in arguments. Use equality, matching, or unification for
the final check.

## 4. Structural Equality

`wr_equal_term()` and the `WR_EQUAL_TERM` macro test structural equality without
consulting or changing variable bindings.

Important behavior from `unify.c`:

1. If `x == y`, equality succeeds immediately.
2. For small integers, interned long strings with `uniquestrflag == 1`, and
   several immediate values, different encoded values are immediately unequal.
3. Pointer-like values must have compatible low-bit pointer type tags.
4. Full integers compare by fetched word value.
5. Full doubles compare by decoded double value.
6. Short strings compare by their fixed `SHORTSTR_SIZE` bytes.
7. Long strings compare by encoded value when `uniquestrflag == 1`; otherwise
   they compare type, language/prefix, length, and bytes.
8. Data records compare field-by-field from `g->unify_firstuseterm` to the
   configured end position.

`WR_EQUAL_TERM` is the fast macro version. It requires a local variable named
`eqencx` and falls through to `wr_equal_term_macroaux()` for complex cases.
Use it only in code that follows that convention.

`wr_equal_mod_vars_term()` is a related structural test that treats variables
with the same variable number modulo `NROF_VARSINBANK` as equal. It is used by
propositional/instantiation duplicate checks where the bank identity should be
ignored.

Equality does not dereference current varbank bindings. If a value may contain
bound variables and you want instantiated equality, build or substitute first,
or use matching/unification as appropriate.

## 5. One-Way Matching

`wr_match_term(g, x, y, 1)` tests whether the general term `x` matches the
specific term `y`. It returns true iff there is a substitution for variables in
`x` that makes `x` structurally equal to `y`.

Core behavior:

1. `wr_match_term()` sets `g->tmp_unify_vc = &g->varstack[1]` and calls
   `wr_match_term_aux()`.
2. If `x` is an unassigned variable, the matcher binds `x` to `y` and succeeds.
3. If `x` is an already assigned variable, the stored value must be structurally
   equal to `y` under `WR_EQUAL_TERM`.
4. If `x` is not a variable and not a data record, it must be structurally equal
   to `y`.
5. If `x` is a data record and `y` is not, matching fails.
6. If both are records, matching recurses over configured term fields.

Matching does not bind variables occurring only in `y`. If `y` contains
variables, they are treated as values that variables in `x` may be bound to.
This is deliberate and is different from full unification.

`wr_match_term_aux()` does not initialize `g->tmp_unify_vc`; callers using the
auxiliary function directly must set it first. Recursive subsumption uses this
lower-level entry point for backtracking.

## 6. Full Unification

`wr_unify_term(g, x, y, 1)` is symmetric. It can bind variables on either side
and uses occurs-check machinery to avoid cyclic substitutions.

Core behavior:

1. `wr_unify_term()` sets `g->tmp_unify_vc = &g->varstack[1]`, enables
   occurs-check state, and calls `wr_unify_term_aux()`.
2. If either side is a variable, the current value is read through `VARVAL_F`,
   following variable-to-variable chains.
3. If both sides become equal after dereferencing, unification succeeds.
4. An unbound variable can be bound to a primitive, another variable, or a
   complex record.
5. Before binding a variable to a complex record, `wr_occurs_in()` checks that
   the variable does not occur inside that record under current bindings.
6. Two complex records unify by recursively unifying corresponding configured
   fields.
7. Two primitive constants unify only if `wr_equal_ptr_primitives()` or direct
   encoded equality says they are equal.

Occurs-check state is held in `g->tmp_unify_occcheck` and
`g->tmp_unify_do_occcheck`. The check is skipped for known-ground terms when
`USE_TERM_META` is available and the term metadata proves groundness.

Resolution, factoring, paramodulation, and some instantiation paths use full
unification. Typical call sites clear the varstack after every candidate, even
when the candidate fails.

## 7. Varstack Discipline

Every match or unification attempt may leave bindings behind. The caller owns
rollback.

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

For top-level candidate loops:

```c
if (wr_unify_term(g, xatom, yatom, 1)) {
  wr_process_resolve_result(g, xatom, xcl, yatom, ycl, xcl_as_active);
}
wr_clear_varstack(g, g->varstack);
```

`wr_clear_varstack_topslice(g, vs, vc)` clears only bindings pushed after a
snapshot. `wr_clear_varstack(g, vs)` clears all bindings on that varstack. Use
topslice rollback inside recursive search where earlier bindings are part of
the current branch.

Before any independent matching context outside the normal `wr_genloop()` setup,
ensure both structures exist and are clean:

```c
if (g->varbanks == NULL) {
  g->varbanks = wr_vec_new(g, NROF_VARBANKS * NROF_VARSINBANK);
}
if (g->varstack == NULL) {
  g->varstack = wr_cvec_new(g, NROF_VARBANKS * NROF_VARSINBANK);
}
CVEC_NEXT(g->varstack) = CVEC_START;
wr_clear_all_varbanks(g);
```

## 8. Clause Subsumption

Clause subsumption is built on one-way matching:

```text
cl1 subsumes cl2 iff every literal in cl1 can be matched to a distinct
same-polarity literal in cl2 under one shared substitution for cl1 variables.
```

Important stages in `Reasoner/subsume.c`:

1. `wr_given_cl_subsumed()` first tries fast active ground-unit subsumption.
2. It then scans `clactivesubsume` metablocks, first local and then KB-backed
   blocks from `r_kb_g(g)` when a shared KB is attached.
3. `wr_clmetablock_can_subsume()` checks cheap length, negative-length,
   ground-length, size, preflen, and prefix-bit filters.
4. Candidates that pass filters are tested by `wr_subsume_cl()`.
5. Recursive literal matching is performed by `wr_subsume_cl_aux()` using
   `wr_match_term_aux()`.
6. Each recursive branch snapshots `*(g->tmp_unify_vc)` and rolls back with
   `wr_clear_varstack_topslice()` when the branch fails or after it is consumed.

Polarity is part of literal metadata. Subsumption matches same-polarity
literals. Resolution uses opposite-polarity literals.

Fact clauses require special handling. A positive ground fact clause is the atom
record itself, not a wrapper with literal metadata. A negative unit, a non-ground
unit, or a multi-literal clause is a rule clause.

## 9. Equality and Matching in Other Modules

Common call patterns:

| Module | Pattern |
| --- | --- |
| `resolve.c` | Uses `wr_unify_term()` for binary resolution, factoring, and paramodulation candidates; clears varstack after each candidate. |
| `rewrite.c` | Uses `wr_match_term(g, rewrite_lhs, term, 1)` to apply rewrite rules; uses `wr_equal_term()` to avoid no-op rewrites. |
| `derive.c` | Uses `wr_equal_term()` for duplicate/tautology checks over half-built literals and clears varstack around derived-clause processing. |
| `arithinst.c` | Binds selected encoded variables directly with `SETVAR`, rebuilds the parent under substitution, and clears the whole varstack on every probe exit. It never binds constants or leaves candidate bindings live. |
| `hash.c` | Uses `wr_equal_term()` after hash-bucket lookup to confirm an indexed term/atom match. |
| `prop.c` | Uses `wr_equal_mod_vars_term()` for equality modulo variable-bank differences. |
| `history.c` | Uses `wr_equal_term()` when comparing answers and proof terms. |

This division is intentional: hash equality, literal metadata, and functor
matching narrow the candidate set, but the final semantic relation is still one
of structural equality, one-way matching, or full unification.

## 10. Choosing the Right Operation

Use this rule of thumb:

| Situation | Operation |
| --- | --- |
| Same fully built ground or already-normalized term? | `WR_EQUAL_TERM` or `wr_equal_term()`. |
| Potential subsumer against a more specific clause/literal? | `wr_match_term(g, general, instance, 1)`. |
| Resolution, factoring, or paramodulation where either side may bind? | `wr_unify_term()`. |
| Rewrite left-hand side against a target term? | `wr_match_term(rewrite_lhs, target)`. |
| Duplicate check where variable bank should not matter? | `wr_equal_mod_vars_term()`. |
| Only need to preselect candidates? | Functor/hash/meta checks, followed by one of the above. |

A common bug is to compare predicate/functor symbols and assume the literals are
the same. `p(a)` and `p(b)` have the same predicate symbol. They are not equal,
one does not match the other unless variables allow it, and they do not unify
unless their arguments do.

## 11. Failure Modes to Check in Review

1. Matching called with arguments reversed. The general side must be first.
2. Full unification used where one-way subsumption matching is required.
3. Equality used on terms that still contain bound variables instead of building
   the instantiated term first.
4. Varstack not restored after a probe, contaminating later candidates.
5. Fresh varbanks used before `wr_clear_all_varbanks()`.
6. `wr_match_term_aux()` called without setting `g->tmp_unify_vc`.
7. Fact clauses accessed with rule-clause literal macros.
8. Literal polarity read from the atom instead of from rule-clause metadata.
9. Atom/term field 0 treated as a subterm.
10. Hash/functor/meta prefilters treated as final proof of equality,
    matchability, or unifiability.
11. Terms from the wrong database base decoded through `g->db`; this is
    especially dangerous in shared-KB runs.
12. Bindings intentionally kept across sequential matches but then accidentally
    cleared before the shared substitution is consumed.
