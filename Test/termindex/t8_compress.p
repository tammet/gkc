% Compressed paths (strategy key tree_compress 1): a node holds a prefix of
% tokens, and a new stored term splits a prefix at its first different
% token. The same file runs on both forms of the tree.
% A split at the first token after the root (f with two arguments against
% f with three), at a middle token (b against c) and at the last token (d
% against e); a second and a third split at the same node (k(a,X) against
% k(b,X) and k(c,X)); a repeated variable in a prefix that is never split
% (h(X,a,X)), and a repeated variable in a prefix that is split (m(X,b,X)
% against m(X,c,Y)).
cnf(r1,axiom, f(a,b,d) = r1).
cnf(r2,axiom, f(a,c,d) = r2).
cnf(r3,axiom, f(a,b,e) = r3).
cnf(r4,axiom, f(a,b) = r4).
cnf(r5,axiom, k(a,X) = s5(X)).
cnf(r6,axiom, k(b,X) = s6(X)).
cnf(r7,axiom, k(c,X) = s7(X)).
cnf(r8,axiom, h(X,a,X) = s8(X)).
cnf(r9,axiom, m(X,b,X) = s9(X)).
cnf(r10,axiom, m(X,c,Y) = s10(X,Y)).
cnf(q1,axiom, ti_query(f(a,b,d), r1)).
cnf(q2,axiom, ti_query(f(a,c,d), r2)).
cnf(q3,axiom, ti_query(f(a,b,e), r3)).
cnf(q4,axiom, ti_query(f(a,b), r4)).
cnf(q5,axiom, ti_query(f(a,c,e), none)).
cnf(q6,axiom, ti_query(f(a,d), none)).
cnf(q7,axiom, ti_query(k(a,g(b)), s5(g(b)))).
cnf(q8,axiom, ti_query(k(b,a), s6(a))).
cnf(q9,axiom, ti_query(k(c,c), s7(c))).
cnf(q10,axiom, ti_query(k(d,c), none)).
cnf(q11,axiom, ti_query(h(g(a),a,g(a)), s8(g(a)))).
cnf(q12,axiom, ti_query(h(g(a),a,g(b)), none)).
cnf(q13,axiom, ti_query(m(a,b,a), s9(a))).
cnf(q14,axiom, ti_query(m(a,b,c), none)).
cnf(q15,axiom, ti_query(m(a,c,b), s10(a,b))).
