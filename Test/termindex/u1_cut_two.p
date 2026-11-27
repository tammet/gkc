% The full unit cut (strategy key unit_cut_match): the derived clause
% ~p(a) | ~p(b) | q has two literals that the unit p(X) cuts, each with a
% binding of its own; the unit tables find neither (they need an equal
% literal).
cnf(u,axiom, p(X)).
cnf(c1,axiom, ~s | ~p(a) | ~p(b) | q).
cnf(c2,axiom, s).
cnf(g,negated_conjecture, ~q).
