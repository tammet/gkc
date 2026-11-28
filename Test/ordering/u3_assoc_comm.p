% Associativity and commutativity: a permutation of three arguments.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(assoc, axiom, f(f(X,Y),Z) = f(X,f(Y,Z))).
cnf(comm, axiom, f(X,Y) = f(Y,X)).
cnf(goal, negated_conjecture, f(a,f(b,c)) != f(c,f(b,a))).
