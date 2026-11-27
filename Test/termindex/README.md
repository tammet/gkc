# Tests of the term index

The term index (Reasoner/termindex.c) is a perfect discrimination tree. gkc
uses it to find the demodulators that rewrite a term.

The files t1-t5 are inputs for the self test of the index (strategy key
`ti_selftest`, see Doc/strategy_reference.md):

* t1_backtrack.p: a binding is cleared on backtracking;
* t2_repeated.p: repeated variables;
* t3_children.p: nodes with more children than the array in the node (a
  skip list);
* t4_query_vars.p: variables of the query term;
* t6_same_cells.p: a variable of a rule and a variable of a query with
  the same name, so the same variable cell; the right side is built from
  the values of the walk;
* t5_mixed.p: arity, nesting, small integers, quoted strings (interned
  URIs), variants. No floats and no large integers until their encoders
  give one encoding per value (Doc/DATA_REPRESENTATION.md, section 1).

Each `ti_query(T,E)` clause gives a term and its expected rewrite. The test
also compares the tree with a plain match on every subterm of the input.
Test/regress.py runs these files (cases `ti_*` and `ucm_*`).

t7_units.p is the self test with `ti_selftest` 2: the stored terms are the
atoms of the unit clauses, as the full unit cut and the full unit
subsumption use them (keys `unit_cut_match`, `unit_subsume_match`).
u1_cut_two.p and u2_no_cut_repeated.p run the prover with these keys: a
unit cuts two literals of a derived clause with different bindings; a unit
with a repeated variable must not cut, so there is no proof.

t8_compress.p tests the compressed form of the tree (strategy key
`tree_compress` 1): prefixes split at the first, a middle and the last
token, a node split more than once, a repeated variable in a prefix. Every
file runs on both forms (regress cases `ti_c_*` for the compressed form).

t9_unify.p is the self test with `ti_selftest` 3: the candidates for
unification that the strong cut uses (a query variable against a stored
constant and a stored variable, a repeated query variable, an occurs
check), each tested with `wr_unify_term`; every stored atom that unifies
with a subterm of the input must be a candidate.

`selftest.sh` runs the self test on any input files:

    Test/termindex/selftest.sh [-b path/gkc] [-c] [-m 2|3] file...

`-c` tests the compressed form.
