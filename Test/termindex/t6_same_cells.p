% A variable of a rule and a variable of a query with the same name: in the
% input they have the same number, so the same variable cell. The rewrite
% builds the right side from the values of the walk, not through the
% cells: f(X,Y) = Y rewrites f(a,X) to X (through the cells it would
% follow Y -> X -> a), and g(X) = h(X) rewrites g(k(X)) to h(k(X))
% (through the cells it would loop on X -> k(X)).
cnf(r1,axiom, f(X,Y) = Y).
cnf(r2,axiom, g(X) = h(X)).
cnf(r3,axiom, p(X,X,Y) = q(Y,X)).
cnf(q1,axiom, ti_query(f(a,X), X)).
cnf(q2,axiom, ti_query(g(k(X)), h(k(X)))).
cnf(q3,axiom, ti_query(p(Y,Y,X), q(X,Y))).
cnf(q4,axiom, ti_query(p(X,Y,X), none)).
