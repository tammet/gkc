% The other literal is refuted only by a general clause.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = a | q(X)).
cnf(nq, axiom, ~q(Y) | r(Y)).
cnf(nr, axiom, ~r(g(Z))).
cnf(pgb, axiom, p(g(b))).
cnf(goal, negated_conjecture, ~p(a)).
