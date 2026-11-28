% Rewriting with an unorientable equation (strategy key demod_unoriented):
% commutativity of f. With the key on, the goal is rewritten to the same
% normal form on both sides, by the oriented instances of the equation.
cnf(comm, axiom, f(X,Y) = f(Y,X)).
cnf(fact, axiom, p(g(f(b,a)))).
cnf(goal, negated_conjecture, ~p(g(f(a,b)))).
