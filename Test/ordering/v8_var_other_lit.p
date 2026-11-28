% The variable of the equation also occurs in another literal of its clause.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = a | q(X)).
cnf(nqb, axiom, ~q(b)).
cnf(pb, axiom, p(b)).
cnf(goal, negated_conjecture, ~p(a)).
