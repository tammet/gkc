% Defect 3 of the study of the strategies (document 05): with query_focus (set of support)
% the proof needs X = a used into the goal and a step that the ordering leaves out.
% With query_focus it is proved only with sos_ordering "none".
% Status (E 3.5.11 and Vampire 5.1.0 agree): Unsatisfiable
cnf(c1, axiom, X = a).
cnf(c2, negated_conjecture, b != c).
