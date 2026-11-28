% The variable of the equation also occurs in a negative literal.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = a | ~q(X)).
cnf(qb, axiom, q(b)).
cnf(qc, axiom, q(c)).
cnf(pb, axiom, p(b)).
cnf(goal, negated_conjecture, ~p(c)).
