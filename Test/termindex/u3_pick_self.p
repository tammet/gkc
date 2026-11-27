% The picked clause at its simplification (strategy keys unit_cut_match 2,
% unit_subsume_match 2): a picked unit is in its unit tree itself and must
% not cut or subsume itself. The unit p(f(a)) is rewritten to p(a) at its
% pick; the unit q(X) is picked as it is. A proof needs both.
cnf(e,axiom, f(a) = a).
cnf(u1,axiom, p(f(a))).
cnf(u2,axiom, q(X)).
cnf(c,axiom, ~p(a) | ~q(b) | r).
cnf(g,negated_conjecture, ~r).
