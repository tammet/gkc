% The equation with a variable side is the negated conjecture.
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(pb, axiom, p(b)).
cnf(npc, axiom, ~p(c)).
cnf(goal, negated_conjecture, X = a).
