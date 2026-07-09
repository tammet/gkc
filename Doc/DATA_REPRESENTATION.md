# GKC Data Representation

This document describes the low-level data formats used by the gkc reasoner.
It should be read with `Reasoner/clterm.h`, `Reasoner/mem.h`, `Reasoner/unify.h`,
`Reasoner/glb.h`, `Reasoner/clmeta.h`, `Reasoner/clstore.h`,
`Reasoner/history.h`, `Reasoner/build.h`, `Db/dbdata.h`, and
`Db/dballoc.h` open nearby.

The architecture-level proof-search flow is documented in
`Doc/ARCHITECTURE.md`. Practical equality, one-way matching, full
unification, varstack, and subsumption guidance is in `Doc/TERM_MATCHING.md`.
This file focuses on the values, records, buffers, variable banks, and
shared-memory pointer rules those algorithms rely on.

## 1. Encoded Scalar Values

The base scalar type is `gint` (`Db/dballoc.h`). A `gint` can be an immediate
value or an encoded offset into a WhiteDB memory segment. Pointers into records
are usually represented as `gptr`, which is `gint *` (`Reasoner/types.h`). The
low bits of encoded values are type tags; `Db/dbdata.h` is the authoritative
source for the masks and shifts.

Common predicates and decoders:

| Predicate | Meaning | Decode or access |
| --- | --- | --- |
| `isvar(x)` | Immediate reasoner variable | `decode_var(x)` |
| `issmallint(x)` | Immediate small integer | `decode_smallint(x)` |
| `isdatarec(x)` | Offset to a data record | `decode_record(db, x)` |
| `islongstr(x)` | Offset to a long string or URI object | `wg_decode_str()`, `wg_decode_uri()` |
| `isfulldouble(x)` | Offset to a double object | `wg_decode_double()` |

Reasoner code normally assumes unique/interned URI and string values. For
ordinary in-database comparisons, pass `uniquestrflag == 1` to equality,
matching, and unification helpers.

Important low-bit encodings from `Db/dbdata.h`:

| Encoding | Low bits | Notes |
| --- | --- | --- |
| small integer | `...011` | Immediate value, shifted by `SMALLINTSHFT == 3`. |
| data record offset | `...000` | Used for clauses, atoms, terms, and ordinary records. |
| full integer offset | low two bits `01` | Points to word storage for integers that do not fit as smallints. |
| full double offset | `...010` | Points to doubleword storage. |
| long string / URI offset | `...100` | Interned string-like object. |
| short string offset | `...110` | Fixed-size short string object. |
| variable | low four bits `0111` | Immediate reasoner variable. |
| char/date/time/fixpoint/anonconst | low byte tags | Immediate non-pointer values. |

`isimmediatedata(x)` is true for `0` and non-pointer immediate values, but not
for full integers stored in the word area. Do not use pointer tests as a proxy
for term kind; records still need their meta bits checked.

Reasoner anonymous constants are immediate values created with
`encode_anonconst(n)`. `Db/dballoc.h` reserves ids for boolean/connective and
comparison/arithmetic symbols such as `ACONST_FALSE`, `ACONST_TRUE`,
`ACONST_NOT`, `ACONST_AND`, `ACONST_OR`, `ACONST_EQUAL`, `ACONST_LESS`,
`ACONST_PLUS`, `ACONST_MINUS`, `ACONST_MULTIPLY`, and `ACONST_DIVIDE`. These
are not URI records and should be tested as encoded constants, not decoded as
strings.

## 2. Offset Converters and Database Bases

All references stored in database records and reasoner indexes are offsets, not
raw process pointers. Use the converter family that matches the state you have:

| Macro | Meaning |
| --- | --- |
| `otp(db, offset)` | Offset to pointer using an explicit database base. |
| `pto(db, ptr)` | Pointer to offset using an explicit database base. |
| `rotp(g, offset)` | Offset to pointer using `g->db`. |
| `rpto(g, ptr)` | Pointer to offset using `g->db`. |

In a normal no-KB run, `g->db`, `g->local_db`, and `g->child_db` point to the
same segment. In a `-usekb` run, `g->db` is usually the shared KB base while
`g->local_db` is the writable local child segment.

That split is intentional: local records are encoded through `g->local_db`, and
`encode_kb_offset()` rebases their offsets so they are still usable through the
KB base. The rule is:

* Decode and compare with `g->db`.
* Allocate new run-time records, histories, strings, and computed constants
  through `g->local_db`.
* Do not write through a pointer inside `wg_in_attached_kb(ptr)`.

## 3. WhiteDB Segment Layout

A WhiteDB segment begins with `db_memsegment_header` (`Db/dballoc.h`) and then
contains areas for records, strings/URIs, words, doubles, list cells, reasoner
objects, indexes, locks, logging data, and bookkeeping.

With shared KBs there are two segments:

| Segment | Contents |
| --- | --- |
| Shared KB | Parsed KB clauses, interned strings/URIs, histories, prebuilt reasoner queues and indexes, and the in-segment `glb` called `rglb`. |
| Local child DB | Query clauses, derived clauses, answers, histories created during the query, and run-time allocation. |

During `-readkb`, the shared segment is writable and `init_shared_database()`
allocates `rglb` and shared indexes inside it. During `-usekb`, an existing KB
must be treated as read-only. The process uses a private copy of `rglb`,
available as `g->kb_g` / `r_kb_g(g)`, to read KB indexes safely.

The segment header also carries reasoner-facing process and parser state when
`USE_REASONER` is enabled: `kb_db`, `rglb`, `clauselist`, `urilist`,
`infrmlist`, parser counters such as `parse_skolem_nr` and
`parse_newpred_nr`, command-line flags such as `tptp`, `json`, `convert`,
`clausify`, print settings, time limits, and error fields. In shared-KB query
mode, run-time code should read query-local settings from the local DB header
when appropriate and should not write option state into the KB header.

### Database Object Headers

Variable-length objects start with a size word whose low two bits are allocator
state, not length. Use allocator macros such as `getusedobjectwantedgintsnr()`
instead of hand-masking unless you are working on the allocator itself.

The allocator state bits in `Db/dballoc.h` are:

| Low bits | Macro | Meaning |
| --- | --- | --- |
| `01` | `isfreeobject()` / `makefreeobjectsize()` | Free object. |
| `00` | `isnormalusedobjectprevused()` | Used object whose predecessor is used. |
| `10` | `isnormalusedobjectprevfree()` | Used object whose predecessor is free. |
| `11` | `isspecialusedobject()` | Special allocator object such as designated victim or area boundary. |

These bits are allocator metadata. They are separate from the low-bit tags of
encoded `gint` values stored inside records.

Data records use this logical header before content fields:

| Gint position | Meaning |
| --- | --- |
| 0 | Encoded object length in bytes. |
| 1 | Record meta, including reasoner record-kind bits. |
| 2 | Backlinks offset. |
| 3+ | User/reasoner content fields. |

String-like objects (`str`, `xmlliteral`, `uri`, `blob`) use a longer header.
With `REASONER_SINE` enabled, `LONGSTR_HEADER_GINTS == 13` and the reasoner
uses these positions:

| Position | Macro | Meaning |
| --- | --- | --- |
| 1 | `LONGSTR_META_POS` | Object subtype and length-difference byte. |
| 2 | `LONGSTR_REFCOUNT_POS` | Reference count. |
| 3 | `LONGSTR_BACKLINKS_POS` | Backlinks offset. |
| 4 | `LONGSTR_HASHCHAIN_POS` | Next object in string hash bucket. |
| 5 | `LONGSTR_EXTRASTR_POS` | Language, xsd type, or namespace offset. |
| 6 | `LONGSTR_RHASH_POS` | Reasoner hash cache. |
| 7 | `LONGSTR_RMETA_POS` | Reasoner metadata cache. |
| 8 | `LONGSTR_RCOUNT_POS` | Reasoner count cache. |
| 9 | `LONGSTR_TAXONOMY_POS` | Taxonomy relation pointer. |
| 10 | `LONGSTR_ID_POS` | Unique SINE/string id. |
| 11 | `LONGSTR_SCOUNT_POS` | Occurrence clause count for SINE. |
| 12 | `LONGSTR_OCCS_POS` | Occurrence cvec for SINE. |

The `LONGSTR_R*`, SINE, refcount, and hash-chain fields are in-place caches or
bookkeeping. In `-usekb`, updating them on a KB-resident URI/string is a shared
KB write and must be avoided or redirected.

### List Cells and Clause Lists

WhiteDB list cells are fixed-length `gcell` pairs with `car` and `cdr` fields.
They are allocated with `alloc_listcell(db)`. Database-header lists such as
`dbmemsegh(db)->clauselist`, `urilist`, and parsed formula lists are chains of
these cells. The fields store encoded offsets or list-cell offsets, not C list
objects.

When a local child DB is attached to a shared KB, do not splice query-time list
cells into KB-owned lists. Build temporary local lists in `g->local_db` or save
and restore local header fields around a nested query.

## 4. `vec`, `cvec`, and Raw Buffers

`Reasoner/mem.h` defines the small dynamic array formats used throughout the
reasoner:

| Type | Layout |
| --- | --- |
| `vec` | `v[0]` is length; elements start at `v[1]`. |
| `cvec` | `v[0]` is length; `v[1]` is the next free position; elements start at `v[2]`. |

Offset-valued versions are named `veco` and `cveco`.

Store through helpers that can reallocate:

```c
v = wr_vec_store(g, v, i, value);
cv = wr_cvec_store(g, cv, i, value);
cv = wr_cvec_push(g, cv, value);
```

Always reassign the returned pointer. `wr_alloc_from_cvec(g, buf, gints)`
reserves raw space inside a `cvec` build buffer. Reset a normal cvec buffer by
setting `CVEC_NEXT(buffer) = CVEC_START`.

`wr_str_new(g, len)` creates a process-local string managed by the reasoner
memory helpers.

`wr_malloc()`, `wr_calloc()`, and vector allocation depend on `g->inkb`: during
KB build, `inkb == 1` makes reasoner allocation use the DB segment; during
ordinary runs, `inkb == 0` uses process memory. Offset-valued vector fields
inside `glb` must still be interpreted with `rotp()` / `rpto()`.

Alignment matters for buffers allocated out of cvecs. `build.h` sets
`CVEC_ALLOC_ALIGNMENT_BYTES == 8`, and `mem.h` uses `VEC_ALIGNMENT_BYTES == 8`.
Code that stores raw records in a cvec should allocate through
`wr_alloc_from_cvec()` rather than manually advancing `CVEC_NEXT`.

## 5. The `glb` Reasoner State

`glb` is defined in `Reasoner/glb.h` and initialized in `Reasoner/glb.c`. It is
the central state object for one reasoner run.

Important field groups:

| Group | Representative fields |
| --- | --- |
| Database pointers | `db`, `kb_db`, `child_db`, `local_db`, `kb_g`, `inkb` |
| Clause queues and sets | `clbuilt`, `clactive`, `clactivesubsume`, `clpickstack`, `clqueue`, `clqueue_given`, `clpickpriorqueue`, `clpick_queues` |
| Resolution indexes | `hash_neg_atoms`, `hash_pos_atoms`, `hash_neg_units`, `hash_pos_units` |
| Cut/subsumption indexes | `hash_neg_groundunits`, `hash_pos_groundunits`, `hash_neg_grounddoubles`, `hash_pos_grounddoubles`, `hash_neg_active_groundunits`, `hash_pos_active_groundunits` |
| Equality/rewrite indexes | `hash_para_terms`, `hash_eq_terms`, `hash_rewrite_terms` |
| Variable state | `varbanks`, `varstack`, `tmp_unify_vc`, occurs-check fields |
| Build buffers | `given_termbuf`, `derived_termbuf`, `queue_termbuf`, `active_termbuf`, `hyper_termbuf`, `simplified_termbuf` |
| Strategy/run state | guide fields, limits, print flags, counters, proof state, answers |

`wr_glb_init_simple()` sets scalar defaults. Notable data-layout defaults are:

* `unify_firstuseterm = 1`
* `unify_funpos = 1`
* `unify_funarg1pos = 2`
* `unify_funarg2pos = 3`
* `local_db = db` until `rmain.c` overrides it for shared-KB runs

`wr_glb_init_shared_complex()` allocates queue and index structures that may be
stored in a KB during `-readkb`. `wr_glb_init_local_complex()` allocates the
per-run vectors, buffers, varbanks, stacks, and temporary structures.

The `glb` struct has a shared-data block followed by a local-data block. The
shared-data block contains offset-valued clause queues and indexes that are
meaningful inside an `rglb` stored in a KB. The local-data block contains
per-run pointers, buffers, varbanks, temporary vectors, strategy state, build
flags, statistics, and print buffers. When a KB is attached, only the
offset-valued shared structures copied into `kb_g` should be used to read KB
indexes; local pointer fields copied from `rglb` are stale process addresses.

Important temporary/build fields in `glb`:

| Field | Meaning |
| --- | --- |
| `tmp_unify_vc` | Pointer to the active varstack next-free counter during low-level unify/match calls. |
| `tmp_unify_occcheck`, `tmp_unify_do_occcheck` | Occurs-check state used by unification. |
| `build_subst` | Substitute bound variables while building. |
| `build_rename` | Rename variables into the configured target bank. |
| `build_calc` | Evaluate computable functions/predicates while building. |
| `build_dcopy` | Copy non-immediate data instead of reusing existing records. |
| `build_rewrite` | Apply rewrite rules during building. |
| `build_buffer` | Target cvec buffer, or `NULL` for direct record allocation. |
| `build_rename_bank`, `build_rename_banknr` | Target variable bank for renamed variables. |

These fields are global state for a build phase. Use the setup/cleanup helpers
in `derive.c`, `rgenloop.c`, and related modules instead of setting individual
flags opportunistically.

## 6. Record Header and Record Kinds

Every WhiteDB record has `RECORD_HEADER_GINTS == 3` header gints. The reasoner
uses `RECORD_META_POS` for record-kind bits and accesses content fields through
the macros in `Reasoner/clterm.h`:

```c
get_field(rec, n)
set_field(rec, n, value)
get_record_len(rec)
```

The reasoner distinguishes these record kinds by meta bits:

| Predicate | Record kind |
| --- | --- |
| `wg_rec_is_fact_clause(db, rec)` | Ground positive unit clause |
| `wg_rec_is_rule_clause(db, rec)` | Negative unit, non-ground unit, or multi-literal clause |
| `wg_rec_is_atom_rec(db, rec)` | Atom record |
| `wg_rec_is_term_rec(db, rec)` | Term record |
| `wg_rec_is_prop_clause(db, rec)` | Propositional clause |

The raw meta bits include `RECORD_META_FACT_CLAUSE`,
`RECORD_META_RULE_CLAUSE`, `RECORD_META_PROP_CLAUSE`, `RECORD_META_ATOM`,
`RECORD_META_TERM`, and `RECORD_META_BLOCKED`.

## 7. Clauses and Literals

### Fact Clauses

A fact clause is a ground positive unit clause. The record itself is the atom;
there is no wrapper and no literal meta field.

```c
if (wg_rec_is_fact_clause(g->db, cl)) {
  gint atom = rpto(g, cl);
}
```

Do not call `wg_get_rule_clause_atom()` on a fact clause.

### Rule Clauses

Rule clauses represent everything else:

* negative units;
* non-ground units;
* multi-literal clauses.

After the normal record header, a rule clause has one extra clause-header field
followed by `[literal meta, atom]` pairs:

```text
[CLAUSE_EXTRAHEADERLEN = 1]
[lit0_meta][lit0_atom]
[lit1_meta][lit1_atom]
...
```

Use the accessors instead of open-coding offsets:

```c
int n = wg_count_clause_atoms(db, cl);
gint meta = wg_get_rule_clause_atom_meta(db, cl, i);
gint atom = wg_get_rule_clause_atom(db, cl, i);
wg_set_rule_clause_atom_meta(db, cl, i, meta);
wg_set_rule_clause_atom(db, cl, i, atom);
```

Literal polarity is encoded in the literal meta, not in the atom:

```c
int neg = wg_atom_meta_is_neg(db, meta) != 0;
int opposite = litmeta_negpolarities(meta1, meta2);
```

The atom term of `-p(a)` is still `p(a)`.

### Propositional Clauses

A propositional clause is marked with `RECORD_META_PROP_CLAUSE`. It still has
`CLAUSE_EXTRAHEADERLEN == 1`, but each literal slot has only an atom/value and
no literal meta:

```c
#define PROP_LIT_WIDTH 1
#define PROP_LIT_ATOM_POS 0
wg_get_prop_clause_atom(db, rec, litnr)
wg_set_prop_clause_atom(db, rec, litnr, atom)
wg_count_prop_clause_atoms(db, clause)
```

Do not use rule-clause literal-meta accessors on prop clauses.

### Literal Meta

`Reasoner/clterm.h` packs literal metadata into an encoded small integer. The
currently used fields include:

| Field | Purpose |
| --- | --- |
| negative bit | Literal polarity. |
| ground bit | Whether the atom is ground. |
| size | Approximate term size, capped by the field width. |
| depth | Approximate term depth, capped by the field width. |
| preference length/hash | Fast prefilters for selection/subsumption. |
| special/cut/equality/linear bits | Selection and inference-control flags. |

The bit packing described in `clterm.h` is:

| Bit field | Mask / shift | Meaning |
| --- | --- | --- |
| negative | `ATOM_META_NEG_MASK`, `ATOM_META_NEG_SHIFT == 22` | Literal is negative. |
| ground | `ATOM_META_GROUND_MASK`, `ATOM_META_GROUND_SHIFT == 21` | Atom has no variables. |
| size | `ATOM_META_SIZE_MASK`, shift 16 | Approximate size, 5 bits. |
| depth | `ATOM_META_DEPTH_MASK`, shift 11 | Approximate depth, 5 bits. |
| preference length | `ATOM_META_PREFLEN_MASK`, shift 8 | Ground-prefix length, 3 bits. |
| preference hash | `ATOM_META_PREFHASH_MASK`, shift 0 | Ground-prefix hash, 8 bits. |

Macros such as `ATOM_META_CANSUBS_MASK`, `ATOM_META_GROUNDCMP_MASK`,
`ATOM_META_DEPTHCANSUBS_MASK`, and `ATOM_META_PREFLENCANSUBS_MASK` are
prefilters. Passing a prefilter does not prove identity or subsumption; call
the relevant equality, matching, or unification function.

`Reasoner/clmeta.h` also defines a compact active-clause metablock used before
full subsumption:

| Position | Macro | Meaning |
| --- | --- | --- |
| 0 | `CLMETABLOCK_LENGTHS_POS` | Packed ground/length/negative-length counts. |
| 1 | `CLMETABLOCK_SIZES_POS` | Packed depth/size/preflen values. |
| 2 | `CLMETABLOCK_PREF1BITS_POS` | Predicate-symbol bitmask. |
| 3 | `CLMETABLOCK_PREF2BITS_POS` | Predicate plus shallow-argument bitmask. |
| 4 | `CLMETABLOCK_PREF3BITS_POS` | Longer prefix bitmask. |
| 5 | `CLMETABLOCK_CL_POS` | Clause pointer/offset. |

`term_metacalc` and `cl_metacalc` are temporary calculation structs, not stored
record layouts. They feed `wr_litmeta_to_gint()` and `wr_calc_clause_meta()`.

## 8. Atoms and Terms

Atoms and terms use the same content-field convention:

```text
field 0: reserved term-meta slot
field 1: functor or predicate symbol
field 2: first argument
field 3: second argument
...
```

Normal unification and matching start at `g->unify_firstuseterm`, usually 1.
Field 0 is reserved. In the normal current build, `USE_TERM_META` is not
enabled, so field 0 is not maintained as a meaningful term and must not be fed
to matching or unification.

Use the configured positions rather than constants when possible:

```c
int start = RECORD_HEADER_GINTS + g->unify_firstuseterm;
int funpos = RECORD_HEADER_GINTS + g->unify_funpos;
```

Creation and mutation helpers:

```c
void *atom = wr_create_atom(g, termnr);
void *term = wr_create_term(g, termnr);
wr_set_atom_subterm(g, atom, termnr, subterm);
wr_set_term_subterm(g, term, termnr, subterm);
```

In shared-KB build paths there are also `wr_set_kb_atom_subterm()` and
`wr_set_kb_term_subterm()`. During `-usekb`, new run-time records should be
created in the local DB and KB-resident records should not be mutated.

If `USE_TERM_META` is enabled in a future build, field 0 is interpreted with
`TERM_META_POS == 0`, `TERMMETA_GROUND_MASK`, and `TERMMETA_SIZE_MASK`. In the
current normal build it is a reserved slot, not a valid subterm. This is a
common source of crashes in term-walking code.

## 9. Variables, Varbanks, and Varstack

Variables are immediate encoded `gint` values. `decode_var(x)` is the full
index into `g->varbanks`, including the bank offset. It is not just a local
variable number.

`Reasoner/glb.h` defines five banks of 1000 variables:

| Bank | Purpose |
| --- | --- |
| 0 | Input/passive clauses |
| 1 | Given-clause renamed variables |
| 2 | Active-clause renamed variables |
| 3 | Derived-clause renamed variables |
| 4 | Temporary rename area; values should remain `UNASSIGNED` |

`varbanks` is allocated as a `vec`, but unification treats it as a raw array of
`NROF_VARBANKS * NROF_VARSINBANK` slots. `wr_clear_all_varbanks(g)`
intentionally overwrites slot 0, including the normal vec length header, with
`UNASSIGNED`.

`varstack` is a `cvec` containing addresses of assigned varbank cells so they
can be reset cheaply.

Important macros from `Reasoner/unify.h`:

```c
#define UNASSIGNED WG_ILLEGAL
#define VARVAL(x, vb) (wr_varval(x, vb))
#define VARVAL_DIRECT(x, vb) (vb[decode_var(x)])
#define SETVAR(x, y, vb, vstk, vc) \
  (vb[decode_var(x)] = y, vstk[*vc] = (gint)((gptr)vb + decode_var(x)), ++(*vc))
```

`VARVAL_F(x, vb)` is the fast lookup used in hot paths: it reads the direct
slot, returns the variable itself when it is unassigned, returns a non-variable
value directly, or follows variable chains through `wr_varval()`.

Before a fresh independent matching context:

```c
if (g->varbanks == NULL) {
  g->varbanks = wr_vec_new(g, NROF_VARBANKS * NROF_VARSINBANK);
}
if (g->varstack == NULL) {
  g->varstack = wr_cvec_new(g, NROF_VARBANKS * NROF_VARSINBANK);
}
CVEC_NEXT(g->varstack) = CVEC_START;
wr_clear_all_varbanks(g);
```

## 10. Equality, Matching, and Unification

The three central relations are implemented in `Reasoner/unify.c`:

| Function | Relation | Binding behavior |
| --- | --- | --- |
| `wr_equal_term(g, x, y, 1)` / `WR_EQUAL_TERM` | Syntactic equality | No bindings; variables compare as literal encoded values. |
| `wr_match_term(g, x, y, 1)` | One-way matching | `x` is the general side; variables in `x` may be bound. |
| `wr_unify_term(g, x, y, 1)` | Symmetric unification | Variables on both sides may be bound; occurs-check machinery is used. |

Both matching and unification write bindings into `g->varbanks` and push changed
cells onto `g->varstack`. For isolated probes, snapshot and restore:

```c
int vc = CVEC_NEXT(g->varstack);
int ok = wr_match_term(g, general, instance, 1);

if (ok) {
  /* inspect bindings or build an instantiated term */
}

if (vc != CVEC_NEXT(g->varstack)) {
  wr_clear_varstack_topslice(g, g->varstack, vc);
}
```

Top-level resolution candidate loops usually clear the whole stack after each
candidate:

```c
if (wr_unify_term(g, xatom, yatom, 1)) {
  wr_process_resolve_result(g, xatom, xcl, yatom, ycl, xcl_as_active);
}
wr_clear_varstack(g, g->varstack);
```

Choose the relation by semantics:

* resolution and factoring use `wr_unify_term()`;
* subsumption uses `wr_match_term()` with the potential subsumer first;
* ground identity uses `WR_EQUAL_TERM` or `wr_equal_term()`;
* functor equality and hash equality are only prefilters.

## 11. Clause and Term Building Buffers

Clause construction uses `cvec` buffers owned by `glb`:

| Buffer | Typical use |
| --- | --- |
| `given_termbuf` | Rebuilding a selected given clause. |
| `derived_termbuf` | Half-built literals after resolution/factoring/paramodulation. |
| `queue_termbuf` | Final queued clause copy. |
| `active_termbuf` | Active-clause copy used by indexes. |
| `hyper_termbuf` | Partial hyperresolution clauses. |
| `simplified_termbuf` | Simplification output. |

`wr_build_calc_term(g, x)` in `Reasoner/build.c` is the central recursive
builder. Depending on `glb` build flags it can:

* substitute currently bound variables;
* rename variables into the target bank;
* copy atom/term records;
* preserve the reserved term field;
* evaluate computable arithmetic, string, list, and equality functions;
* apply rewriting.

Use existing setup/cleanup helpers in `derive.c` and `rgenloop.c` before
calling build functions. Do not leave build flags or `g->build_buffer` in a
modified state after a local construction phase.

`Reasoner/build.h` groups computable functions by opcode range:

| Range | Examples | Meaning |
| --- | --- | --- |
| 1-6 | `COMP_FUN_EQUAL`, `COMP_FUN_LESS`, `COMP_FUN_GREATER` | Predicate comparisons. |
| 10-19 | `COMP_FUN_PLUS`, `COMP_FUN_DIV`, quotient/remainder variants | Binary arithmetic functions. |
| 20-26 | `COMP_FUN_IS_INT`, `COMP_FUN_IS_LIST`, `COMP_FUN_IS_UNIQUE` | Unary predicate tests. |
| 30-38 | `COMP_FUN_TO_INT`, `COMP_FUN_ROUND`, `COMP_FUN_STRLEN`, substring test | Unary computations. |
| 40-41 | `COMP_FUN_FIRST`, `COMP_FUN_REST` | List decomposition. |
| 50 | `COMP_FUN_IS_SUBSTRAT` | Three-argument substring predicate. |

Computed constants produced during a run must be encoded through `g->local_db`
in code paths that can run under `-usekb`. Encoding through `g->db` can write
into a read-only KB.

## 12. Queues, Hash Nodes, and Active-Clause Blocks

`Reasoner/clstore.h` documents the concrete layouts used by clause queues,
priority queues, hash lists, and active-clause subsumption blocks.

### Clause-Pick Queues

`g->clpick_queues` is a vec of `NROF_CLPICK_QUEUES == 4` queue blocks. Each
block has `CLPICK_QUEUE_BLOCKGINTS == 10` gints. The predefined block starts
are:

| Queue | Start position |
| --- | --- |
| goal plus assumptions | `GOAL_ASSUMPTIONS_QUEUE_POS == 1` |
| goal | `GOAL_QUEUE_POS == 11` |
| assumptions | `ASSUMPTIONS_QUEUE_POS == 21` |
| axioms | `AXIOMS_QUEUE_POS == 31` |

Inside each block, positions store the cvec queue, priority queue, next-given
index, and ratio counters:

| Position macro | Meaning |
| --- | --- |
| `CLPICK_QUEUE_POS` | Ordinary queue cvec offset. |
| `CLPICK_PRIORQUEUE_POS` | Priority queue offset. |
| `CLPICK_QUEUE_GIVEN_POS` | Next ordinary queue element to pick. |
| `CLPICK_PRIORQUEUE_RATIO` / `_COUNTER` | Interleaving control for priority picks. |
| `CLPICK_THIS_QUEUE_RATIO` / `_COUNTER` | Interleaving control among queue classes. |

A single priority queue uses `PRIORQUEUE_NODE_GINT_NR == 4` and header fields
for max priority, min pickable priority, max used priority, and max keep
priority. Each priority bucket array stores length, next free index, next pick
index, and bucket-array offset.

### Hash Nodes

Resolution, paramodulation, rewrite, and related indexes use hash-list nodes of
`CLTERM_HASHNODE_GINT_NR == 4` gints:

| Position | Macro | Meaning |
| --- | --- | --- |
| 0 | `CLTERM_HASHNODE_TERM_POS` | Indexed term or atom. |
| 1 | `CLTERM_HASHNODE_CL_POS` | Parent clause. |
| 2 | `CLTERM_HASHNODE_PATH_POS` | Term path for paramodulation/rewrite indexes. |
| 3 | `CLTERM_HASHNODE_NEXT_POS` | Next node in bucket. |

`CLTERM_HASHNODE_LEN_POS` aliases position 0 for allocation/free-list handling.
With `MALLOC_HASHNODES` enabled, nodes come from malloc; otherwise they are
allocated from `g->hash_nodes`.

The older atom-hash implementation in `hash.h` uses `ATOMHASH_NODE_SIZE == 4`:
`ATOMHASH_ATOM_POS`, `ATOMHASH_ATOMNR_POS`, `ATOMHASH_POSCLAUSES_POS`, and
`ATOMHASH_NEGCLAUSES_POS`.

### Active-Clause Subsumption Blocks

`clactivesubsume` stores compact blocks of `CLACTIVE_NODE_GINT_NR == 5` gints:

| Position | Macro | Meaning |
| --- | --- | --- |
| 0 | `CLACTIVE_NODE_META_POS` | Packed clause meta. |
| 1 | `CLACTIVE_NODE_PATH0HASH_POS` | Prefix/path hash 0. |
| 2 | `CLACTIVE_NODE_PATH1HASH_POS` | Prefix/path hash 1. |
| 3 | `CLACTIVE_NODE_PATH2HASH_POS` | Prefix/path hash 2. |
| 4 | `CLACTIVE_NODE_CL_POS` | Clause pointer/offset. |

These blocks are filters. Full subsumption still uses literal matching.

### Paramodulation Paths

Paramodulation paths are packed integers:

| Field | Macro | Limit |
| --- | --- | --- |
| literal number | `PARA_TERMPATH_LITNRSHIFT == 16`, `PARA_TERMPATH_LITNRMASK` | `PARA_TERMPATH_MAXLITNR == 255` |
| term position/path | `PARA_TERMPATH_POSMASK` | `PARA_TERMPATH_MAXPOS == 60000` |
| left/right flag | `PARA_TERMPATH_LEFTMASK` | one bit |

Use `wr_encode_para_termpath()`, `wr_decode_para_termpath_litnr()`, and
`wr_decode_para_termpath_pos()` rather than open-coding these masks.

### Resolution Scan Pattern

The binary-resolution scan follows this representation pattern:

1. For each literal in the given clause, compute the atom functor hash with
   `wr_atom_funhash(g, atom)`.
2. Select the opposite-polarity active atom index:
   `hash_pos_atoms` for a negative given literal, `hash_neg_atoms` for a
   positive given literal.
3. Scan the hash bucket. Each node carries the indexed term and its parent
   clause.
4. Call `wr_unify_term(g, xatom, yatom, 1)`.
5. On success, build the resolvent through `wr_process_resolve_result()`.
6. Clear the varstack before the next candidate.

The relevant helpers include:

```c
wr_cl_store_res_terms(g, cl, resolvability);
wr_term_hashstore(g, hashdata, term, cl);
wr_term_complexhash(g, hasharr, hashposbits, term);
wr_atom_funhash(g, atom);
wr_clterm_add_hashlist(g, hashvec, hash, term, cl);
wr_clterm_add_hashlist_withpath(g, hashvec, hash, term, cl, path);
```

With a shared KB, candidate loops may scan both local indexes from `g` and KB
indexes from `r_kb_g(g)`.

## 13. Histories and Answers

History records are ordinary database records described in `Reasoner/history.h`.
With `RECORD_HISTORY_ORDER` enabled, the prefix layout is:

| Position | Macro | Meaning |
| --- | --- | --- |
| 0 | `HISTORY_PRIORITY_POS` | Encoded role/priority. |
| 1 | `HISTORY_NAME_POS` | Input clause name or generated name. |
| 2 | `HISTORY_CLID_POS` | Stable clause id. |
| 3 | `HISTORY_DERIVED_ORDER_POS` | Derivation order stamp. |
| 4 | `HISTORY_GIVEN_ORDER_POS` | Given-clause order stamp. |
| 5 | `HISTORY_DERIVATION_TAG_POS` | Encoded derivation tag head. |

Parent/path positions after the prefix depend on the tag. Resolution/factoring
use `HISTORY_PARENT1_POS` and `HISTORY_PARENT2_POS`; paramodulation uses
`HISTORY_PATH_POS`, `HISTORY_PARA_PARENT1_POS`, and
`HISTORY_PARA_PARENT2_POS`.

The tag head packs tag, extra bits, and literal positions with
`WR_HISTORY_TAG_SHIFT`, `WR_HISTORY_EXTRA_SHIFT`, and
`WR_HISTORY_POS1_SHIFT`. Tags include resolve, factorial, para, equality
reflexive, simplify, propagate, propinst, instgen, and external prop.

Input histories are built with `wr_build_input_history()`. Derived histories
are built by operation-specific helpers such as:

```c
wr_build_resolve_history(g, ...);
wr_build_factorial_history(g, ...);
wr_build_para_history(g, ...);
wr_build_simplify_history(g, ...);
```

Clause roles are encoded in history priority values such as
`WR_HISTORY_GOAL_ROLENR`, `WR_HISTORY_ASSUMPTION_ROLENR`,
`WR_HISTORY_AXIOM_ROLENR`, `WR_HISTORY_FROMGOAL_ROLENR`, and
`WR_HISTORY_FROMAXIOM_ROLENR`. Queue placement and proof printing both depend
on these roles.

Answer literals are recognized by `wr_answer_lit()`. Pure answer clauses are
registered with `wr_register_answer()` in `g->answers` and are not queued as
ordinary clauses.

In `-usekb` runs, history order setters and other in-place writers must skip
history records inside the attached KB.

## 14. Shared-Memory Representation Invariants

The shared-memory model is documented in detail in `Doc/SHARED_MEMORY.md`.
For data representation work, keep these invariants in view:

1. `rglb` is the `glb` stored inside the shared segment. It is meaningful only
   as a read-only source of offset-valued shared structures.
2. `kb_g` is a private malloc copy of `rglb` with `kb_g->db` fixed to this
   process's attach address.
3. Pointer-valued local fields copied from `rglb` are not usable in an attaching
   process.
4. New run-time objects in a query go into `g->local_db`.
5. Offsets stored in records and indexes must be usable relative to `g->db`.
6. KB-resident clauses, histories, strings/URIs, headers, and indexes are
   read-only during `-usekb`.
7. Before an in-place mutation, check whether the target pointer is inside the
   attached KB with `wg_in_attached_kb(ptr)`.

## 15. Review Checklist

Use this checklist when changing code that touches records, terms, clauses, or
indexes:

1. Branch on fact versus rule clause before reading literals.
2. Treat fact clauses as atom records.
3. Read rule-clause polarity from literal meta, not from atom terms.
4. Skip atom/term field 0 in normal matching and unification.
5. Clear varbanks before a fresh independent matching context.
6. Snapshot and restore varstack around matching/unification probes.
7. Use `wr_match_term(g, general, instance, 1)` only in that direction.
8. Use builder helpers instead of hand-copying nested terms.
9. Allocate run-time values with `g->local_db` in `-usekb`-capable code.
10. Treat hash/meta checks as prefilters, not proofs.
11. Reassign vectors after `wr_vec_store()`, `wr_cvec_store()`, or
    `wr_cvec_push()`.
12. Guard all in-place record/history/index writes against attached KB memory.
13. Use prop-clause accessors for `RECORD_META_PROP_CLAUSE`; rule-clause
    literal meta does not exist there.
14. Use paramodulation path helpers instead of hand-packing literal/path bits.
15. Treat URI/string reasoner metadata fields as shared-KB writes when the
    object lives in the attached KB.
16. Keep `glb` build flags scoped to the established setup/cleanup phase.
