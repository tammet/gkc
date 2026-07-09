# GKC Command-Line Reference

This file documents the command-line interface implemented by `Main/gkc.c`.
For input syntax, see `README.md`, `Examples/README.md`, and
`Doc/json_ld_logic.md`. For strategy JSON, see `Doc/strategy_reference.md`.

## Usage

```sh
gkc [options] <logic-file> [<logic-file> ...]
gkc -prove [options] <logic-file> [<logic-file> ...]
gkc -prove [options] -text '<logic text>'
gkc -prove [options] -jstext '<json logic text>'
gkc -readkb <logic-file> [<strategy-file>] [options]
gkc -usekb [options] [<query-file> ...]
gkc -writekb <dump-file> [options]
gkc -loadkb <dump-file> [options]
gkc -readwritekb <logic-file> <dump-file> [<strategy-file>] [options]
gkc -deletekb [options]
gkc -help
gkc -version
```

If no primary command is given, `-prove` is assumed.

Options may generally appear before or after filenames. Non-option arguments
are treated as input or dump filenames according to the selected command.

## Input Formats

GKC accepts:

* simple/Otter-like text syntax;
* TPTP FOF/CNF-style input;
* JSON/JSON-LD-LOGIC input.

A filename containing `.js` is imported through the JSON path. `-jstext` also
forces JSON text input. Other text normally goes through the simple/TPTP parser,
with TPTP recognized during import.

Multiple input files are imported into the same database before proof search.
`-text` and `-jstext` can be used multiple times, up to the current fixed parser
array limits in `Main/gkc.c`.

## Primary Commands

### `-prove`

Parse all input, build or read a strategy, and search for a proof or answers.
This is the default command.

```sh
./gkc Examples/example1.txt
./gkc -prove Examples/example1.txt -parallel 0
./gkc -prove -text 'p(a). ?- p(a).'
```

### `-readkb <logic-file> [<strategy-file>]`

Create a shared-memory KB from a logic file. GKC deletes any existing shared DB
with the same `-mbnr`, creates a new shared segment, imports the logic file, and
calls `init_shared_database()` to build shared reasoner indexes.

On Unix, use at least `-mbsize 1000` when specifying a size explicitly. On
Windows, the explicit minimum is 100 MB. If omitted, the platform default is
used.

The optional second non-option argument is passed to shared index
initialization as a strategy filename.

```sh
./gkc -readkb Examples/steam_kb.txt -mbsize 1000
./gkc -readkb axioms.txt kb_build_strategy.json -mbnr 1001 -mbsize 2000
```

### `-usekb [<query-file> ...]`

Attach an existing shared KB and create a writable local child database for the
query. Query files, `-text`, and `-jstext` are imported into the local child DB;
the shared KB is used as background axioms.

`-clausify` is rejected with `-usekb`. `-convert` returns after parsing the
query input. SINE is disabled for shared-KB query runs in `rmain.c`.

```sh
./gkc -usekb Examples/steam_query.txt -mbnr 1000 -parallel 0
./gkc -usekb -text '...' -strategy query_strategy.json
```

See `Doc/SHARED_MEMORY.md` for the two-segment representation model and
read-only KB rules.

### `-writekb <dump-file>`

Attach the existing shared DB selected by `-mbnr` and write it to a dump file.

```sh
./gkc -writekb kb.dump -mbnr 1000
```

### `-loadkb <dump-file>`

Load a previously dumped database file into shared memory under the selected
`-mbnr`.

```sh
./gkc -loadkb kb.dump -mbnr 1000
```

### `-readwritekb <logic-file> <dump-file> [<strategy-file>]`

Build a shared KB from a logic file and immediately dump the resulting database
to disk. The shared DB remains present in memory after the dump.

```sh
./gkc -readwritekb axioms.txt kb.dump -mbsize 1000
```

### `-deletekb`

Delete the shared DB selected by `-mbnr`. This command is available on Unix-like
systems; on Windows the current code reports that it is unavailable.

```sh
./gkc -deletekb -mbnr 1000
```

### `-help`, `--help`

Print the built-in usage text from `Main/gkc.c`.

### `-version`, `--version`

Print the compiled GKC version.

## Numeric Options

### `-seconds <n>`

Set a total wall-clock search limit in seconds. `0` means no command-line limit.
The value is stored in the local database header and copied into each run.

```sh
./gkc Examples/example1.txt -seconds 30
```

Strategy files can also set per-run `max_seconds` / `max_dseconds` and total
`total_seconds` / `total_dseconds` limits.

### `-parallel <n>`

Set the maximum number of worker processes for strategy runs. `0` disables
forking and is recommended for deterministic debugging. Negative values are
rejected. On Windows and Emscripten, GKC forces `parallel = 0`.

If omitted, `DEFAULT_PARALLEL` is used; the built-in help describes the normal
Unix default as four worker processes plus a parent.

```sh
./gkc Examples/example1.txt -parallel 0
./gkc Examples/example1.txt -parallel 4
```

### `-mbsize <megabytes>`

Set the initial database segment size in megabytes. Values below 10 are
rejected. Values above 100000 are rejected by the command-line parser.

If omitted, current defaults are:

* Unix-like 64-bit build: 5000 MB for the main/shared DB and 1000 MB for a
  local child DB used with `-usekb`;
* 64-bit Windows: 5000 MB main/shared and 1000 MB local child;
* 32-bit Windows: 1000 MB main/shared and 500 MB local child;
* Emscripten: 100 MB.

### `-mbnr <number>`

Select the shared-memory database number. If omitted, number `1000` is used.
Values below 1 are rejected.

Use different `-mbnr` values to keep several shared KBs available at once.

### `-print <level>`

Set output detail. The command-line default in `gkc_main()` is `15`; a strategy
file may override it with `print_level`.

Useful levels:

| Level | Effect |
| --- | --- |
| `0` | Suppress almost all printing. |
| `1` to `10` | Tiny/default proof output. |
| `11` | Include extra proof/history detail. |
| `12` | Show run starts and strategy text. |
| `13` to `15` | Low-detail run/statistics output; useful for seeing generated strategies. |
| `16` to `20` | Include final given clauses. |
| `21` to `30` | Include initial/final given clauses and derived clauses. |
| `31` to `50` | Detailed parser, active/passive, derived, and selection diagnostics. |
| `51+` | Detailed diagnostics plus internal data structure dumps. |

The exact flags are set in `Reasoner/rmain.c` by the `wr_set_*_printout()`
helpers.

## Strategy Options

### `-strategy <filename>`

Read strategy JSON from a file.

```sh
./gkc Examples/example1.txt -strategy Examples/strat.txt
```

### `-strategytext '<json>'`

Read strategy JSON directly from the command line.

```sh
./gkc Examples/example1.txt -strategytext '{"max_seconds":5,"strategy":["query_focus"],"query_preference":1}'
```

See `Doc/strategy_reference.md` for accepted keys. Unknown strategy settings
are warnings, not silently ignored.

## Direct Input Options

### `-text '<logic text>'`

Add simple/Otter-like logic text from the command line.

```sh
./gkc -text 'p(a). ?- p(a).'
```

### `-jstext '<json logic text>'`

Add JSON/JSON-LD-LOGIC text from the command line.

```sh
./gkc -jstext '[{"@role":"axiom","@logic":["p","a"]},{"@question":["p","a"]}]'
```

## Output and Conversion Flags

### `-tptp`

Use TPTP/CASC-style output. `-tptp` and `-json` cannot be combined.

### `-json`

Use JSON output. `-json` and `-tptp` cannot be combined.

### `-convert`

Parse and convert input, then return without proof search. Combine with `-tptp`
or `-json` to choose the output format.

```sh
./gkc Examples/example2.txt -convert -json
```

### `-clausify`

Parse input and convert it to clausal normal form instead of searching for a
proof. `-clausify` cannot be used with `-usekb`.

### `-derived`

Print all derived clauses regardless of the normal `-print` level. Use with
`-parallel 0` and a single-run strategy when debugging.

```sh
./gkc problem.txt -parallel 0 -derived -strategy one_run.json
```

## Shared-Memory Workflow

A typical KB workflow is:

```sh
./gkc -readkb Examples/steam_kb.txt -mbsize 1000 -mbnr 1000
./gkc -usekb Examples/steam_query.txt -mbnr 1000 -parallel 0
./gkc -writekb steam.dump -mbnr 1000
./gkc -deletekb -mbnr 1000
./gkc -loadkb steam.dump -mbnr 1000
```

For implementation details and hazards, read `Doc/SHARED_MEMORY.md`.
