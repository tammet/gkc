% Repeated variables: a later occurrence of a variable must be equal to
% its binding.
cnf(r1,axiom, f(X,Y) = r2).
cnf(r2,axiom, f(X,X) = r1).
cnf(r3,axiom, h(X,Y,X) = s(Y,X)).
cnf(r4,axiom, k(g(X),X) = X).
cnf(q1,axiom, ti_query(f(a,b), r2)).
cnf(q2,axiom, ti_query(f(a,a), r1)).
cnf(q3,axiom, ti_query(f(g(a),g(a)), r1)).
cnf(q4,axiom, ti_query(f(g(a),g(b)), r2)).
cnf(q5,axiom, ti_query(h(a,b,a), s(b,a))).
cnf(q6,axiom, ti_query(h(a,b,b), none)).
cnf(q7,axiom, ti_query(h(g(a,b),c,g(a,b)), s(c,g(a,b)))).
cnf(q8,axiom, ti_query(k(g(f(a,b)),f(a,b)), f(a,b))).
cnf(q9,axiom, ti_query(k(g(a),b), none)).
