% Two variables; one occurs in the other literal, one only in the equation side.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = Y | q(Y)).
cnf(nqa, axiom, ~q(a)).
cnf(pb, axiom, p(b)).
cnf(goal, negated_conjecture, ~p(c)).
