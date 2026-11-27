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

### Everything Is a `gint`: Use It Directly

Every constant, function symbol, predicate symbol and variable is one `gint`.
A large object (a long string, a URI, a record) is an offset to the object,
and that offset is also a `gint`. So every value has the same size, and code
can treat it as an integer:

* Use the `gint` itself as a key of a table, as a token of a tree index, and
  as an operand of `==`.
* Do not decode a symbol to its string to compare it. Do not compute a hash
  of a constant where its `gint` serves. `wg_decode_uri()` and `strcmp()` are
  for printing and parsing only.
* A compound term is the exception: it is a record, and records are not
  shared (section 8). Walk a record; never use its offset as a key.

Read this section, the next three subsections and `Doc/TERM_MATCHING.md`
sections 3 and 4 before writing any code that compares, hashes or indexes
terms.

### Type Tags in the Low Bits

Common predicates and decoders:

| Predicate | Meaning | Decode or access |
| --- | --- | --- |
| `isvar(x)` | Immediate reasoner variable | `decode_var(x)` |
| `issmallint(x)` | Immediate small integer | `decode_smallint(x)` |
| `isdatarec(x)` | Offset to a data record | `decode_record(db, x)` |
| `islongstr(x)` | Offset to a long string or URI object | `wg_decode_str()`, `wg_decode_uri()` |
| `isfulldouble(x)` | Offset to a double object | `wg_decode_double()` |

All encodings from `Db/dbdata.h` (lines 305-460), with the answer to the
question "does one value have exactly one encoding?":

| Encoding | Low bits | Test | One encoding per value | Notes |
| --- | --- | --- | --- | --- |
| null | the value `0` | `x == 0` | yes | Unassigned field. |
| data record offset | `000` | `isdatarec(x)` | no | Clauses, atoms, compound terms. Records are not shared: compare by walking. |
| full integer offset | low two bits `01` (so `001` or `101`) | `isfullint(x)` | no | Word storage for an integer that does not fit a small integer. The third bit is an address bit. |
| full double offset | `010` | `isfulldouble(x)` | no | Doubleword storage. |
| small integer | `011` | `issmallint(x)` | yes | Immediate, `(value << 3) \| 3`; 61 bits on a 64-bit build. |
| long string / URI offset | `100` | `islongstr(x)` | yes | Interned object: long strings, URIs, XML literals, blobs. All symbols are here. |
| short string offset | `110` | `isshortstr(x)` | no | Fixed 32-byte object for a string of type `WG_STRTYPE` without a language tag. |
| variable | low four bits `0111` | `isvar(x)` | yes | Immediate, `(n << 4) \| 7`. |
| fixpoint | low byte `0000 1111` | `isfixpoint(x)` | yes | Immediate. |
| char | low byte `0001 1111` | `ischar(x)` | yes | Immediate. |
| date | low byte `0010 1111` | `isdate(x)` | yes | Immediate. |
| time | low byte `0011 1111` | `istime(x)` | yes | Immediate. |
| tiny string | low byte `0100 1111` | `istinystr(x)` | yes | Defined, not in use (`USETINYSTR` is off). |
| anonymous constant | low byte `0101 1111` | `isanonconst(x)` | yes | Immediate, `(n << 8) \| 0x5f`. |

* `isptr(x)` is `x && (x & 3) != 3`: low two bits `11` mean an immediate
  value.
* Test a full integer with the two-bit mask (`isfullint`, `FULLINTMASK`,
  `NONPTRBITS`), never with three bits: both `001` and `101` are full
  integers.
* One `==` on two `gint` values compares kind and value at once for every
  encoding marked "yes".
* `isimmediatedata(x)` is true for `0` and non-pointer immediate values, but
  not for full integers stored in the word area. Do not use pointer tests as
  a proxy for term kind; records still need their meta bits checked.

The comment table at `Db/dbdata.h:307-327` marks each kind with "= eq" or
"= not eq". "= eq" means: two values are equal if and only if their `gint`
values are equal. "= not eq" means: two different `gint` values can still
denote equal values.

### Uniqueness of Encoded Values

**Unique (one `gint` per value).** Every immediate kind, and every object
that `find_create_longstr()` creates (`Db/dbdata.c`): it looks the value up
in the string hash (`wg_find_strhash_bucket()`, `Db/dbhash.c`), first in an
attached shared KB and then in the local database, and returns the existing
object when there is one. `wg_encode_uri()` calls it directly;
`wg_encode_unistr()` calls it for every string that is not a short string.
A long-string object has a reference count and is freed only when the count
reaches zero (`free_field_encoffset()`).

Reasoner code relies on this: pass `uniquestrflag == 1` to equality,
matching and unification helpers. A symbol of an attached KB keeps its KB
encoding in a query, so `gint` equality also holds across the KB and the
local database (compare through `g->db`).

**Not unique today (a new object per encoder call).** `wg_encode_double()`
allocates a doubleword for every call. `wg_encode_int()` allocates a word
for every integer that does not fit a small integer. `wg_encode_unistr()`
allocates a short-string object for every string of type `WG_STRTYPE` that
has no language tag and is shorter than `SHORTSTR_SIZE`. None of the three
looks for an existing equal object, and `free_field_encoffset()` frees such
an object directly, without a count. So three occurrences of `2.5` in an
input have three different `gint` values.

The existing equality, matching and unification code handles these three
kinds: `wr_equal_ptr_primitives()` (`Reasoner/unify.c`) compares a full
integer by its word, a double by its value and a short string by its bytes.

**Rule for new code.** The missing uniqueness of doubles, full integers and
short strings is a known defect of the encoders; the fix is to make these
values unique at encoding. Until then, new index code assumes one encoding
per constant and compares the `gint` only (as `Reasoner/termindex.c` does).
For two equal doubles with different encodings such code sees two different
constants and can miss a match. That loses a simplification and is sound.
Do not add a compare by value or a hash to work around it, and do not write
tests that depend on floats or on integers above 61 bits until the encoders
are fixed.

### What the Parsers Produce

Checked by a run (a breakpoint at the return of `wr_parse_primitive()`,
`Builtparser/dbparse.c`), for TPTP and for JSON input:

| Input | Encoded as | One encoding per value |
| --- | --- | --- |
| Predicate, function or constant symbol: `p`, `f`, `a` | URI object, low bits `100` | yes |
| Single-quoted symbol: `'quoted sym'` | URI object | yes |
| Double-quoted string (TPTP distinct object): `"abc"` | URI object | yes |
| JSON string in a term position: `"abc"` | URI object | yes |
| Integer: `7` | immediate small integer | yes |
| Decimal number: `2.5` | double object, low bits `010` | no: a new object per occurrence |

* No input syntax produces a short string or a full integer. A computed
  value (`Reasoner/build.c`, `Reasoner/arithinst.c`) can be a full integer
  or a double.
* The built-in symbols (`ACONST_EQUAL`, `ACONST_PLUS` and the others, below)
  are immediate anonymous constants.

### Check an Encoding Claim by a Run

Reading one encoder is not enough to know what a value looks like at run
time: the parser decides the type first. Before you state or rely on a fact
about an encoding, print the `gint`:

```sh
gcc -O0 -g -w -I. -o /tmp/gkc-dbg <the source list of compile.sh> -lm
gdb -q -batch -ex 'break wr_equal_ptr_primitives' -ex 'run problem.p -parallel 0' \
    -ex 'printf "a=0x%lx b=0x%lx\n", a, b' /tmp/gkc-dbg
```

The low three bits of the printed value give the kind (table above).

### Anonymous Constants

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

Unlike `wr_cvec_store()` and `wr_cvec_push()`, `wr_alloc_from_cvec()` does not
grow its buffer. It sets `g->alloc_err` and returns `NULL` if the fixed build
buffer lacks room. Size build buffers in their initialization code rather than
expecting raw allocation to reallocate them.

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
| Arithmetic instantiation | `arith_inst_*` mode/bounds, global candidate pool, lazy duplicate cache, probe temporaries, and `stat_arithinst_*` counters |

`wr_glb_init_simple()` sets scalar defaults. Notable data-layout defaults are:

* `unify_firstuseterm = 1`
* `unify_funpos = 1`
* `unify_funarg1pos = 2`
* `unify_funarg2pos = 3`
* `local_db = db` until `rmain.c` overrides it for shared-KB runs
* `arith_instantiation = 1` for conservative bounded numeric instantiation

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
| `arith_inst_probe_active` and `tmp_arithinst_*` | Observe calculator progress/deleted literals only while a bounded numeric probe rebuilds a parent. |

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

### Copied Trees, Not Globally Hash-Consed DAGs

Compound terms (`isdatarec(x)`) are copied as fresh record trees by
`wr_build_calc_term()` into the active build arena; gkc does **not** perform
global hash-consing of compound terms. Therefore:

* Two structurally identical compound terms usually have **different** `gint`
  encoded offsets. `x == y` is a fast positive shortcut, but `x != y` does not
  imply structural inequality — call `WR_EQUAL_TERM` or `wr_equal_term()`.
* Interned URI/symbol strings (under the standard `uniquestrflag == 1` setting)
  and immediate scalars (smallints, encoded variables, `ACONST_*`) *are* unique
  by value and can be compared directly via `gint` equality.
* Section 1 lists every kind with the answer to "one encoding per value?",
  and says which kinds the encoders do not make unique today (doubles, full
  integers, short strings).

## 9. Variables, Varbanks, and Varstack

Variables are immediate encoded `gint` values. `decode_var(x)` is the full
index into `g->varbanks`, including the bank offset. It is not just a local
variable number.

`Reasoner/glb.h` defines six banks of 1000 variables:

| Bank | Purpose |
| --- | --- |
| 0 | Input clauses as parsed |
| 1 | Given-clause renamed variables |
| 2 | Active-clause renamed variables |
| 3 | Derived-clause renamed variables; a kept derived clause keeps them |
| 4 | Temporary rename area (`RENAME_VARBANK`); values should remain `UNASSIGNED` |
| 5 | Variables of the discrimination tree (`TREE_VARBANK`) |

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

### Hash Encodings and the Four Hash-Table Families

`Reasoner/hash.h` and `Reasoner/hash.c` implement four distinct hash
mechanisms. Do not mix accessors across families:

1. **`clterm_hashlist` (`vec` of `NROF_CLTERM_HASHVEC_ELS == 1,000,000` buckets):**
   Used for `hash_pos_atoms`, `hash_neg_atoms`, `hash_para_terms`,
   `hash_eq_terms`, and `hash_rewrite_terms`. Hash values are combined with
   `WR_HASH_ADD(toadd, orig)` (sdbm-style mixing) and folded into valid bucket
   indices `1 .. (maxsize - 2)` via `WR_HASH_NORM(tonorm, maxsize)` (slot 0 is
   the `vec` length header). Each bucket chains 4-`gint` `CLTERM_HASHNODE`
   records:

   | Position | Macro | Meaning |
   | --- | --- | --- |
   | 0 | `CLTERM_HASHNODE_TERM_POS` | Indexed term or atom (`gint`). |
   | 1 | `CLTERM_HASHNODE_CL_POS` | Parent clause (`db`-relative offset via `rpto`). |
   | 2 | `CLTERM_HASHNODE_PATH_POS` | Term path for paramodulation/rewrite indexes. |
   | 3 | `CLTERM_HASHNODE_NEXT_POS` | Next node in bucket. |

   `CLTERM_HASHNODE_LEN_POS` aliases position 0 for allocation/free-list
   handling. With `MALLOC_HASHNODES` enabled, nodes come from malloc; otherwise
   they are allocated from `g->hash_nodes`.

2. **`termhash` and `offset_termhash` (dynamic `cvec` buckets):**
   Managed via `wr_push_termhash()` / `wr_find_termhash()` and the `offset_`
   variants. Used for ground-unit and ground-double indexes:
   `hash_{pos,neg}_groundunits`, `hash_{pos,neg}_active_groundunits`, and
   `hash_{pos,neg}_grounddoubles`.

3. **`atomhash` (`ATOMHASH_NODE_SIZE == 4`):**
   Managed via `wr_push_atomhash()` / `wr_find_atomhash_bucketnode()` with
   fields `ATOMHASH_ATOM_POS`, `ATOMHASH_ATOMNR_POS`,
   `ATOMHASH_POSCLAUSES_POS`, and `ATOMHASH_NEGCLAUSES_POS`. Used by
   `hash_atom_occurrences` for unit propagation.

4. **Base term hashing (`wr_term_basehash()`):**
   Computes a base hash in `1 .. (N - 3)` for primitive and symbol encodings.
   For URI/long-string records, the hash is cached in `LONGSTR_RHASH_POS`,
   guarded by `wg_in_attached_kb()` so `-usekb` queries never write into
   read-only shared KB URI records.

### When Not to Hash: Tree Indexes

The hash tables above hash a whole atom or term. Their purpose is to find or
compare a term without walking it. A hash match is a candidate only; confirm
it with `wr_equal_term()`.

A discrimination tree is the opposite case: its lookup walks the term token
by token, so a hash has no use there. `Reasoner/termindex.c` is the tree
module (a perfect discrimination tree; its users: the demodulators of
`Reasoner/rewrite.c` and the stored units of the full unit cut and
subsumption in `Reasoner/subsume.c`). Its rules, which any new tree index should follow:

* A token is the encoded `gint` of a symbol or constant, the arity of a
  record, or a variable slot of the stored term. Nothing else.
* A node finds its child by comparing `gint` values (a small array in the
  node, then a skip list ordered by token). No hash of a token, no hash table
  of edges.
* A variable of a stored term is a slot, numbered by first occurrence in that
  term, so variants share one path. Slot k is variable k of the tree bank
  (bank 5), which no term given to the tree uses. The walk binds the cell of
  a slot at its first occurrence, compares the bound term at a later
  occurrence, and clears the cell when it backtracks. The bindings of the
  leaf are the substitution. The demodulators store their right side renamed
  to the tree bank by slot number; the caller sets the bindings on the
  variable stack and builds the right side with the ordinary build
  (`build_subst` 1), without a second match.
* A variable of the query term is a constant for the walk.
* The compare order is the one of `wr_equal_term()`
  (`Doc/TERM_MATCHING.md`, section 4).

### Pointer-vs-Offset Storage in Clause Containers

Not every clause container in `Reasoner/clstore.c` stores clauses in the same
representation. Mixing raw `gptr` casts with `db`-relative offsets causes
immediate crashes:

| Container / Field | Stored element format | Conversion required on read |
| --- | --- | --- |
| `clqueue`, `clactive`, `clpickstack` (`clstore.c`) | Raw `(gint)cl` (`gptr` cast to `gint`) | Cast back to `(gptr)` directly — do **not** call `rotp()` on elements. |
| `clactivesubsume` (`CLMETABLOCK_CL_POS`) | `db`-relative offset (`rpto(g, cl)`) | Decode with `rotp(g, val)`. |
| `CLTERM_HASHNODE` (`CLTERM_HASHNODE_CL_POS`) | `db`-relative offset (`rpto(g, cl)`) | Decode with `rotp(g, val)`. |
| `glb` vector/buffer heads (e.g. `g->clqueue`, `g->clactivesubsume`) | `db`-relative offset (`rpto(g, ptr)`) | Decode the container head with `rotp(g, g->field)`. |

Note the two-level trap in `clstore.c`: the container head `g->clqueue` itself
is stored as an offset (`rotp(g, g->clqueue)`), while the clause elements
pushed inside that `cvec` are raw `(gint)cl` pointers. By contrast,
`clactivesubsume` and `CLTERM_HASHNODE` store clause offsets so shared-KB
structures remain relocatable across processes.

### Active-Clause Subsumption Blocks

`clactivesubsume` stores compact `CLMETABLOCK_ELS == 6`-gint blocks produced by
`wr_calc_clause_meta()`:

| Position | Macro | Meaning |
| --- | --- | --- |
| 0 | `CLMETABLOCK_LENGTHS_POS` | Packed ground/length counts. |
| 1 | `CLMETABLOCK_SIZES_POS` | Packed depth/size/prefix length. |
| 2 | `CLMETABLOCK_PREF1BITS_POS` | Predicate-symbol bitmask. |
| 3 | `CLMETABLOCK_PREF2BITS_POS` | Predicate plus shallow-argument bitmask. |
| 4 | `CLMETABLOCK_PREF3BITS_POS` | Longer prefix bitmask. |
| 5 | `CLMETABLOCK_CL_POS` | Clause offset (`rpto(g, cl)`). |

Older revisions also declared an incompatible five-gint `CLACTIVE_NODE_*`
layout in `clstore.h`; those unused definitions were removed. Current storage
and scans use only the six-gint `CLMETABLOCK_*` layout. These blocks are filters;
full subsumption still uses literal matching.

### Paramodulation Paths

Paramodulation paths are packed integers:

| Field | Macro | Limit |
| --- | --- | --- |
| literal number | `PARA_TERMPATH_LITNRSHIFT == 16`, `PARA_TERMPATH_LITNRMASK` | `PARA_TERMPATH_MAXLITNR == 255` |
| term position/path | `PARA_TERMPATH_POSMASK` | `PARA_TERMPATH_MAXPOS == 60000` |
| left/right flag | `PARA_TERMPATH_LEFTMASK` | one bit |

Use `wr_encode_para_termpath()`, `wr_decode_para_termpath_litnr()`, and
`wr_decode_para_termpath_pos()` rather than open-coding these masks.

### Resolution Scan Pattern (3-Level Ground-Prefix Hashing)

By default (`SIMPLE_ACTIVE_SEARCH_HASH` is undefined), binary resolution in
`Reasoner/resolve.c` uses **3-level ground-prefix hashing** rather than a
single functor-only hash:

1. For an indexed active literal, `wr_atom_calc_prefhashes(g, atom, prefhashes)`
   walks the atom in preorder up to `ATOM_PREFHASH_MAXLEN == 3` non-variable
   symbols, stopping at the first variable and recording cumulative hashes
   (`WR_HASH_ADD` + `WR_HASH_NORM`) in `prefhashes[0..preflen-1]`. The active
   literal is stored in the bucket for its longest ground prefix
   (`prefhashes[preflen - 1]`).
2. For a given literal in `resolve.c`, `wr_atom_calc_prefhashes()` computes its
   prefix hashes `prefhashes[0..preflen-1]`, and the scan probes **every
   prefix level** `0 .. preflen - 1` in the opposite-polarity active atom index
   (`hash_pos_atoms` for a negative given literal, `hash_neg_atoms` for a
   positive given literal). Thus a ground literal `p(a, b)` probes buckets for
   `p` (finding `p(X, Y)`), `p(a` (finding `p(a, Y)`), and `p(a, b` (finding
   `p(a, b)`).
3. For each node in the probed bucket(s), decode the clause offset with
   `rotp(g, node[CLTERM_HASHNODE_CL_POS])` and call `wr_unify_term(g, xatom, yatom, 1)`.
4. On success, build the resolvent through `wr_process_resolve_result()`.
5. Clear the varstack before the next candidate.

The relevant helpers include:

```c
wr_cl_store_res_terms(g, cl, resolvability);
wr_atom_calc_prefhashes(g, atom, prefhashes);
wr_term_hashstore(g, hashdata, term, cl);
wr_term_complexhash(g, hasharr, hashposbits, term);
wr_atom_funhash(g, atom); /* functor-only helper; not the default active-res scan */
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
reflexive, simplify, propagate, propinst, instgen, external prop, and
ARITHINST.

Input histories are built with `wr_build_input_history()`. Derived histories
are built by operation-specific helpers such as:

```c
wr_build_resolve_history(g, ...);
wr_build_factorial_history(g, ...);
wr_build_para_history(g, ...);
wr_build_simplify_history(g, ...);
wr_build_arithinst_history(g, parent, subst_count, variables, values, cuts);
```

ARITHINST records use `HISTORY_ARITH_PARENT_POS`, store the substitution count
and `(variable number, encoded numeric value)` pairs, then a cut count and cut
parents. They are not the two-parent Inst-Gen layout. Consumers and printers
must dispatch on `WR_HISTORY_TAG_ARITHINST` before interpreting tag-specific
positions.

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
17. Compare a symbol or constant by its `gint`. Do not decode it, and do not
    hash it where the `gint` serves (section 1).
18. Do not use a record offset as a key: records are not shared (section 8).
19. Test a full integer with the two-bit mask, not with three low bits.
20. Before relying on a fact about an encoding (which kind the parser makes,
    whether a kind is unique), check it by a run (section 1).
