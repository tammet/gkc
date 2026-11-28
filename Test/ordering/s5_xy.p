% X = Y with X in another literal.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = Y | q(X)).
cnf(nqb, axiom, ~q(b)).
cnf(pa, axiom, p(a)).
cnf(goal, negated_conjecture, ~p(c)).
