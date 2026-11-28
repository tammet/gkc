% The repair after the rewriting of a derived clause: a clause that the
% rewriting gives a literal and its negation is a tautology and is dropped.
% r(f(a)) | ~r(b) is derived from the rule and q(a); f(a) = b makes it
% r(b) | ~r(b). The problem has no proof: the search saturates.
cnf(eq, axiom, f(a) = b).
cnf(rule, axiom, r(f(X)) | ~r(b) | ~q(X)).
cnf(fact, axiom, q(a)).
cnf(goal, negated_conjecture, ~s).
