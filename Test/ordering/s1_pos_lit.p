% The variable of the equation also occurs in a positive literal; no negative literal.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = a | q(X)).
cnf(nqb, axiom, ~q(b)).
cnf(nqc, axiom, ~q(c)).
cnf(pb, axiom, p(b)).
cnf(goal, negated_conjecture, ~p(c)).
