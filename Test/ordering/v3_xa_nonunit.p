% The equation X = a comes from a non-unit clause by resolution.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = a | q(X)).
cnf(nq, axiom, ~q(X)).
cnf(pb, axiom, p(b)).
cnf(goal, negated_conjecture, ~p(c)).
