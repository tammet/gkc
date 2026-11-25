% E5 regression: the only resolvent r(Xn) gets a term of 2^25 leaves, more than
% derived_termbuf holds (10,000,000 words). gkc drops it and the search ends
% without a proof; gkc 1.0.4 ended the run with "cannot allocate memory".
cnf(fact,axiom, q(X1,X2,X3,X4,X5,X6,X7,X8,X9,X10,X11,X12,X13,X14,X15,X16,X17,X18,X19,X20,X21,X22,X23,X24,X25,f(X0,X0),f(X1,X1),f(X2,X2),f(X3,X3),f(X4,X4),f(X5,X5),f(X6,X6),f(X7,X7),f(X8,X8),f(X9,X9),f(X10,X10),f(X11,X11),f(X12,X12),f(X13,X13),f(X14,X14),f(X15,X15),f(X16,X16),f(X17,X17),f(X18,X18),f(X19,X19),f(X20,X20),f(X21,X21),f(X22,X22),f(X23,X23),f(X24,X24))).
cnf(rule,axiom, ~q(Z1,Z2,Z3,Z4,Z5,Z6,Z7,Z8,Z9,Z10,Z11,Z12,Z13,Z14,Z15,Z16,Z17,Z18,Z19,Z20,Z21,Z22,Z23,Z24,Z25,Z1,Z2,Z3,Z4,Z5,Z6,Z7,Z8,Z9,Z10,Z11,Z12,Z13,Z14,Z15,Z16,Z17,Z18,Z19,Z20,Z21,Z22,Z23,Z24,Z25) | r(Z25)).
cnf(goal,negated_conjecture, ~r(c)).
