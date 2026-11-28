% An equation with a variable side and a constant side: every term equals a.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = a).
cnf(pb, axiom, p(b)).
cnf(goal, negated_conjecture, ~p(c)).
