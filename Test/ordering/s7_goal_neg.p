% The clause with the equation is the negated conjecture; the other literal is negative.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(pb, axiom, p(b)).
cnf(npa, axiom, ~p(a)).
cnf(qb, axiom, q(b)).
cnf(goal, negated_conjecture, X = a | ~q(X)).
