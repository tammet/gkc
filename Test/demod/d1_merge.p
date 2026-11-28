% The repair after the rewriting of a derived clause: two literals that the
% rewriting makes equal are merged. p(f(a)) | p(b) is derived from the rule
% and q(a); f(a) = b rewrites it to p(b) | p(b), which becomes the unit p(b).
% The problem has no proof: the search saturates.
cnf(eq, axiom, f(a) = b).
cnf(rule, axiom, p(f(X)) | p(b) | ~q(X)).
cnf(fact, axiom, q(a)).
cnf(goal, negated_conjecture, ~r).
