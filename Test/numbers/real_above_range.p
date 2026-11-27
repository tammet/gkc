% 1.0e39 written out is above the range of a 4-byte float: a parse error.
cnf(a,axiom, p(1000000000000000000000000000000000000000.0)).
cnf(b,negated_conjecture, ~p(1.0)).
