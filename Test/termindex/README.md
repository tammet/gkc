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
* t5_mixed.p: arity, nesting, small integers, quoted strings (interned
  URIs), variants. No floats and no large integers until their encoders
  give one encoding per value (Doc/DATA_REPRESENTATION.md, section 1).

Each `ti_query(T,E)` clause gives a term and its expected rewrite. The test
also compares the tree with a plain match on every subterm of the input.
Test/regress.py runs the five files (cases `ti_*`).

`selftest.sh` runs the self test on any input files:

    Test/termindex/selftest.sh [-b path/gkc] file...
