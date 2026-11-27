% The candidates for unification (strategy key ti_selftest 3), as the
% strong cut uses them: the walk binds nothing, a variable of the query
% takes any one stored term, and each candidate is tested with
% wr_unify_term. ti_query(T,E): E is yes when a stored atom unifies with T,
% none when no stored atom does. The query is renamed apart from the
% stored atoms. Every stored atom that unifies with a subterm of the input
% must be a candidate.
cnf(u1,axiom, p(a)).
cnf(u2,axiom, q(X)).
cnf(u3,axiom, r(X,X)).
cnf(u4,axiom, s(f(Y),b)).
cnf(u5,axiom, t(X,f(X))).
cnf(q1,axiom, ti_query(p(Z), yes)).
cnf(q2,axiom, ti_query(p(b), none)).
cnf(q3,axiom, ti_query(q(Z), yes)).
cnf(q4,axiom, ti_query(q(g(a)), yes)).
cnf(q5,axiom, ti_query(r(Z,a), yes)).
cnf(q6,axiom, ti_query(r(a,b), none)).
cnf(q7,axiom, ti_query(r(Z,Z), yes)).
cnf(q8,axiom, ti_query(s(Z,Z), none)).
cnf(q9,axiom, ti_query(s(Z,W), yes)).
cnf(q10,axiom, ti_query(t(Z,Z), none)).
cnf(q11,axiom, ti_query(t(a,f(a)), yes)).
cnf(q12,axiom, ti_query(t(Z,f(Z)), yes)).
