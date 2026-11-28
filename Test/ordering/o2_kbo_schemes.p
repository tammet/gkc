% The self test of the weights and the precedence of KBO in E's schemes
% (strategy keys kbo_weights "invfreqrank" and kbo_precedence "invfreq").
% The occurrences of each symbol in this file decide the scheme. The weight
% classes count all symbols, constants and predicates too, from the most
% frequent: a (28 occurrences), f (18), term and ordf (8), b (7), g (6), then
% c, h and ctx (2). So f weighs 20, g 50, h 60 (times 10 for the rank of the
% class); a constant and a variable weigh 10. The precedence: a < f < term <
% ordf < b < g < c < h < ctx (fewer occurrences is greater; then the arity;
% then the later first occurrence).
% ordf(A,B,R): the result R for A against B under this scheme (1 A greater,
% 2 B greater, 3 neither), computed by hand and checked by a separate program.
cnf(t1, axiom, term(f(a,a))).
cnf(t2, axiom, term(f(f(a,a),a))).
cnf(t3, axiom, term(f(a,b))).
cnf(t4, axiom, term(f(X,a))).
cnf(t5, axiom, term(f(b,a))).
cnf(t6, axiom, term(f(a,f(a,a)))).
% g is heavier than f: g(a) weighs 60, f(a,a) 40 (under equal weights: 2)
cnf(c1, axiom, ordf(g(a),f(a,a),1)).
% f(f(a,a),a) weighs 70, g(a) 60
cnf(c2, axiom, ordf(g(a),f(f(a,a),a),2)).
% h(a) weighs 70, f(a,f(a,a)) 70: h is greater in the precedence
cnf(c3, axiom, ordf(h(a),f(a,f(a,a)),1)).
% constants: a is smallest, c greatest
cnf(c4, axiom, ordf(a,b,2)).
cnf(c5, axiom, ordf(c,b,1)).
% equal weights and heads: the first different arguments decide (b is greater than a)
cnf(c6, axiom, ordf(f(a,b),f(b,a),2)).
% a variable: g(X) weighs 60, f(X,a) 40, X occurs once in both
cnf(c7, axiom, ordf(g(X),f(X,a),1)).
% the variable condition: f(X,X) has X twice, g(a) has no variable; g(a) 60, f(X,X) 40
cnf(c8, axiom, ordf(g(a),f(X,X),3)).
cnf(t7, axiom, term(h(c))).
cnf(t8, axiom, term(g(b))).
cnf(x1, axiom, ctx(f(Y,a),Y)).
cnf(x2, axiom, ctx(g(Y),Y)).
