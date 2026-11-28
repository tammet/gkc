% The variable occurs in a positive and in a negative literal of the clause.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = a | q(X) | ~r(X)).
cnf(rb, axiom, r(b)).
cnf(rc, axiom, r(c)).
cnf(nqb, axiom, ~q(b)).
cnf(nqc, axiom, ~q(c)).
cnf(pb, axiom, p(b)).
cnf(goal, negated_conjecture, ~p(c)).
