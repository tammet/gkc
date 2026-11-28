% X = f(Y): every term equals a term f(...).
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = f(Y)).
cnf(pa, axiom, p(a)).
cnf(goal, negated_conjecture, ~p(b)).
