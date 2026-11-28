% Two steps with the variable side: both arguments of p.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = a).
cnf(pbc, axiom, p(b,c)).
cnf(goal, negated_conjecture, ~p(a,a)).
