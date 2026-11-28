% X = Y in a non-unit clause.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = Y | r).
cnf(nr, axiom, ~r).
cnf(pa, axiom, p(a)).
cnf(goal, negated_conjecture, ~p(b)).
