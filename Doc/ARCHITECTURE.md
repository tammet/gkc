# GKC Architecture

This document is a developer-facing architecture reference for `gkc`, the
classical first-order prover in this checkout. It is written for maintainers and
LLM agents that need to understand the code path before changing parser,
reasoner, representation, or shared-memory behavior.

Read this together with:

* `README.md` for user-level build, syntax, options, and strategy overview.
* `Examples/README.md` for tutorial examples and expected proof behavior.
* `Doc/DEVELOPMENT_GUIDE.md` for practical change guidance and review
  checklists.
* `Doc/DATA_REPRESENTATION.md` for encoded values, records, clauses, terms,
  vectors, varbanks, and representation invariants.
* `Doc/TERM_MATCHING.md` for equality, matching, unification, varstack, and
  subsumption guidance.
* `Doc/SHARED_MEMORY.md` for the shared-memory KB model and read-only KB
  integrity rules.
* `Doc/json_ld_logic.md` for the JSON-LD-LOGIC background used by JSON input.

## 1. Source Tree and File Map

GKC builds one prover executable from four main source areas. Generated object
files such as `*.o` and parser reports such as `*.output` are build artifacts,
not architecture boundaries.

### `Main/`

`Main/gkc.c` is the normal executable entry point. It parses command-line
options, chooses the command mode, creates or attaches WhiteDB segments, imports
input, stores run options in the database header, and calls the converter or
reasoner.

Important command paths in `gkc.c`:

| Path | Purpose |
| --- | --- |
| `main()` / `gkc_main()` | Top-level process control. |
| `parse_cmdline()` | Decodes command, memory size, format flags, strategy input, time limits, parallelism, and input sources. |
| `-prove` | Creates a local database, imports input files/strings, then calls `wg_run_reasoner()`. |
| `-readkb` / `-loadkb` / `-readwritekb` | Builds or loads a shared KB segment and initializes shared indexes. |
| `-usekb` | Attaches an existing shared KB and creates a local child DB for the query. |
| `-convert` / `-clausify` | Parses/converts input without running the proof search. |

Other `Main/` files are support utilities or tests:

| File | Purpose |
| --- | --- |
| `Main/wgdb.c` | WhiteDB command-line utility with some reasoner integration. |
| `Main/indextool.c` | Database/index inspection utility. |
| `Main/selftest.c`, `Main/stresstest.c` | Local test/stress programs. |
| `Main/gendata.c` | Data generation utility. |
| `Main/Makefile.am` | Automake build description for this directory. |

### `Parser/` and `Builtparser/`

`Parser/` contains parser and clausifier sources. `Builtparser/` mirrors the
checked-in generated parser sources used by the simple build scripts, so the
normal build does not need flex/bison installed.

| File or group | Purpose |
| --- | --- |
| `dbparse.c`, `dbparse.h` | Top-level import functions for files, strings, TPTP/Otter-like syntax, include handling, parser errors, and conversion into DB records. |
| `dbotter.l`, `dbotter.y`, `dbotterparse.h` | Lexer/parser source for the Otter-like/TPTP-oriented text syntax. |
| `dbprolog.l`, `dbprolog.y`, `dbprologparse.h` | Lexer/parser source for the Prolog-like syntax. |
| `dbotter.yy.c`, `dbotter.tab.c`, `dbotter.tab.h`, `dbprolog.yy.c`, `dbprolog.tab.c`, `dbprolog.tab.h` | Generated lexer/parser files. Keep `Parser/` and `Builtparser/` copies in sync when regenerating. |
| `dbclausify.c`, `dbclausify.h` | FOF preprocessing and clausification: negation push, miniscope, skolemization, distribution, definition predicates, and flattening. |
| `jsparse.c`, `jsparse.h` | JSON/JSON-LD-LOGIC input handling. |
| `dbgenparse.h` | Shared parser data structures and helper declarations. |
| `Makefile.am` | Automake build description for parser sources. |

The parser creates WhiteDB records directly. FOF input is converted into clause
records before proof search. CNF-like input can already be clause-shaped but
still passes through parser/import normalization.

### `Reasoner/`

`Reasoner/` contains the proof search and all clause-level operations.

| File or group | Purpose |
| --- | --- |
| `rmain.c`, `rmain.h` | `wg_run_reasoner()`: parse/build strategy guide, analyze input, initialize each run, call `wr_genloop()`, print results, free run state. |
| `rgenloop.c`, `rgenloop.h` | Main given-clause search loop and given-clause rebuilding. |
| `glb.c`, `glb.h` | `glb` state object, scalar defaults, queue/index/buffer fields, strategy fields, stats, and print flags. |
| `init.c`, `init.h` | Shared KB initialization and shared reasoner structures. |
| `analyze.c`, `analyze.h` | Input statistics, role counts, equality counts, symbol/SINE analysis, and guide input for automatic strategies. |
| `guide.c`, `guide.h`, `makeguide.c`, `makeguide.h` | Strategy JSON parsing and automatic strategy construction. |
| `strat.c`, `strat.h` | Strategy-dependent clause/literal preference, resolvability, hardness, SINE, and queue heuristics. |
| `clterm.c`, `clterm.h` | Clause, atom, and term record allocation/access helpers. The macros here are authoritative for record layout. |
| `clmeta.c`, `clmeta.h` | Clause/term metadata: length, depth, size, groundness, hashes, literal sorting, and meta blocks. |
| `clstore.c`, `clstore.h` | Passive queues, active lists, active indexes, unit/double hashes, paramodulation indexes, and active storage. |
| `unify.c`, `unify.h` | Term equality, one-way matching, full unification, variable banks, and varstack cleanup. |
| `subsume.c`, `subsume.h` | Forward/backward subsumption and active-subsumption candidate checks. |
| `resolve.c`, `resolve.h` | Binary resolution, factoring, equality reflexivity, and paramodulation candidate scanning. |
| `derive.c`, `derive.h` | Builds and filters derived clauses after resolution/factoring/paramodulation/instantiation. |
| `build.c`, `build.h` | Recursive term/clause builder, substitution, variable renaming, computable functions, rewrite application, and term replacement. |
| `arithinst.c`, `arithinst.h` | Bounded arithmetic/comparison frontier scan, numeric candidate ranking, calculator-gated probes, duplicate/depth limits, and ARITHINST result storage. |
| `simp.c`, `simp.h` | Given-clause simplification pipeline. |
| `rewrite.c`, `rewrite.h` | Rewrite rule storage/application and demodulation support. |
| `prop.c`, `prop.h`, `propagate.c`, `propagate.h` | Propositional propagation and related clause generation. |
| `history.c`, `history.h` | Input/derived history records, proof output, answer registration, and answer comparison. |
| `hash.c`, `hash.h` | Term/atom hashing, preference hashes, dual string hashes, and hash-node helpers. |
| `chains.c`, `chains.h` | Chain-related helper logic used by the reasoner. |
| `mem.c`, `mem.h` | Reasoner allocation wrappers and vector/buffer helpers. |
| `prob.c`, `prob.h` | Probability/weight helper code used by strategies. |
| `printerrutils.c`, `printerrutils.h` | Printing and error utility functions. |
| `rincludes.h`, `types.h` | Common reasoner include hub and basic type aliases. |
| `Makefile.am` | Automake build description for the reasoner. |

### `Db/`

`Db/` is the modified WhiteDB storage engine used by GKC as an encoded typed
record store and as the backing store for shared-memory KBs.

| File or group | Purpose |
| --- | --- |
| `dballoc.c`, `dballoc.h` | Segment header, allocation areas, record/long-string/list allocation, KB pointer registration, and allocation error state. |
| `dbmem.c`, `dbmem.h` | Local database allocation, shared-memory attach/create/detach, dump/load, and read-only KB attach behavior. |
| `dbdata.c`, `dbdata.h`, `dbapi.h` | Encoded value API, records, fields, strings/URIs, numbers, offset conversion, and public DB API declarations. |
| `dbhash.c`, `dbhash.h` | String hash tables and hash-chain lookup. |
| `dbindex.c`, `dbindex.h`, `indexapi.h` | General WhiteDB indexes. Reasoner-specific active indexes are in `Reasoner/clstore.c`. |
| `dbquery.c`, `dbquery.h` | Low-level DB query API and temporary query parameter encodings. |
| `dbcompare.c`, `dbcompare.h` | Encoded value and record comparison. |
| `dbmpool.c`, `dbmpool.h` | Memory-pool/list helpers used heavily by parsing and clausification. |
| `dbdump.c`, `dbdump.h`, `dblog.c`, `dblog.h` | Dump/load and logging support. |
| `dbjson.c`, `dbjson.h`, `dbschema.c`, `dbschema.h`, `dbutil.c`, `dbutil.h`, `rdfapi.h` | JSON/schema/RDF utility layers inherited from WhiteDB and used by import/export utilities. |
| `dblock.c`, `dblock.h` | Locking support. |
| `dbfeatures.h` | Feature macros. |
| `Makefile.am` | Automake build description for the DB layer. |

## 2. Process Overview

A normal proof run is a staged pipeline. The important point is that parsing,
analysis, strategy setup, active/passive initialization, and the given-clause
loop are separate phases.

### Stage 1: Command and Database Setup

`Main/gkc.c` `gkc_main()` handles command setup:

1. Parse command-line options with `parse_cmdline()`.
2. Choose output/input format flags: ordinary text, TPTP, JSON, conversion, or
   clausification.
3. Choose memory size and segment number.
4. For `-prove`, create one local WhiteDB segment with `wg_attach_local_database()`.
5. For shared-KB commands, create/load/attach the shared segment and, in
   `-usekb`, create a local child DB attached to the shared KB.
6. Store run options in the database header: parallelism, print flags, time
   limits, output mode, conversion/clausification flags, and start time.

After this phase, the database exists but the proof search has not started.

### Stage 2: Parsing and Clausification

`gkc_main()` imports every input source before calling the reasoner:

1. `wg_import_data_file()` imports each logic file.
2. `wg_import_data_string()` imports each command-line logic string.
3. JSON strings use the JSON parser path; text input uses the Otter/TPTP parser
   path.
4. Parser functions in `Parser/dbparse.c` build formula or clause records in
   the database.
5. FOF input goes through `Parser/dbclausify.c`: negation push, miniscope,
   skolemization, distribution, definition handling, and flattening into
   clauses.
6. Input histories are attached so proof output can identify original clauses
   and derivation parents.

If `-convert` or `-clausify` is active, the command can stop after parsing and
conversion. Otherwise `gkc_main()` calls `wg_run_reasoner()`.

#### Clausification Internals

The text parser first builds formulas as list trees in a parser memory pool.
`wr_preprocess_clauselist()` in `Parser/dbparse.c` classifies each top-level
item as an include, TPTP `cnf`, TPTP `fof`, or unwrapped Otter-style formula.
Includes recursively import another file. A `cnf` item keeps its clause body;
FOF and unwrapped formulas call `wr_clausify_formula()`.

`wr_clausify_formula()` performs these transformations in order:

1. Discover free variable occurrences with `wr_parse_freeoccs()`. Free
   variables are wrapped in an implicit universal quantifier before the main
   transformations.
2. `wr_clausify_negpush()` removes implication, equivalence, and XOR forms,
   applies De Morgan and quantifier duality, and leaves negation only at atoms.
3. `wr_clausify_miniscope()` pushes quantifiers inward where the relevant
   variables occur and removes quantified variables that do not occur in a
   branch. This can reduce the arguments needed by later Skolem functions.
4. `wr_clausify_skolemize()` removes all quantifiers. Universally quantified
   variables receive fresh variable names. Each existential variable becomes
   a fresh Skolem constant or a Skolem function of the universal variables in
   its outer scope. The database header's `parse_skolem_nr` supplies unique
   suffixes.
5. `wr_clausify_distribute()` distributes disjunction over conjunction to form
   CNF. When `parse_newpred_strat` is enabled, a sufficiently complex case with
   conjunctions on both sides can use `wr_clausify_makedef()` instead of full
   expansion. The new predicate contains the subformula's collected variables;
   clauses for both directions of the definition are accumulated and conjoined
   with the main result. `parse_newpred_nr` supplies unique suffixes.

The distribution threshold is `MAKEDEF_COMPLEXITY_THRESHOLD` in
`Parser/dbclausify.c`; it is an implementation heuristic, not a syntax or
strategy-file setting. The transformations allocate replacement list nodes in
the parser pool, while parts of Skolem substitution update term occurrences in
place. Callers must therefore treat the parser tree as temporary mutable data.

After preprocessing, `wr_parse_clauselist()` calls
`wr_flatten_logclause()`. Top-level conjunction becomes a list of clauses and
nested disjunction becomes each clause's literal list; logical true/false terms
are simplified while flattening. `wr_parse_clause()` then creates the persistent
WhiteDB representation: a one-literal ground atom may remain a fact record,
whereas other clauses use rule-clause and atom records. It converts variables,
sets literal polarity metadata, maps TPTP roles to goal, assumption, external
axiom, or axiom role numbers, builds an input history, and links the record into
the database clause list.

When source/skolem-step storage is compiled and enabled, FOF source formulas
and the post-Skolem formula are retained for conversion-proof printing. These
records explain clausification in proof output; they are separate from the
clause histories used by proof search. JSON input has its own front end in
`Parser/jsparse.c`, but converges on the same flattening and clause-record
construction model.

Both front ends enforce structural and numeric safety before persistent records
reach proof search. Parser nesting is bounded by `PARSE_NESTING_DEPTH`; numeric
conversion checks `errno`, range, and complete-token consumption; and malformed
list/object structure propagates a parse failure instead of returning a partial
formula. The checked-in `Builtparser/` lexer/parser copies must preserve the
same checks as `Parser/`, because `compile.sh` deliberately builds without
regenerating them.

### Stage 3: Reasoner Entry and Input Analysis

`Reasoner/rmain.c` `wg_run_reasoner()` is the top-level reasoner driver.

Its first job is to normalize the database view:

* no shared KB: `db`, `child_db`, `local_db`, and `kb_db` all refer to the same
  segment;
* `-usekb`: `db` is reset to the shared KB base, `child_db` and `local_db` are
  the writable local query segment, and `kb_g` is a private malloc copy of the
  shared KB's stored `glb`.

Then `wg_run_reasoner()` creates a lightweight `analyze_g`, initializes its
varbanks/varstack, and calls `wr_analyze_clause_list(analyze_g, db, child_db)`.
Analysis counts input roles, equality presence, goals, symbols, SINE data, and
other statistics used by strategy selection. It also records whether local or
attached-KB input contains recognized non-ground arithmetic; that flag gates
the numeric-instantiation pass before any selected-clause scan occurs.

### Stage 4: Strategy Guide Construction

The strategy guide controls how many runs are attempted and how each run is
configured.

1. If a strategy file or strategy string was supplied, `guide.c` parses it.
2. If no guide was supplied, `make_auto_guide()` builds one from `analyze_g` and
   any shared KB statistics.
3. For each guide section, `wr_parse_guide_section()` writes strategy fields
   into a fresh per-run `glb`.
4. Parallel mode may fork multiple strategy runs. On Windows and Emscripten,
   parallelism is disabled.

The guide affects queue selection, equality, hyperresolution, SINE, limits,
query focus, rewriting, literal preference, arithmetic-instantiation mode and
bounds, and output behavior. Conservative arithmetic mode 1 is the scalar
default unless a guide explicitly selects mode 0 or 2.

### Stage 5: Per-Run Initialization

For each strategy run, `wg_run_reasoner()`:

1. Allocates a fresh `glb` with `wr_glb_new_simple(db)`.
2. Copies print/time/output settings from the database header.
3. Copies analysis statistics from `analyze_g`.
4. Installs `kb_g`, `child_db`, and `local_db` when a shared KB is attached.
5. Validates mode-derived arithmetic bounds with `wr_arithinst_configure()`.
6. Initializes shared-style structures with `wr_glb_init_shared_complex()` and
   local run structures with `wr_glb_init_local_complex()`.
7. Resets run-local arithmetic counters/candidate state with
   `wr_arithinst_reset_run()`; the duplicate cache remains lazy until an actual
   probe path.
8. Disables equality if the analyzed input has no positive or negative equality
   clauses.
9. Initializes active/passive lists with `wr_init_active_passive_lists_from_one()`.

In an ordinary one-segment run, initialization reads the single database clause
list. In a shared-KB run, initialization may read the shared KB clause list and
the local query clause list separately. Query-focus strategies can use the
prebuilt KB indexes through `r_kb_g(g)` instead of re-indexing every KB clause
locally.

### Stage 6: Main Search Loop

`wr_genloop(g)` runs one strategy's given-clause search. It returns:

| Return | Meaning |
| --- | --- |
| `0` | Proof or enough answers found. |
| `1` | Search finished without proof, or no more useful passive clauses. |
| `2` | Timeout/termination without proof, or no candidate existed at the start of the run. |
| Negative | Allocation or internal error. |

After each run, `wg_run_reasoner()` prints the result with `wr_show_result()` if
appropriate, frees the run `glb`, and either exits on proof or tries the next
strategy section.

## 3. Given-Clause Loop

`Reasoner/rgenloop.c` `wr_genloop()` implements the active/passive saturation
loop. Its inputs are the passive queues, active indexes, strategy flags, and
representation buffers already installed in `glb`.

The loop proceeds in this order:

1. Start run timers and clear all variable banks.
2. Initialize given-candidate and used-given statistics.
3. Repeatedly check run time, total time, allocation status, and endgame mode.
4. Pick a candidate from `hyper_queue` first; if that is empty, pick from the
   clause-pick queues.
5. Reject candidates that exceed argument-length limits or endgame limits.
6. Reject tautologies, back-subsumed clauses, and propagation-blocked clauses.
7. Set up given-clause substitution/build state.
8. Sort the candidate in place only if it is not inside the attached KB.
9. Simplify the candidate with `wr_simplify_cl()`.
10. Recalculate metadata only for non-KB-resident clauses.
11. Check propositional true/false status and immediate proof cases.
12. Run forward subsumption with `wr_given_cl_subsumed()` unless endgame mode
    disables it.
13. If the arithmetic gates and budgets allow it, call
    `wr_generate_arith_instances()` on the simplified selected clause. Useful
    instances enter the ordinary derived/answer lifecycle; the parent remains
    available.
14. Rebuild the normalized given clause with `wr_process_given_cl()`.
15. Stamp given-history order when history ordering is enabled.
16. Optionally perform backward subsumption.
17. Calculate literal resolvability with `wr_calc_clause_resolvability()`.
18. Rebuild and add the given clause to active indexes with
    `wr_add_given_cl_active_list()`, unless the candidate came from the hyper
    queue and needs the partial-hyper active path.
19. Factor the given clause.
20. Resolve reflexive equality atoms when equality is enabled.
21. Resolve the given clause against active clauses.
22. Paramodulate from and into active clauses when equality is enabled and the
    strategy allows it.
23. Stop when enough answers are found, allocation fails, timeout triggers, or
    no passive clauses remain.

The loop uses two representations of the selected clause:

| Name | Meaning |
| --- | --- |
| `given_cl_cand` | Simplified candidate selected from passive structures. |
| `given_cl` | Normalized, rebuilt clause used as the parent for inferences. |
| `given_cl_as_active` | Rebuilt active copy stored in active indexes and often used in histories. |

Candidate selection is strategy-dependent. `hyper_queue` supports partial
hyperresolution work. Ordinary clause-pick queues use priority and FIFO-like
structures maintained by `clstore.c` and strategy weights from `strat.c`.

Endgame mode is time-ratio driven: as the run approaches its time limit,
`wr_genloop()` tightens resolution argument-length limits and eventually biases
toward unit-style work.

## 4. Resolution, Factoring, and Paramodulation

`Reasoner/resolve.c` scans inference candidates. It does not build final
clauses itself; successful inference attempts call `derive.c`.

### Binary Resolution

`wr_resolve_binary_all_active(g, given_cl, given_cl_as_active, resolvability)`
scans literals of the given clause:

1. Skip literals marked non-resolvable by the `resolvability` vector.
2. Determine literal polarity.
3. Build one or more atom preference hashes with `wr_atom_calc_prefhashes()`.
4. Select the opposite-polarity active atom index:
   `hash_pos_atoms` for a negative given literal, `hash_neg_atoms` for a
   positive given literal.
5. Scan local active hash buckets.
6. If a shared KB is attached, also scan the corresponding read-only KB active
   hash buckets through `r_kb_g(g)`.
7. For each candidate active atom, enforce argument-length and strategy limits.
8. Call `wr_unify_term(g, xatom, yatom, 1)`.
9. On success, call `wr_process_resolve_result()`.
10. Clear the varstack before the next candidate.

The `uniquestrflag` argument is `1` in normal resolution, so interned strings
and URIs compare by encoded identity. This is why shared/local string encoding
rules are architecture-critical.

### Factoring

`wr_factor(g, given_cl, given_cl_as_active)` scans pairs of same-polarity
literals inside the given clause. It skips unit clauses, unifies same-polarity
literal atoms, and calls `wr_process_factor_result()` for successful pairs. In
query-focus mode it only factors around the first literal.

### Equality Reflexivity and Paramodulation

When `g->use_equality` is true, `wr_genloop()` also calls:

* `wr_resolve_equality_reflexive()` for `X = X`-style simplification/proof
  opportunities;
* `wr_paramodulate_from_all_active()` to use positive equalities in the given
  clause as rewrite sources into active clauses;
* `wr_paramodulate_into_all_active()` to rewrite terms in the given clause with
  positive equalities from active clauses.

Paramodulation uses term paths encoded by `wr_encode_para_termpath()` and
decoded by `wr_decode_para_termpath_litnr()` /
`wr_decode_para_termpath_pos()`. Replacement terms are built through
`wr_build_calc_term_replace()` so the builder can preserve the correct
substitution, renaming, and offset-base behavior.

## 5. New Clause Construction

`Reasoner/derive.c` constructs new clauses after resolution, factoring,
paramodulation, and instantiation generation.

For binary resolution, `wr_process_resolve_result()` follows this pipeline:

1. Compute parent clause sizes. If both parents are units and the cut literals
   resolve, register an empty-clause proof.
2. Reset `derived_termbuf` and allocate a half-built literal array:
   `[meta, atom, meta, atom, ...]`.
3. Configure substitution/renaming for the first build phase.
4. Copy all non-cut literals from both parents via
   `wr_process_resolve_result_aux()`, using `wr_build_calc_term()` for atoms.
   This substitutes current unification bindings, renames variables into the
   derived bank, evaluates computable functions, and can detect tautologies or
   duplicates.
5. Run cut/subsumption checks on the half-built clause:
   `wr_derived_cl_cut_and_subsume()` and optionally
   `wr_derived_cl_strong_cut()`.
6. Remove cut literals. If none remain, register a proof.
7. Decide whether the result should be a fact clause or rule clause with
   `wr_process_resolve_result_isrulecl()`.
8. Configure the final build buffer:
   `wr_process_resolve_result_setupquecopy()` for ordinary clauses or
   `wr_process_resolve_result_setuphypercopy()` for partial hyperresolution.
9. Build history with `wr_build_resolve_history()`.
10. Build the final record with `wr_derived_build_cl_from_initial_cl()`.
11. If the clause contains only `$ans` literals, register an answer.
12. Simplify with double-cut logic.
13. For ordinary results, calculate weight, enforce limits, calculate metadata,
    add unit/double hashes, propagate if enabled, store strong unit cutoff
    indexes, and push into clause-pick queues.

`Reasoner/build.c` `wr_build_calc_term()` is the central recursive builder. It:

* substitutes currently bound variables when `g->build_subst` is set;
* renames variables when `g->build_rename` is set;
* copies records into `g->build_buffer` or `g->local_db`;
* preserves the reserved term header area;
* evaluates arithmetic/list/string/equality functions when enabled;
* rewrites terms when `g->build_rewrite` is set.

When extending clause construction, do not hand-copy nested terms unless you
need very specific behavior. Configure the build flags and use
`wr_build_calc_term()` or `wr_build_calc_term_replace()`.

## 6. Data Representation Boundary

The detailed representation rules for `gint` encodings, offset converters,
WhiteDB records, clauses, atoms, terms, literal metadata, `vec`/`cvec`,
varbanks, varstack, and shared-memory allocation discipline are maintained in
`Doc/DATA_REPRESENTATION.md`.

This architecture document assumes those invariants. The most important one for
architecture work is the database split in `-usekb` runs: `g->db` is the offset
base and may point at a read-only shared KB, while `g->local_db` is the writable
segment for run-time records, histories, answers, and computed values.

## 7. WhiteDB Segment Model

A WhiteDB memory segment begins with `db_memsegment_header` in `Db/dballoc.h`,
followed by allocation areas for records, strings/URIs, words, doubles,
listcells, reasoner objects, indexes, and bookkeeping.

In ordinary no-KB proving there is one database segment:

* `g->db`, `g->local_db`, and `g->child_db` all identify the same segment.
* Parsing, input clauses, derived clauses, strings, histories, and indexes can
  all be allocated there.

In a shared-memory KB query there are two segments:

* The shared KB segment contains parsed KB clauses, string/URI intern tables,
  histories, and prebuilt reasoner indexes.
* The local child segment contains query clauses, derived clauses, answers, and
  all run-time allocations.

`wg_run_reasoner()` sets the important pointers this way:

| Field or variable | Meaning during `-usekb` |
| --- | --- |
| `db` local variable after setup | Shared KB base. |
| `local_db` / `child_db` | Writable local query segment. |
| `kb_db` | Shared KB segment. |
| `rglb` | `glb` stored inside the shared KB at KB build time. Treat as read-only. |
| `kb_g` | Private malloc copy of `rglb`, with `kb_g->db` fixed for this process. |
| `g->db` | Offset base for this run, usually the shared KB when attached. |
| `g->local_db` | Segment for new run-time objects. |
| `g->kb_g` | Private copy used to read KB indexes. |

Current gkc has the shared-memory integrity rules described in
`Doc/SHARED_MEMORY.md`: existing KB segments are attached read-only on Unix
(`SHM_RDONLY`), `wg_attached_kb_segment` records the attached range, and
`wg_in_attached_kb(ptr)` guards in-place clause/history writes. New records and
computed numbers should be allocated through `g->local_db`, not `g->db`.

Rule of thumb:

* Decode and compare against `g->db`.
* Allocate new records, histories, and computed values in `g->local_db`.
* Never sort, rewrite metadata, stamp history order, refcount, or cache into an
  object where `wg_in_attached_kb(ptr)` is true.

## 8. `glb`: The Reasoner State

`glb` is defined in `Reasoner/glb.h` and initialized in `Reasoner/glb.c`. It is
the central state object for one reasoner run.

The most important groups of fields are:

* Database pointers: `db`, `kb_db`, `child_db`, `local_db`, `kb_g`, `inkb`.
* Clause queues and active sets: `clqueue`, `clpickstack`, `clpick_queues`,
  `clpickpriorqueue`, `clactive`, `clactivesubsume`.
* Hash indexes: atom indexes for resolution, ground-unit/double indexes for
  cut/subsumption, paramodulation and rewrite indexes, occurrence indexes for
  propagation.
* Variable state: `varbanks`, `varstack`, and temporary unification fields.
* Build buffers: `given_termbuf`, `derived_termbuf`, `queue_termbuf`,
  `active_termbuf`, `hyper_termbuf`, `simplified_termbuf`.
* Strategy fields: flags and limits parsed from guide/strategy JSON.
* Arithmetic-instantiation mode/bounds, observed candidates, lazy duplicate
  cache, probe temporaries, and counters.
* Statistics, print flags, proof state, and answer storage.

`wr_glb_init_simple()` sets scalar defaults. Notable defaults include:

* `unify_firstuseterm = 1`: term/atom field 0 is reserved and skipped by
  unification.
* `unify_funpos = 1`, `unify_funarg1pos = 2`, `unify_funarg2pos = 3`.
* `local_db = db` until `rmain.c` overrides it in the shared-KB case.
* `arith_instantiation = 1`: conservative one-variable numeric instantiation.

`wr_glb_init_shared_complex()` allocates queue/hash structures that may be
stored in a KB during `-readkb`. `wr_glb_init_local_complex()` allocates
per-run vectors, buffers, varbanks, stacks, and temporary structures.

## 9. Data Structures Used by Search

The search algorithm is built on the structures described in
`Doc/DATA_REPRESENTATION.md`:

* clause queues and indexes are `vec`/`cvec`-based offset structures;
* fact clauses are atom records, while rule clauses contain literal meta/atom
  pairs;
* literal polarity is in rule-clause metadata;
* matching and unification write into `varbanks` and must restore `varstack`;
* clause construction goes through `wr_build_calc_term()` and the
  `given_termbuf`, `derived_termbuf`, `queue_termbuf`, `active_termbuf`, and
  `hyper_termbuf` buffers.

Architecture changes should avoid duplicating those layouts here. Update the
data representation reference when changing representation invariants.

## 10. Indexes and Active Clause Storage

`Reasoner/clstore.c` owns the clause queues and term indexes used by the search
loop. Important index families:

* `hash_neg_atoms`, `hash_pos_atoms`: active literal indexes for binary
  resolution.
* `hash_neg_units`, `hash_pos_units`: unit active clauses.
* `hash_neg_groundunits`, `hash_pos_groundunits`: ground unit clauses for
  cutting and subsumption.
* `hash_neg_grounddoubles`, `hash_pos_grounddoubles`: ground two-literal
  clauses for double cuts.
* `hash_para_terms`, `hash_eq_terms`, `hash_rewrite_terms`: equality and
  rewrite indexes.
* `clactivesubsume`: compact active clause meta blocks used before full
  subsumption.

When a clause becomes active, `wr_add_given_cl_active_list()` rebuilds it into
the active term buffer, pushes it into `clactive`, possibly into
`clactivesubsume`, stores unit hashes, stores resolution terms, and stores
paramodulation terms when equality is enabled.

With a shared KB, candidate loops frequently try local indexes first and then
the read-only KB indexes through `r_kb_g(g)`.

## 11. Simplification, Cutoff, and Subsumption

Simplification is spread across `Reasoner/simp.c`, `Reasoner/rewrite.c`,
`Reasoner/propagate.c`, and `Reasoner/subsume.c`.

The subsumption relation is:

```text
cl1 subsumes cl2 iff every literal in cl1 can be matched to a distinct
same-polarity literal in cl2 under one shared substitution for cl1 variables.
```

`wr_given_cl_subsumed()` first tries cheap ground-unit checks, then scans
`clactivesubsume` meta blocks locally and in `r_kb_g(g)`, then calls
`wr_subsume_cl()` for candidates.

`wr_subsume_cl()` handles fact/rule special cases and then calls
`wr_subsume_cl_aux()` for recursive multi-literal matching. The reusable
pattern is:

```c
g->tmp_unify_vc = (g->varstack) + 1;
int vc_tmp = *(g->tmp_unify_vc);
if (wr_match_term_aux(g, general_lit, specific_lit, 1)) {
  /* recurse or consume the substitution */
}
if (vc_tmp != *(g->tmp_unify_vc)) {
  wr_clear_varstack_topslice(g, g->varstack, vc_tmp);
}
```

Do not replace this with full unification unless the mathematical relation you
need is actually symmetric unifiability. See `Doc/TERM_MATCHING.md` for the
full equality/matching/unification distinction.

## 12. Histories and Answers

Clause history records are defined in `Reasoner/history.h`. With
`RECORD_HISTORY_ORDER` enabled, the prefix fields are:

| Field | Meaning |
| --- | --- |
| `HISTORY_PRIORITY_POS` | Role/priority encoding. |
| `HISTORY_NAME_POS` | Clause name. |
| `HISTORY_CLID_POS` | Clause id. |
| `HISTORY_DERIVED_ORDER_POS` | Derivation order stamp. |
| `HISTORY_GIVEN_ORDER_POS` | Given-clause order stamp. |

Then comes `HISTORY_DERIVATION_TAG_POS` and tag-specific parent/cut/path
fields. Tags include resolve, factorial, para, equality reflexive, simplify,
propagate, propinst, instgen, external prop, and ARITHINST.

Input histories are built with `wr_build_input_history()`. Derived histories
are built by operation-specific helpers such as:

* `wr_build_resolve_history()`
* `wr_build_factorial_history()`
* `wr_build_para_history()`
* `wr_build_simplify_history()`
* `wr_build_arithinst_history()`

ARITHINST history is a specialized one-parent record containing the parent,
the numeric substitution pairs, and any cut clauses. Proof flattening follows
both the parent and cuts; plain and TPTP output identify the inference.

`wr_register_answer()` stores answer/proof pairs in `g->answers`. Answer
literals are recognized by `wr_answer_lit()` looking for the `$ans` predicate.
Pure answer clauses cause `proof_found` to be set without being queued as
ordinary clauses.

When a shared KB is attached, history order setters return immediately for
KB-resident history records. Do not add new code that stamps or rewrites a KB
history record.

## 13. Strategy and Queue Semantics

Strategy JSON is parsed in `Reasoner/guide.c` and stored in `glb` fields. The
user-level fields are summarized in `README.md`.

Important strategy effects in gkc:

* `query_focus`: uses role-aware initial activation and set-of-support style
  search.
* `negative_pref`, `positive_pref`, `hardness_pref`, `knuthbendix_pref`: affect
  literal and clause preference.
* `hyper`: enables hyperresolution behavior and the `hyper_queue`.
* `unit`, `double`, `triple`: restrict resolution argument lengths.
* `equality`, `rewrite`, `posunitpara`, `prohibit_nested_para`: control
  equality reasoning.
* `sine`: relevance filtering and ordering for large clause sets.
* `arith_instantiation`: mode 0 disables, default mode 1 performs bounded
  one-variable probes, and mode 2 permits bounded two-variable/deeper probes.
* weight, depth, size, and length limits: control whether derived clauses are
  kept.

Clause roles are represented in history priorities and affect queue placement.
Derived clauses inherit role-like origin information from their parents, such
as from axiom, from assumption, from goal, or mixed goal/assumption.

## 14. High-Risk Invariants

Keep these invariants visible during code review:

1. Fact clauses are ground positive units and are accessed as atom records.
2. Rule clauses store literal polarity in meta, not in atom terms.
3. Atom/term field 0 is reserved. Normal matching starts at field 1.
4. `decode_var(x)` indexes the full varbank array, not a per-clause variable
   number.
5. Varbanks must be cleared before a new independent matching context.
6. Every failed or completed match/unification probe must restore the varstack.
7. `wr_match_term(g, general, instance, 1)` is directional.
8. New run-time records and computed constants go into `g->local_db`.
9. KB-resident records and histories are read-only during `-usekb`.
10. Hashes and literal metas are prefilters. They do not prove term identity.
11. `cvec` helpers can reallocate. Always use their return values.
12. Build flags in `glb` are global state for the current construction phase.
    Set them with the established setup/cleanup helpers.
13. Arithmetic probes must restore varstack/build state on every exit and may
    retain only genuine calculator progress; see
    `Doc/ARITHMETIC_INSTANTIATION.md`.
