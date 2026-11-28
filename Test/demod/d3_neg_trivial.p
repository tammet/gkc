% The repair after the rewriting of a derived clause: a negative literal
% s != s is false and is removed. g(f(a)) != g(b) | p(c) is derived from the
% rule and q(a); f(a) = b makes its first literal g(b) != g(b), which is
% removed, leaving p(c). The problem has no proof: the search saturates.
cnf(eq, axiom, f(a) = b).
cnf(rule, axiom, g(f(X)) != g(b) | p(c) | ~q(X)).
cnf(fact, axiom, q(a)).
cnf(goal, negated_conjecture, ~r).
