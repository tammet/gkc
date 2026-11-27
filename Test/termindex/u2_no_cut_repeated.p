% A unit with a repeated variable must not cut a literal with two
% different arguments: p(X,X) does not cut ~p(a,b), so there is no proof.
cnf(u,axiom, p(X,X)).
cnf(c1,axiom, ~s | ~p(a,b) | q).
cnf(c2,axiom, s).
cnf(g,negated_conjecture, ~q).
