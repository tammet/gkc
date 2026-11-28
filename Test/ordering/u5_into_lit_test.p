% The test of the "into" literal (para_into_ordered 2): f(a) occurs on the smaller side
% g(f(a)) of the equation of eq2; the step into that side is left out (the counter
% stat_para_lit_order_skip). The literals p(a) are removed only after a few steps.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, f(a) = b | p(a)).
cnf(eq2, axiom, g(f(a)) = h(h(h(f(a)))) | p(a)).
cnf(p1, axiom, ~p(X) | p1(X)).
cnf(p2, axiom, ~p1(X) | p2(X)).
cnf(p3, axiom, ~p2(a)).
cnf(goal, negated_conjecture, g(b) != h(h(h(b)))).
