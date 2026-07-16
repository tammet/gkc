# GKC arithmetic regression corpus

This directory contains focused calculator and bounded numeric-instantiation
regressions. They are tests rather than tutorial examples: the maintained
driver pins mode boundaries, expected proofs, controls, and deliberately
out-of-scope cases.

Run the suite from the repository root after changes to arithmetic calculation,
numeric candidate generation, strategy defaults, clause construction, history
printing, or shared-memory allocation:

```sh
python3 Test/arithmetic/check_arithmetic_instantiation.py
```

The two user-facing examples remain in `Examples/`:

- `Examples/arithmetic_instantiation_apples.txt` demonstrates the default
  conservative one-variable mode and returns `$ans(8)`;
- `Examples/arithmetic_instantiation_product.txt` demonstrates a bounded
  two-variable problem which requires explicit mode 2.

The numbered regression cases cover:

| Files | Expected boundary |
| --- | --- |
| `arithmetic_ground_regression*.txt` | Ground calculation proves with instantiation disabled. |
| `arithmetic_instantiation1-5.txt`, `7-10.txt`, `12`, `13`, `16` | Mode 0 does not prove; conservative mode 1 proves. |
| tutorial product example (former case 6) | Mode 1 does not prove; mode 2 proves. |
| tutorial apples example (former case 11) | Omitted setting defaults to mode 1; mode 0 does not prove. |
| `arithmetic_instantiation14*`, `15*` | Ordinary unification proves in both modes and needs no arithmetic instance. |
| `arithmetic_instantiation17*`, `18*` | Expected-hard boundary remains unproved in mode 2. |
| `arithmetic_overflow_safety.txt` | Ground overflow/conversion edges are safe and contradictory as written. |

Mode 1 is the compiled default. Explicit `"arith_instantiation":0` is the
negative-control/opt-out setting; mode 2 remains opt-in.
