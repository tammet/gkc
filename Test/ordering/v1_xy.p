% An equation with two variable sides: every two terms are equal.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = Y).
cnf(pa, axiom, p(a)).
cnf(goal, negated_conjecture, ~p(b)).
