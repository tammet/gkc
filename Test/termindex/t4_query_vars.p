% Variables of the query term are constants for the walk; a stored
% variable can bind to them. The variables of a stored term and of a query
% term can have the same numbers.
cnf(r1,axiom, m(X,a) = X).
cnf(r2,axiom, q2(X,X) = t).
cnf(r3,axiom, n(a) = u).
cnf(r4,axiom, w(X,Y) = v(Y,X)).
cnf(q1,axiom, ti_query(m(Y,a), Y)).
cnf(q2,axiom, ti_query(q2(Y,Y), t)).
cnf(q3,axiom, ti_query(q2(Y,Z), none)).
cnf(q4,axiom, ti_query(n(Y), none)).
cnf(q5,axiom, ti_query(m(f(Y,Z),a), f(Y,Z))).
cnf(q6,axiom, ti_query(m(a,Y), none)).
cnf(q7,axiom, ti_query(w(Y,X), v(X,Y))).
