% Rewriting with an unorientable equation (strategy key demod_unoriented):
% h is symmetric in its first and third arguments, and f is commutative.
% Each ground term has one normal form under the oriented instances.
cnf(sym, axiom, h(X,Y,Z) = h(Z,Y,X)).
cnf(comm, axiom, f(X,Y) = f(Y,X)).
cnf(fact, axiom, q(h(f(a,c),b,f(b,a)))).
cnf(goal, negated_conjecture, ~q(h(f(a,b),b,f(c,a)))).
