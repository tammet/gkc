% Commutativity in both directions: f(a,b) and f(c,d) must both be turned.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(comm, axiom, f(X,Y) = f(Y,X)).
cnf(fact, axiom, p(f(a,b),f(d,c))).
cnf(goal, negated_conjecture, ~p(f(b,a),f(c,d))).
