% Commutativity, needed below a function symbol of the goal.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(comm, axiom, f(X,Y) = f(Y,X)).
cnf(goal, negated_conjecture, g(f(a,b)) != g(f(b,a))).
