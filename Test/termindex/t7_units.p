% The tree over the atoms of unit clauses (strategy key ti_selftest 2), as
% the full unit cut and the full unit subsumption use it. ti_query(T,E): E
% is yes when a stored atom matches T, none when no stored atom does.
% A unit with a repeated variable matches only equal arguments; one unit
% matches two literals with different bindings, each in a lookup of its own.
cnf(u1,axiom, p(X,X)).
cnf(u2,axiom, ~q(X,b)).
cnf(u3,axiom, r(f(X),g(Y,X))).
cnf(u4,axiom, s(a)).
cnf(u5,axiom, X = f(X)).
cnf(q1,axiom, ti_query(p(a,b), none)).
cnf(q2,axiom, ti_query(p(a,a), yes)).
cnf(q3,axiom, ti_query(p(f(Y),f(Y)), yes)).
cnf(q4,axiom, ti_query(q(a,b), yes)).
cnf(q5,axiom, ti_query(q(c,b), yes)).
cnf(q6,axiom, ti_query(q(c,a), none)).
cnf(q7,axiom, ti_query(r(f(a),g(b,a)), yes)).
cnf(q8,axiom, ti_query(r(f(a),g(b,b)), none)).
cnf(q9,axiom, ti_query(s(b), none)).
cnf(q10,axiom, ti_query(c = f(c), yes)).
cnf(q11,axiom, ti_query(c = f(d), none)).
