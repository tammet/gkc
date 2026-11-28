# Tests of the term ordering

gkc orders the two sides of an equation with `wr_order_eqterms`
(Reasoner/strat.c). The result decides which side may rewrite and which
sides paramodulation uses.

o1_cases.p is the input of the self test of the ordering (strategy key
`order_selftest`, see Doc/strategy_reference.md). The test runs under the
ordering of the key `term_ordering`: the regress cases `order_cases` (KBO,
the default) and `order_cases_weight_lex`. Under KBO the test also compares
every pair with a second, direct implementation of the definition. Each `ord(A,B,R)` clause
states the result for `A` against `B`, worked out by hand from the code:
1 `A` greater, 2 `B` greater, 3 neither (also for equal terms). `ordw` and
`ordk` state a result that holds only under `weight_lex` and only under KBO
(cases c24 and c24v). `ordnot3`
states a pair that the order of two symbols decides; the order of two
symbols is the order of their gints, which the input does not fix. The
test then checks properties over all terms of the file and their
subterms: antisymmetry, transitivity, the subterm property, totality on
ground pairs, stability under bindings of one variable (to ground terms,
terms with variables and a variable), the agreement of a comparison under
bindings with the comparison of the built instances, and monotony in the
`ctx` contexts. Test/regress.py runs it (case `order_cases`).

The test also checks one built case, the line `ORD_MANYVARS`: a term with
more distinct variables than the counted variable lists of the ordering
hold. The variable condition cannot be tested then, and the result must be
3; it counts as one of the cases.

The other files are minimal problems with equality. Each is unsatisfiable
(the status comment: E and Vampire agree).

* u1 to u4: unorientable equations and the test of the ordering after
  unification. u5: the test of the "into" literal (`para_into_ordered` 2)
  leaves out a step into the smaller side of an equation.
* v1 to v10: an equation with a variable side (`X = a`, `X = Y`,
  `X = f(Y)`). Paramodulation uses a variable side as the side it starts
  from, in both directions (from the given clause and into it).
* s1 to s10: the variable of such an equation also occurs in another
  literal of its clause (positive, negative, under a function symbol, in
  the negated conjecture). gkc leaves out the steps from such a variable
  side, as Vampire does. With the strategy `negative_pref`, s1 and s4 end
  without a proof by this condition (regress cases with the expected result
  noproof); the automatic strategy proves them.

The regress cases `ord_*` run all of them with the automatic strategy, and
some with a fixed strategy. With the strategy `query_focus` (set of
support), v2, v3, v4, v6 and s1 to s4 end without a proof: their proofs
need a step between two clauses that are not in the set of support.
