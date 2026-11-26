% The self test of the perfect discrimination tree (strategy key ti_selftest).
% A binding must be cleared on backtracking: the walk binds X:=d on the
% path of g(X,c), fails at c and backtracks; a binding left over would
% reject g(d,X) at e.
cnf(r1,axiom, g(X,c) = r1).
cnf(r2,axiom, g(d,X) = r2).
% The walk tries g(X,c) inside f before the variable Y of f(Y,b): it binds
% X:=d, fails at c and must clear X before it binds Y.
cnf(r3,axiom, f(g(X,c),a) = r3).
cnf(r4,axiom, f(Y,b) = r4(Y)).
cnf(q1,axiom, ti_query(g(d,e), r2)).
cnf(q2,axiom, ti_query(g(d,c), r2)).
cnf(q3,axiom, ti_query(g(e,c), r1)).
cnf(q4,axiom, ti_query(g(e,e), none)).
cnf(q5,axiom, ti_query(h(g(d,e)), none)).
cnf(q6,axiom, ti_query(f(g(d,e),b), r4(g(d,e)))).
cnf(q7,axiom, ti_query(f(g(d,c),a), r3)).
