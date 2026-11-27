% Arity, nesting, small integers and strings; a constant and a function with the
% same name; variants of one left side share one path (the newest wins).
% Every constant is its own token, its encoded gint. The TPTP parser makes
% a quoted string a URI, interned, so str("abc") is one gint everywhere.
% A real number and a large integer are one gint each (a 4-byte float and
% an integer of 61 bits in the gint), so num(2.5) of a query is the
% num(2.5) of the equation. The older num(X) = anynum matches the numbers
% with no equation of their own.
cnf(r0,axiom, num(X) = anynum).
cnf(r1,axiom, f(a) = r1).
cnf(r2,axiom, f(a,b) = r2).
cnf(r3,axiom, f(f(f(X))) = r3(X)).
cnf(r4,axiom, num(1) = one).
cnf(r5,axiom, num(2.5) = half).
cnf(r5b,axiom, num(5000000000) = big).
cnf(r6,axiom, str("abc") = s3).
cnf(r6b,axiom, str("a long string of more than eight bytes") = slong).
cnf(r7,axiom, a(b) = ab).
cnf(r8,axiom, d(X,g(Y)) = r8(X,Y)).
cnf(r9,axiom, d(Y,g(X)) = r9(Y,X)).
cnf(q1,axiom, ti_query(f(a), r1)).
cnf(q2,axiom, ti_query(f(a,b), r2)).
cnf(q3,axiom, ti_query(f(b), none)).
cnf(q4,axiom, ti_query(f(f(f(f(a)))), r3(f(a)))).
cnf(q5,axiom, ti_query(f(f(a)), none)).
cnf(q6,axiom, ti_query(num(1), one)).
cnf(q7,axiom, ti_query(num(2), anynum)).
cnf(q9,axiom, ti_query(str("abc"), s3)).
cnf(q10,axiom, ti_query(str("abd"), none)).
cnf(q11,axiom, ti_query(a(b), ab)).
cnf(q12,axiom, ti_query(a(a), none)).
cnf(q13,axiom, ti_query(d(c,g(e)), r9(c,e))).
cnf(q14,axiom, ti_query(str("a long string of more than eight bytes"), slong)).
cnf(q18,axiom, ti_query(num(7), anynum)).
cnf(q19,axiom, ti_query(num(2.5), half)).
cnf(q20,axiom, ti_query(num(2.6), anynum)).
cnf(q21,axiom, ti_query(num(5000000000), big)).
cnf(q22,axiom, ti_query(num(5000000001), anynum)).
