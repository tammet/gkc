% Three occurrences of the same real number and of the same large integer
% give one constant each: the clauses unify.
cnf(a,axiom, p(5000000000,2.5)).
cnf(b,axiom, ~p(5000000000,2.5) | q(2.5,5000000000)).
cnf(c,negated_conjecture, ~q(2.5,5000000000)).
