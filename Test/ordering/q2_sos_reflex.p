% Defect 3 of document 05, second example. With query_focus no value of sos_ordering
% proves it: equality resolution uses only the first literal of a clause with
% query_focus, and f(X) != f(a) is the second literal. The other strategies prove it.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(c1, negated_conjecture, p(a) | f(X) != f(a)).
cnf(c2, axiom, ~p(a) | q(b)).
cnf(c3, axiom, ~q(b)).
