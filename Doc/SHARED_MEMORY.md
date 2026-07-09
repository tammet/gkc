# GKC Shared-Memory Knowledge Bases

This document describes how gkc builds and queries shared-memory knowledge bases
and what invariants keep those knowledge bases read-only after they are built.
The focus here is current gkc behavior.

Read this with:

* `Main/gkc.c`: command-line setup for `-readkb`, `-loadkb`, `-usekb`, and local
  child databases.
* `Reasoner/rmain.c`: `wg_run_reasoner()` and the local/shared database split.
* `Reasoner/init.c`: `init_shared_database()` and KB index construction.
* `Db/dbmem.c`: shared-memory attach/create/detach and `SHM_RDONLY` handling.
* `Db/dballoc.h`: `wg_attached_kb_segment` and `wg_in_attached_kb()`.
* `Db/dbdata.h`: `encode_kb_offset()` and base conversion helpers.
* `Doc/DATA_REPRESENTATION.md`: encoded values, records, offsets, and `glb`.

## 1. Segment Model

GKC stores data in WhiteDB memory segments. A segment is a contiguous memory
image beginning with `db_memsegment_header`, followed by allocation areas for
records, strings/URIs, words, doubles, list cells, indexes, reasoner structures,
locks, and bookkeeping.

Internal references are offsets from a segment base, not raw persistent
pointers. This is what makes a segment dumpable, reloadable, and attachable at a
different process address.

There are two relevant segment roles:

| Segment | Created by | Writable when queried? | Contents |
| --- | --- | --- | --- |
| Shared KB | `-readkb`, `-loadkb`, `-readwritekb` | No on Unix after attach; intended read-only everywhere | Parsed KB clauses, strings/URIs, histories, KB `glb`, and prebuilt indexes. |
| Local child DB | ordinary run setup or `wg_attach_local_database_with_kb()` | Yes | Query clauses, derived clauses, answers, run-time histories, computed constants, and local indexes. |

When `-usekb` is active, `Main/gkc.c` attaches the existing shared segment and
then creates a local child DB whose header points to the shared KB:

```c
dbmemsegh(local_db)->kb_db = shared_kb;
```

The rest of the code detects the presence of a KB with `db_get_kb_db(db)`.

## 2. Offset Bases and Local Allocation

In a shared-KB query, encoded values are interpreted relative to the KB base,
even if some newly created objects physically live in the local child DB. This
works through `encode_kb_offset(db, offset)` in `Db/dbdata.h`.

If the DB handle has `kb_db` set, `encode_kb_offset()` rebases a local offset by
the distance between the local segment and the KB segment. Decoding later uses
`g->db`, which is the KB base, and reaches both KB-resident and local child
objects.

Practical rules:

* Decode, compare, hash, and print against `g->db`.
* Allocate run-time records, histories, computed integers/doubles/strings, and
  query objects through `g->local_db`.
* Treat `g->db` as the offset/read base in `-usekb` runs, not as the allocation
  target.
* If a newly encoded value is needed while querying a KB, encode through
  `g->local_db`; the returned value remains usable relative to `g->db`.

Bug pattern to avoid:

```c
/* Wrong in -usekb-capable code: may allocate in the read-only KB. */
return wg_encode_double(g->db, result);

/* Correct: allocates in the local child DB and returns a KB-relative offset. */
return wg_encode_double(g->local_db, result);
```

Current gkc follows this rule in the clause/term builders, history allocation,
and computable arithmetic/string functions.

## 3. The `glb`, `rglb`, and `kb_g` Relationship

`glb` is the reasoner state object. In shared-memory KB handling there are three
forms to distinguish:

| Name | Location | Mutability | Purpose |
| --- | --- | --- | --- |
| `rglb` | Inside the shared KB segment | Read-only after KB build | Stored KB globals and offsets to KB indexes. |
| `kb_g` | Malloc copy in the query process | Private copy | Safe base object for reading KB indexes with `r_kb_g(g)`. |
| per-run `g` | Malloc per strategy run | Writable | Query-local run state, local indexes, buffers, varbanks, counters. |

During KB build, `init_shared_database()` allocates `rglb` inside the segment,
stores its offset in `dbmemsegh(db)->rglb`, sets `g->inkb = 1`, and builds the
shared queues and indexes inside the segment.

During query, `wg_run_reasoner()` does the important pointer swap:

```c
kb_db = db_get_kb_db(db);
if (kb_db) {
    local_db = db;
    child_db = db;
    db = kb_db;
    rglb = db_rglb(db);
    kb_g = sys_malloc(sizeof(glb));
    memcpy(kb_g, rglb, sizeof(glb));
    kb_g->db = kb_db;
} else {
    local_db = child_db = kb_db = db;
    kb_g = NULL;
}
```

For each strategy run, the per-run `g` receives:

| Field | Value in `-usekb` |
| --- | --- |
| `g->db` | Shared KB base. |
| `g->local_db` | Writable local child DB. |
| `g->child_db` | Same local child DB. |
| `g->kb_g` | Private `kb_g` copy used to read KB indexes. |
| `g->inkb` | `0`; run-time reasoner allocations are process-local unless explicit DB allocation is used. |

The pointer-valued local fields copied from `rglb` are not valid in an attaching
process. Use only offset-valued shared structures from `kb_g`, with `kb_g->db`
fixed to the current attach address. Current `wg_run_reasoner()` frees `kb_g`
on normal exit paths.

## 4. KB Build Phase

A KB build run is allowed to write the shared segment because it is constructing
that segment. Important build-time steps:

1. Parse input clauses and formulas into the shared DB.
2. Allocate `rglb` in the shared DB and initialize simple globals.
3. Build shared complex structures with `wr_glb_init_shared_complex()` while
   `g->inkb == 1`.
4. Calculate clause metadata, histories, and SINE/statistics as needed.
5. Build shared indexes such as active clause lists, ground-unit hashes,
   resolution term hashes, paramodulation/rewrite indexes, and SINE data.
6. Leave `rglb` and all referenced indexes as offset-based structures.

After the build, the segment is a read-only KB image for query processes.

## 5. Query Phase

A `-usekb` query has two clause sources:

* the shared KB clause list from `dbmemsegh(g->db)->clauselist`;
* the local query clause list from `dbmemsegh(g->child_db)->clauselist`.

`wr_init_active_passive_lists_from_one()` and related initialization code handle
both sources. In query-focus modes, KB clauses may be read from prebuilt KB
indexes rather than re-indexed locally.

Candidate and subsumption loops often check two index sets:

1. local indexes from the current run `g`;
2. shared KB indexes through `r_kb_g(g)`.

The shared indexes must be read only. If code needs a derived, simplified, or
rewritten clause, it should build a local copy or write into a run-time buffer.

## 6. Read-Only Enforcement

Current gkc uses two layers of protection.

### OS-Level Mapping

On Unix, `Db/dbmem.c::link_shared_memory()` attaches existing shared segments
with:

```c
shmat(shmid, NULL, SHM_RDONLY)
```

Creation of a new shared segment remains writable. Only attachment of an
existing KB for querying is read-only. A missed write bug should therefore fail
loudly instead of silently corrupting the KB.

On Windows, the current mapping uses `FILE_MAP_ALL_ACCESS`, so the OS-level
read-only protection is not equivalent. The code-level guards still matter, but
Windows cannot currently rely on a read-only mapping to catch every write bug.

### Address-Range Guard

`Db/dbmem.c` maintains:

```c
void *wg_attached_kb_segment;
```

`link_shared_memory()` sets it when an existing segment is attached, and
`detach_shared_memory()` clears it when that segment is detached. `Db/dballoc.h`
defines:

```c
wg_in_attached_kb(ptr)
```

which tests whether a pointer is inside the currently attached KB address
range. This is the canonical guard before in-place mutation of records,
histories, strings, or indexes that may be KB-resident.

## 7. Guarded Write Sites

Current gkc has guards or local allocation in the important write classes:

| Write class | Current handling |
| --- | --- |
| Clause sorting and metadata recalculation in `rgenloop.c` | Skipped when the clause pointer is inside `wg_in_attached_kb()`. |
| Initial rule/fact clause metadata and unit-hash setup in `rmain.c` | Metadata/unit-hash writes skipped for KB-resident clauses; local doublehash may still be filled. |
| Active-clause storage in `clstore.c` | Avoids recalculating/storing metadata into KB-resident clauses. |
| History derived/given order stamps in `history.c` | Return/skip when the history record is inside the attached KB. |
| Raw clause/atom/term creation in `clterm.c` | Allocates through `g->local_db`. |
| Build-result record allocation in `build.c` | Allocates through `g->local_db` when not using a build buffer. |
| History record allocation in `history.c` | Allocates through `g->local_db`. |
| Computed integer/double/string/list values in `build.c` | Allocating encodes use `g->local_db`; `g->db` is used only as decode/base. |
| URI reasoner hash cache in `hash.c` | Lazy cache write is skipped for KB-resident URI/string objects. |
| Allocation error flags in `dballoc.c` | Error paths do not poison the attached KB header. |

These guards are not optional. Removing one can either crash under `SHM_RDONLY`
or corrupt a writable mapping/platform.

## 8. Parser Field Stores and `QUICK_REASONER`

`Reasoner/clterm.c` still has `wr_set_kb_atom_subterm()` and
`wr_set_kb_term_subterm()`, which write subterms with `wg_set_new_field()` using
`g_to_kb_db(g)`. This is safe in current reasoner builds because
`Db/dbdata.c` defines `QUICK_REASONER`:

* `wg_set_field()` returns before storing or refcounting;
* `wg_set_new_field()` stores the new field but returns before refcount/index
  maintenance;
* refcount increments on KB-resident long strings are also guarded with
  `wg_in_attached_kb()` in the non-quick path.

This is a fragile but intentional reasoner assumption. `g_to_kb_db(g)` is
appropriate for these KB-term setters because the target term structure belongs
in the KB representation. It is not a general query-time allocation helper; new
query records and computed values still belong in `g->local_db`. If
`QUICK_REASONER` is removed or those APIs are reused differently, shared-KB
refcount/index writes must be audited again.

## 9. Constant Identity Across Segments

With `uniquestrflag == 1`, reasoner equality treats interned URI/long-string
values as identical only when the encoded values match. Shared-KB queries rely
on normal `wg_encode_unistr()` / `find_create_longstr()` calls checking the KB
string hash first when `dbmemsegh(local_db)->kb_db` is set. If the symbol is
already in the KB, the encoder returns the KB encoding; otherwise it allocates a
local long string and returns a KB-relative offset.

This is separate from the `wg_encode_query_param_*()` API in `Db/dbquery.c`,
which creates malloc-backed temporary encodings for low-level DB query
parameters. Those values are not persistent KB intern records.

Rules:

* Encode new query-time constants through the local child DB.
* Do not bypass local child DB encoding with direct writes into the shared DB.
* For URIs and long strings, preserve the KB-first string-hash lookup before
  creating a local symbol.
* Do not create duplicate encodings for the same URI/string in the local child
  DB when the KB already contains that symbol.
* Non-string values such as computed doubles and full integers can safely be
  allocated locally because their encoded offsets are rebased and primitive
  equality decodes values.

The split-base design is correct only if encode/decode paths preserve these
identity rules.

## 10. Disabled and Platform-Specific Hazards

Known limitations or fragile areas in current gkc:

1. **Windows mapping is writable.** `_WIN32` uses `FILE_MAP_ALL_ACCESS`; the
   Unix `SHM_RDONLY` safety net does not apply there.
2. **`QUICK_REASONER` must stay audited.** Parser subterm stores rely on its
   current behavior to avoid refcount/index maintenance on KB strings.
3. **`GLOBAL_UNITS` must remain disabled with shared KBs.** `Reasoner/rmain.c`
   carries a comment warning that the disabled block writes into shared KB unit
   hashes through `r_kb_g(g)`.
4. **Lazy caches need range guards.** Any new URI/string metadata cache,
   history stamp, clause metadata, or index backfill must use
   `wg_in_attached_kb()` or write to local state instead.
5. **Read-only attach catches bugs only after KB creation.** The KB builder must
   remain writable; query attach must remain read-only on Unix.
6. **Test coverage is mostly example/script based.** There is no single modern
   test harness that proves every shared-KB invariant.

## 11. Development Checklist

When changing shared-KB-capable code, ask these questions:

1. Is this code running during KB build or query? Build may write the KB; query
   must not.
2. Is `g->db` being used as a decode/offset base or as an allocation target?
   Query-time allocation should use `g->local_db`.
3. Can the pointer being mutated be inside `wg_attached_kb_segment`? If yes,
   skip, copy, or redirect the write.
4. Does the code update clause metadata, literal order, histories, URI metadata,
   refcounts, indexes, or database header fields? Those are common hidden writes.
5. Does the code scan both local and KB indexes where required? Check
   `r_kb_g(g)` paths.
6. Does a new encode allocate a full integer, double, long string, URI, or blob?
   Use the local DB during query.
7. Does the change rely on `wg_set_field()` / `wg_set_new_field()` behavior?
   Recheck `QUICK_REASONER` and attached-KB range guards.
8. Does the change work with `-parallel 0` and the default forked runs?
9. On Unix, does a representative `-usekb` query still run with the KB mapped
   read-only?
10. If possible, compare a dump/hash of the shared KB before and after query;
    it should be byte-identical.

## 12. Suggested Regression Scenarios

For changes that touch shared-memory behavior, run small targeted scenarios:

1. Build a KB with facts and a rule, query it with `-usekb`, and compare answers.
2. Query a KB rule that derives a computed integer or double, exercising
   `wg_encode_int(g->local_db, ...)` and `wg_encode_double(g->local_db, ...)`.
3. Use a multi-literal KB rule that would be sorted or have metadata recalculated
   in no-KB mode.
4. Use a KB with equality/rewrite terms so `r_kb_g(g)->hash_rewrite_terms` and
   paramodulation indexes are read.
5. Use a query that introduces a new local URI/string and a query that reuses a
   URI/string already present in the KB.
6. Run with `-parallel 0` for deterministic traces, then with the default forked
   mode to catch process-local/shared-state mistakes.
7. If tooling is available, hash or dump the shared segment before and after the
   query and verify it is unchanged.

## 13. Relationship to Other Docs

* `Doc/DATA_REPRESENTATION.md` describes the offset encodings and record layouts
  that make local-child values usable against a KB base.
* `Doc/ARCHITECTURE.md` describes where shared-memory setup fits in the
  overall run flow.
* `Doc/DEVELOPMENT_GUIDE.md` gives practical rules for changing code safely.
* `Doc/TERM_MATCHING.md` explains equality/matching/unification behavior that
  depends on using the correct database base.
