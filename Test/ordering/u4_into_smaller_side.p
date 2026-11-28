% Paramodulation into the smaller side of an equation: k(a) = a with
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
% g(k(X)) = c; the goal needs a step into g(k(a)).
cnf(e1, axiom, k(a) = a).
cnf(e2, axiom, g(k(X)) = c).
cnf(goal, negated_conjecture, g(a) != c).
