% Many children of one node: the array in the node, then the skip list.
% The older p(X) = rv matches only the constants with no rule of their own.
cnf(rv,axiom, p(X) = rv).
cnf(r1,axiom, p(c1) = r1).
cnf(r2,axiom, p(c2) = r2).
cnf(r3,axiom, p(c3) = r3).
cnf(r4,axiom, p(c4) = r4).
cnf(r5,axiom, p(c5) = r5).
cnf(r6,axiom, p(c6) = r6).
cnf(r7,axiom, p(c7) = r7).
cnf(r8,axiom, p(c8) = r8).
cnf(r9,axiom, p(c9) = r9).
cnf(r10,axiom, p(c10) = r10).
cnf(r11,axiom, p(c11) = r11).
cnf(r12,axiom, p(c12) = r12).
cnf(r13,axiom, p(c13) = r13).
cnf(r14,axiom, p(c14) = r14).
cnf(r15,axiom, p(c15) = r15).
cnf(r16,axiom, p(c16) = r16).
cnf(r17,axiom, p(c17) = r17).
cnf(r18,axiom, p(c18) = r18).
cnf(r19,axiom, p(c19) = r19).
cnf(r20,axiom, p(c20) = r20).
cnf(m1,axiom, q(f1(X),c1) = s1(X)).
cnf(m2,axiom, q(f2(X),c2) = s2(X)).
cnf(m3,axiom, q(f3(X),c3) = s3(X)).
cnf(m4,axiom, q(f4(X),c4) = s4(X)).
cnf(m5,axiom, q(f5(X),c5) = s5(X)).
cnf(m6,axiom, q(f6(X),c6) = s6(X)).
cnf(m7,axiom, q(f7(X),c7) = s7(X)).
cnf(m8,axiom, q(f8(X),c8) = s8(X)).
cnf(m9,axiom, q(f9(X),c9) = s9(X)).
cnf(m10,axiom, q(f10(X),c10) = s10(X)).
cnf(m11,axiom, q(f11(X),c11) = s11(X)).
cnf(m12,axiom, q(f12(X),c12) = s12(X)).
cnf(m13,axiom, q(f13(X),c13) = s13(X)).
cnf(m14,axiom, q(f14(X),c14) = s14(X)).
cnf(m15,axiom, q(f15(X),c15) = s15(X)).
cnf(m16,axiom, q(f16(X),c16) = s16(X)).
cnf(m17,axiom, q(f17(X),c17) = s17(X)).
cnf(m18,axiom, q(f18(X),c18) = s18(X)).
cnf(m19,axiom, q(f19(X),c19) = s19(X)).
cnf(m20,axiom, q(f20(X),c20) = s20(X)).
cnf(q1,axiom, ti_query(p(c1), r1)).
cnf(q4,axiom, ti_query(p(c4), r4)).
cnf(q7,axiom, ti_query(p(c7), r7)).
cnf(q10,axiom, ti_query(p(c10), r10)).
cnf(q13,axiom, ti_query(p(c13), r13)).
cnf(q16,axiom, ti_query(p(c16), r16)).
cnf(q19,axiom, ti_query(p(c19), r19)).
cnf(q30,axiom, ti_query(p(c21), rv)).
cnf(q31,axiom, ti_query(p(g(c1)), rv)).
cnf(n2,axiom, ti_query(q(f2(c2),c2), s2(c2))).
cnf(n6,axiom, ti_query(q(f6(c6),c6), s6(c6))).
cnf(n10,axiom, ti_query(q(f10(c10),c10), s10(c10))).
cnf(n14,axiom, ti_query(q(f14(c14),c14), s14(c14))).
cnf(n18,axiom, ti_query(q(f18(c18),c18), s18(c18))).
cnf(n30,axiom, ti_query(q(f2(a),c3), none)).
cnf(n31,axiom, ti_query(q(f21(a),c21), none)).
