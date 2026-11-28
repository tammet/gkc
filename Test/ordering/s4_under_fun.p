% The variable occurs under a function symbol in the other literal.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = a | q(f(X))).
cnf(nqb, axiom, ~q(f(b))).
cnf(nqc, axiom, ~q(f(c))).
cnf(pb, axiom, p(b)).
cnf(goal, negated_conjecture, ~p(c)).
