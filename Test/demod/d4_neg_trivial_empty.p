% The repair after the rewriting of a derived clause: a derived clause whose
% only literal becomes s != s is the empty clause. g(f(a)) != g(b) is derived;
% f(a) = b makes it g(b) != g(b), which is removed: the clause is empty.
cnf(eq, axiom, f(a) = b).
cnf(rule, axiom, g(f(X)) != g(b) | ~q(X)).
cnf(goal, negated_conjecture, q(a)).
