% The self test of the term ordering wr_order_eqterms (strategy key
% order_selftest). ord(A,B,R): the result R for A against B, worked out by
% hand from the code: 1 A greater, 2 B greater, 3 neither (also for equal
% terms), under both orderings of the key term_ordering; ordw and ordk hold
% only under weight_lex and under KBO. Every symbol and every variable weighs 10. ordnot3(A,B): a pair
% that the order of two symbols decides, so 1 or 2. term(T) and ctx(C,H) only
% add terms and contexts to the property tests (see Reasoner/strat.c).
%
% a variable against a variable, a constant, a compound term
cnf(c1, axiom, ord(X,Y,3)).
cnf(c2, axiom, ord(X,X,3)).
cnf(c3, axiom, ord(X,a,3)).
cnf(c4, axiom, ord(a,X,3)).
cnf(c5, axiom, ord(X,f(X),2)).
cnf(c6, axiom, ord(f(X),X,1)).
cnf(c7, axiom, ord(g(h(X,a)),X,1)).
cnf(c8, axiom, ord(X,f(Y),3)).
% a constant against a compound term: the compound term is greater
cnf(c9, axiom, ord(a,f(a),2)).
cnf(c10, axiom, ord(f(X),a,1)).
% two small integers: the larger gint is the smaller constant
cnf(c11, axiom, ord(1,2,1)).
cnf(c12, axiom, ord(2,1,2)).
cnf(c13, axiom, ord(f(1),f(2),1)).
% a quoted string is a constant: smaller than a compound term
cnf(c14, axiom, ord("abc",f(a),2)).
cnf(c15, axiom, ordnot3("abc",a)).
% different weights, the variables of the lighter term in the heavier one
cnf(c16, axiom, ord(f(a,a),g(a),1)).
cnf(c17, axiom, ord(f(X,X),g(X),1)).
cnf(c18, axiom, ord(g(X),f(X,X),2)).
% different weights, a variable of the lighter term not in the heavier one
cnf(c19, axiom, ord(f(X,a),g(Y),3)).
% different weights, a variable more often in the lighter term
cnf(c20, axiom, ord(f(g(X),Y),h(X,X),3)).
% equal weights, decided by the record length (fewer fields is smaller)
cnf(c21, axiom, ord(f(a,a),g(g(a)),1)).
% equal weights and lengths, decided by the symbols
cnf(c22, axiom, ordnot3(f(a),g(a))).
cnf(c23, axiom, ordnot3(f(a,b),f(b,a))).
% equal weights, decided at a deeper argument. The two orderings differ:
% ordw(A,B,R) is the result under weight_lex, ordk(A,B,R) under KBO.
% weight_lex: the argument with fewer fields is smaller (not by weight);
% KBO: the first arguments are compared by KBO (weight 40 against 30)
cnf(c24, axiom, ordw(f(g(g(g(a))),a),f(h(a,a),g(a)),2)).
cnf(c24k, axiom, ordk(f(g(g(g(a))),a),f(h(a,a),g(a)),1)).
% the same with a variable: KBO puts the left first argument above the right
% one, but X occurs more often on the right, so KBO gives 3
cnf(c24v, axiom, ordw(f(g(g(g(X))),X),f(h(X,X),g(X)),2)).
cnf(c24vk, axiom, ordk(f(g(g(g(X))),X),f(h(X,X),g(X)),3)).
cnf(c25, axiom, ord(k(g(g(a))),k(h(a,a)),2)).
% equal weights, a variable met in the comparison
cnf(c26, axiom, ord(f(X,a),f(Y,a),3)).
cnf(c27, axiom, ord(f(X,1),f(X,2),1)).
cnf(c28, axiom, ord(f(X,1),f(2,X),3)).
% a repeated variable
cnf(c29, axiom, ord(f(X,X),f(X,Y),3)).
cnf(c30, axiom, ord(f(X,Y),f(X,X),3)).
% more terms and contexts for the property tests
cnf(t1, axiom, term(f(g(a),h(b,c)))).
cnf(t2, axiom, term(h(f(X,a),g(Y)))).
cnf(t3, axiom, term(g(f(a,g(b))))).
cnf(t4, axiom, term(f(h(X,Y),g(g(Z))))).
cnf(t5, axiom, term(h(g(X),g(X)))).
cnf(x1, axiom, ctx(f(H,a),H)).
cnf(x2, axiom, ctx(g(H),H)).
cnf(x3, axiom, ctx(h(b,f(H,X)),H)).
