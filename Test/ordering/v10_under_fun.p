% The variable side against a term below a function symbol.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(eq, axiom, X = a).
cnf(pfb, axiom, p(f(b))).
cnf(goal, negated_conjecture, ~p(f(c))).
