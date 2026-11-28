% The equation instance is needed inside a term of the goal.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = a | q(X)).
cnf(nqb, axiom, ~q(b)).
cnf(goal, negated_conjecture, f(b,b) != f(a,b)).
