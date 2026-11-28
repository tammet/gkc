# Tests of the term ordering

gkc orders the two sides of an equation with `wr_order_eqterms`
(Reasoner/strat.c). The result decides which side may rewrite and which
sides paramodulation uses.

o1_cases.p is the input of the self test of the ordering (strategy key
`order_selftest`, see Doc/strategy_reference.md). Each `ord(A,B,R)` clause
states the result for `A` against `B`, worked out by hand from the code:
1 `A` greater, 2 `B` greater, 3 neither (also for equal terms). `ordnot3`
states a pair that the order of two symbols decides; the order of two
symbols is the order of their gints, which the input does not fix. The
test then checks properties over all terms of the file and their
subterms: antisymmetry, transitivity, the subterm property, totality on
ground pairs, stability under bindings of one variable (to ground terms,
terms with variables and a variable), the agreement of a comparison under
bindings with the comparison of the built instances, and monotony in the
`ctx` contexts. Test/regress.py runs it (case `order_cases`).

The test also prints one line `ORD_MANYVARS`: a term with more distinct
variables than the counted variable lists of the ordering hold. Its result
is printed and not checked by the regress case.
